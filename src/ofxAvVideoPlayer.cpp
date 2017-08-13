//
//  ofxAvVideoPlayer.cpp
//  emptyExample
//
//  Created by Hansi on 13.07.15.
//
// a huge chunk of this file is based on the
// blog entry https://blinkingblip.wordpress.com/2011/10/08/decoding-and-playing-an-audio-stream-using-libavcodec-libavformat-and-libao/
// (had to make some adjustments for changes in ffmpeg
// and libavcodecs examples
// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c

#include "ofxAvVideoPlayer.h"
#include "ofMain.h"
#include "ofxAvUtils.h"
extern "C"{
	#include <libavutil/opt.h>
}

// for release:
// #define AVL_MEASURE(x)
// for debug:
// #define AVL_MEASURE(x) x
#define AVL_MEASURE(x) 

using namespace std;

#define die(msg) { unload(); cerr << msg << endl; return false; }

// this holds an image
class ofxAvVideoData{
public:
	uint8_t *video_dst_data[4];
	int      video_dst_linesize[4];
	int video_dst_bufsize;
	bool allocated;
	int64_t pts;
	double t;
	
	
	ofxAvVideoData() : allocated(false), video_dst_bufsize(0),pts(-1),t(-1){}
};

class ofxAvAudioData{
public:
	int64_t pts;
	int64_t pts_native;
	double t;
	// contains audio data, always in interleaved float format
	int decoded_buffer_pos;
	int decoded_buffer_len;
	float decoded_buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE];
	
	ofxAvAudioData() : decoded_buffer_len(0){}
};


ofxAvVideoPlayer::ofxAvVideoPlayer(){
	ofxAvUtils::init();
	// default audio settings
	output_channel_layout = av_get_default_channel_layout(2);
	output_sample_rate = 44100;
	output_num_channels = 2;
	output_config_changed = false;
	output_setup_called = false;
	
	volume = 1;
	
	isLooping = false;
	fmt_ctx = NULL;
	decoded_frame = NULL;
	audio_context = NULL;
	video_context = NULL;
	audio_stream = NULL;
	video_stream = NULL;
	buffer_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
	swr_context = NULL;
	sws_context = NULL;
	av_init_packet(&packet);
	unload();
	
}

ofxAvVideoPlayer::~ofxAvVideoPlayer(){
	unload();
}

static int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type){
	int ret, stream_index;
	AVStream *st;
	AVCodecContext *dec_ctx = NULL;
	AVCodec *dec = NULL;
	AVDictionary *opts = NULL;
	
	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file\n", av_get_media_type_string(type));
		return ret;
	}
	else {
		stream_index = ret;
		st = fmt_ctx->streams[stream_index];
		
		/* find decoder for the stream */
		dec_ctx = st->codec;
		if( type == AVMEDIA_TYPE_VIDEO){
			av_opt_set(dec_ctx->priv_data, "tune", "zerolatency", 0);
			dec_ctx->thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME;
		}
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}
		if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
			fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
			return ret;
		}
		av_dict_set(&opts, "refcounted_frames", "1", 0);
		*stream_idx = stream_index;
	}
	
	return 0;
}


bool ofxAvVideoPlayer::load(string fileName, bool stream){
	unload();
	fileNameAbs = ofToDataPath(fileName,true);
	fileNameBase = ofFile(fileNameAbs,ofFile::Reference).getFileName();
	const char * input_filename = fileNameAbs.c_str();
	// the first finds the right codec, following  https://blinkingblip.wordpress.com/2011/10/08/decoding-and-playing-an-audio-stream-using-libavcodec-libavformat-and-libao/
	// and also demuxing_decoding.c in the ffmpeg examples
	// throughout all of it we are using "new api, reference counted" for memory management
	fmt_ctx = 0;
//	AVInputFormat fmt;
//	fmt.flags = AVFMT_SEEK_TO_PTS;
	if (avformat_open_input(&fmt_ctx, input_filename, NULL, NULL) < 0) {
		die("Could not open file");
	}
	
	fmt_ctx->iformat->flags |= AVFMT_SEEK_TO_PTS;
 
	if (avformat_find_stream_info(fmt_ctx,NULL) < 0) {
		die("Could not find file info");
	}
 

	// ----------------------------------------------
	// Find video stream
	// ----------------------------------------------
	if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
		video_stream = fmt_ctx->streams[video_stream_idx];
		video_context = video_stream->codec;

		/* allocate image where the decoded image will be put */
		// we will use the sws resampling context to fill out the data!
		width = video_context->width;
		height = video_context->height;
		pix_fmt = AV_PIX_FMT_RGB24;

		// allow skip frames only for prores, which seems to decode particularly slow
		allowSkipFrames = video_context->codec_id == AV_CODEC_ID_PRORES;

		// allocate a bunch of images!
		// we take 1.2 times frame rate (e.g. 36 frames for a 30fps video),
		// and at least 30 frames to avoid big issues just in case r_frame_rate is a bit off.
		int numBufferedFrames = (int)ofClamp(av_q2d(video_stream->r_frame_rate)*1.4, 30.0, 70.0);
		ofLogError() << "[ofxAvVideoPlayer] Buffering " << numBufferedFrames << endl;
		for( int i = 0; i < numBufferedFrames; i++ ){
			ofxAvVideoData * data = new ofxAvVideoData();
			int ret = av_image_alloc(data->video_dst_data, data->video_dst_linesize,
									 video_context->width, video_context->height, video_context->pix_fmt, 1);
			if (ret < 0) {
				die("Could not allocate raw video buffer");
			}
			data->video_dst_bufsize = ret;
			video_buffers.push_back(data);
		}
	}
	
	if( video_stream_idx == -1 ){
		die("Could not find a video stream");
	}
	
	// ----------------------------------------------
	// Find audio stream
	// ----------------------------------------------
	audio_stream_idx = -1;
	if(open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0){
		audio_stream = fmt_ctx->streams[audio_stream_idx];
		audio_context = audio_stream->codec;
	}
 
	//TODO: we don't want to require an audio stream
	//when reading video
	if (audio_stream_idx == -1) {
		//die("Could not find an audio stream");
		//nope, not a problem anymore!
	}
 
	// Find the apropriate codec and open it
	if( forceNativeAudioFormat && audio_stream_idx >= 0){
		output_sample_rate = audio_context->sample_rate;
		output_setup_called = true; 
		output_channel_layout = audio_context->channel_layout;
		if( output_channel_layout == 0 ){
			output_num_channels = audio_context->channels;
			output_channel_layout = av_get_default_channel_layout(output_num_channels);
		}
		else{
			output_num_channels = av_get_channel_layout_nb_channels( output_channel_layout );
		}
		AVL_MEASURE(cout << "native audio thing: " << output_sample_rate << "Hz / " << output_num_channels << " channels" << endl;)
	}

	// dump info, this is interresting!
	av_dump_format(fmt_ctx, 0, input_filename, 0);

	// from here on it's mostly following
	// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c
	//packet.data = inbuf;
	//packet.size = fread(inbuf, 1, AVCODEC_AUDIO_INBUF_SIZE, f);
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;
	
	swr_context = NULL;
	sws_context = NULL;
	fileLoaded = true;
	isPlaying = true;
	
	// we continue here:
	decode_next_frame();
	//TODO: only video should be required.
	if( video_stream_idx >= 0){
		duration = 1000*video_stream->duration*av_q2d(video_stream->time_base);
	}
	else if( audio_stream_idx >= 0 ){
		duration = 1000*audio_stream->duration*av_q2d(audio_stream->time_base);
	}
	
	if( duration <= 0 ){
		duration = 1000 * fmt_ctx->duration*av_q2d(AVRational{ 1,AV_TIME_BASE });
	}
	
	if( duration <= 0 ){
		unload();
		return false;
	}
	
	next_seekTarget = 0;
	requestedSeek = true;
	decoderThread = thread(&ofxAvVideoPlayer::run_decoder, this);
	
	return true;
}


bool ofxAvVideoPlayer::setupAudioOut( int numChannels, int sampleRate ){
	output_setup_called = true;
	
	if( numChannels != output_num_channels || sampleRate != output_sample_rate ){
		output_channel_layout = av_get_default_channel_layout(numChannels);
		output_sample_rate = sampleRate;
		output_num_channels = numChannels;
		
		if( swr_context != NULL ){
			output_config_changed = true;
		}
	}
	
	return true;
}

void ofxAvVideoPlayer::unload(){
	fileNameAbs = "";
	fileLoaded = false;
	isPlaying = false;
	wantsUnload = true;
	if( decoderThread.joinable() ){
		decoderThread.join();
	}
	wantsUnload = false;
	
	packet_data_size = 0;
	len = 0;
	last_t = 0;
	last_pts = 0;
	restart_loop = false;
	texturePts = -1;
	next_seekTarget = 0;
	requestedSeek = false;
	decoder_last_audio_pts = 0;
	waitForVideoDelta = 0;
	
	audio_queue_mutex.lock();
	while( audio_queue.size() > 0 ){
		ofxAvAudioData * data = audio_queue.front();
		delete data;
		audio_queue.pop();
	}
	audio_frames_available = 0;
	audio_queue_mutex.unlock();
	video_buffers_mutex.lock();
	for( ofxAvVideoData * data : video_buffers ){
		av_freep(&data->video_dst_data[0]);
		delete data;
	}
	video_buffers_pos = 0;
	video_buffers.clear();
	video_buffers_mutex.unlock();
	
	next_seekTarget = -1;
	video_stream_idx = -1;
	audio_stream_idx = -1;
	
	av_free_packet(&packet);
	
	if( decoded_frame ){
		//TODO: do we really need the unref here?
		av_frame_unref(decoded_frame);
		av_frame_free(&decoded_frame);
		decoded_frame = NULL;
	}
	
	if( fmt_ctx ){
		avcodec_close(video_context);
		avcodec_close(audio_context);
		avformat_close_input(&fmt_ctx);
		avformat_free_context(fmt_ctx);
		av_free(fmt_ctx);
		fmt_ctx = NULL;
		audio_context = NULL;
		video_context = NULL;
	}
	
	if( swr_context ){
		swr_close(swr_context);
		swr_free(&swr_context);
		swr_context = NULL;
	}
	
	if( sws_context ){
		sws_freeContext(sws_context);
		sws_context = NULL;
	}
}

ofxAvVideoPlayer::AudioResult ofxAvVideoPlayer::audioOut(float *output, int bufferSize, int nChannels){
	AudioResult result{0,last_pts,last_t};
	if( !fileLoaded ){ return result; }
	if( !isPlaying ){ return result; }
	if( !output_setup_called ){
		ofLogWarning() << "[ofxAvVideoPlayer] audioOut() called, but audio was not set up. you must call either player.setupAudioOut() or, if you know what you are doing, set player.forceNativeAudioFormat=true" << endl;
		return result;
	}
	
	lock_guard<mutex> lock(audio_queue_mutex);
	if( audio_stream_idx < 0 ){
		memset(output,0,bufferSize*nChannels*sizeof(float));
		int64_t outLen = bufferSize;
		if(allowWaitForVideo && waitForVideoDelta>0){
			outLen = MAX(bufferSize/8,MIN(bufferSize,bufferSize-waitForVideoDelta*output_sample_rate));
			waitForVideoDelta -= outLen/(float)output_sample_rate;
		}
		
		int64_t nextPts = last_pts + outLen;
		double maxPts = duration*output_sample_rate/1000.0;
		if( nextPts > maxPts ){
			nextPts = maxPts;
		}
		result.pts = nextPts;
		result.t = result.pts/(double)output_sample_rate;
		result.numFrames = nextPts-last_pts;
		
		last_t = result.t;
		last_pts = result.pts;
		return result;
	}
	
	
	int num_samples_read = 0;
	uint64_t pts = 0;
	if( audio_queue.size() > 0 ){
		ofxAvAudioData * data = audio_queue.front();
		result.pts = data->pts + data->decoded_buffer_pos/output_num_channels;
		result.t = data->t + data->decoded_buffer_pos/output_num_channels/(double)output_sample_rate;
		last_pts = data->pts_native +  data->decoded_buffer_pos/output_num_channels/(double)output_sample_rate/av_q2d(audio_stream->time_base);
		last_t = result.t;
	}
	
	
	if(audio_frames_available < 2*bufferSize){
		requestSkipFrame = true;
		AVL_MEASURE(cout << "Request skip frame" << endl);
	}
	
	while( audio_queue.size() > 0 ){
		ofxAvAudioData * data = audio_queue.front();
		int missing_samples = bufferSize*nChannels - num_samples_read;
		int available_samples = data->decoded_buffer_len - data->decoded_buffer_pos;
		if( missing_samples > 0 && available_samples > 0 ){
			int samples = min( missing_samples, available_samples );
			
			if( volume != 0 ){
				memcpy(output+num_samples_read, data->decoded_buffer+data->decoded_buffer_pos, samples*sizeof(float) );
			}
			
			if( volume != 1 && volume != 0 ){
				for( int i = 0; i < samples; i++ ){
					output[i+num_samples_read] *= volume;
				}
			}
			
			data->decoded_buffer_pos += samples;
			num_samples_read += samples;
			audio_frames_available -= samples/nChannels;
			//TODO: increment timestamp?
			if( data->decoded_buffer_pos >= data->decoded_buffer_len ){
				audio_queue.pop();
				delete data;
			}
			
			if( num_samples_read >= bufferSize*nChannels ){
				result.numFrames = bufferSize;
				return result;
			}
		}
	}
	
	result.numFrames = num_samples_read/nChannels;
	return result;
}

// this is _mostly_ only called from the special happy thread!
bool ofxAvVideoPlayer::decode_next_frame(){
	av_free_packet(&packet);
	int res = av_read_frame(fmt_ctx, &packet);
	bool didRead = res >= 0;

	if( didRead ){
		int got_frame = 0;
		if (!decoded_frame) {
			if (!(decoded_frame = av_frame_alloc())) {
				fprintf(stderr, "Could not allocate audio frame\n");
				return false;
			}
		}
		else{
			av_frame_unref(decoded_frame);
		}
		
		// ----------------------------------------------
		// Handle audio stream
		// ----------------------------------------------
		if( packet.stream_index == audio_stream_idx && output_setup_called && audio_stream_idx >= 0 ){
			decode_audio_frame(got_frame);
			
			return true;
		}
		// ----------------------------------------------
		// Handle video stream
		// ----------------------------------------------
		else if(packet.stream_index == video_stream_idx ){
			decode_video_frame(got_frame);
			
			return true;
		}
		// ----------------------------------------------
		// Unknown data stream
		// ----------------------------------------------
		else{
			return true;
		}
	}
	else{
		// no data read...
		// there might be some leftover frames in the buffer!
		packet.data = NULL;
		packet.size = 0;
		int got_frame;
		if( decode_video_frame(got_frame,false) || got_frame ){
			// yep, we got another.
			// back to the start. btw, this game could happen >3 times !
			return true;
		}
		
		
		
		packet_data_size = 0;
		//TODO: clear out all buffers!
		if( isLooping ){
			//avformat_seek_file(fmt_ctx,audio_stream_idx,0,0,0,AVSEEK_FLAG_ANY);
			//avcodec_flush_buffers(audio_context);
			//decode_next_frame();
			if( isPlaying ){
				if( last_pts > 0 ){
					restart_loop = true;
				}
			}
			else{
				needsMoreVideo = false;
			}
			
			return false;
		}
		else{
			isPlaying = false;
		}
		
		return false;
	}
}

bool ofxAvVideoPlayer::decode_video_frame( int & got_frame, bool maySkip ){

	// maybe skip a frame?
	if(allowSkipFrames && requestSkipFrame && maySkip){
		requestSkipFrame = false;
		got_frame = 0;
		return false;
	}
	
	/* decode video frame */
	AVL_MEASURE(uint64_t decode_start = ofGetSystemTime();)
	int res = avcodec_decode_video2(video_context, decoded_frame, &got_frame, &packet);
	AVL_MEASURE(uint64_t decode_end = ofGetSystemTime();)
	AVL_MEASURE(cout << "decode_2 = " << (decode_end-decode_start)/1000.0 << endl;)
	if (res < 0) {
		fprintf(stderr, "Error decoding video frame (%s)\n", ofxAvUtils::errorString(res).c_str());
		return false;
	}
	
	if (got_frame) {
		lock_guard<mutex> lock(video_buffers_mutex);
		AVL_MEASURE(uint64_t queue_start = ofGetSystemTime();)
		queue_decoded_video_frame_vlocked();
		AVL_MEASURE(uint64_t queue_end = ofGetSystemTime();)
		AVL_MEASURE(cout << "queue_frame = " << (queue_end-queue_start)/1000.0 << endl;)
		needsMoreVideo = false;
		return true;
	}
	else{
		return false;
	}
}

bool ofxAvVideoPlayer::decode_audio_frame( int & got_frame ){
	len = avcodec_decode_audio4(audio_context, decoded_frame, &got_frame, &packet);
	if (len < 0) {
		// no data
		return false;
	}
	
	if (got_frame) {
		lock_guard<mutex> lock(audio_queue_mutex);
		queue_decoded_audio_frame_alocked();
	}
	
	packet.size -= len;
	packet.data += len;
	return true;
}

bool ofxAvVideoPlayer::queue_decoded_audio_frame_alocked(){
	if( swr_context != NULL && output_config_changed ){
		output_config_changed = false;
		if( swr_context ){
			swr_close(swr_context);
			swr_free(&swr_context);
			swr_context = NULL;
		}
	}
	
	if( swr_context == NULL ){
		int input_channel_layout = decoded_frame->channel_layout;
		if( input_channel_layout == 0 ){
			input_channel_layout = av_get_default_channel_layout( audio_context->channels );
		}
		swr_context = swr_alloc_set_opts(NULL,
										 output_channel_layout, AV_SAMPLE_FMT_FLT, output_sample_rate,
										 input_channel_layout, (AVSampleFormat)decoded_frame->format, decoded_frame->sample_rate,
										 0, NULL);
		swr_init(swr_context);
		
		if (!swr_context){
			fprintf(stderr, "Could not allocate resampler context\n");
			return false;
		}
	}
	
	int samples_per_channel = AVCODEC_MAX_AUDIO_FRAME_SIZE/output_num_channels;
	ofxAvAudioData * data = new ofxAvAudioData();
	uint8_t * out = (uint8_t*)data->decoded_buffer;
	int samples_converted = swr_convert(swr_context,
										(uint8_t**)&out, samples_per_channel,
										(const uint8_t**)decoded_frame->extended_data, decoded_frame->nb_samples);
	data->decoded_buffer_len = samples_converted*output_num_channels;
	data->decoded_buffer_pos = 0;
	if(packet.pts != AV_NOPTS_VALUE) {
		if(decoder_last_audio_pts < 0 ){
			decoder_last_audio_pts = (decoded_frame->pkt_pts)*output_sample_rate*av_q2d(audio_stream->time_base);
		}
		data->pts = decoder_last_audio_pts;
		data->pts_native = decoded_frame->pkt_pts;
		data->t = av_q2d(audio_stream->time_base)*(decoded_frame->pkt_pts);
		decoder_last_audio_pts += samples_converted;
		AVL_MEASURE(cout << "A: t=" << data->t << "\t\t" << last_t << endl;)
	}
	
	// the entire function must be called with an audio lock.
	// in reality locking from here on out would be good enough.
	audio_frames_available += samples_converted;
	audio_queue.push(data);
	
	return true;
}

// this is _mostly_ only called from the special happy thread!
bool ofxAvVideoPlayer::decode_until( double t, double & decoded_t ){
	int lookForDelayedPackets = 20;
	
decode_another:
	av_free_packet(&packet);
	int res = av_read_frame(fmt_ctx, &packet);
	bool didRead = res >= 0;
	
	if( !didRead && lookForDelayedPackets > 0 ){
		// no data read...
		// are frames hiding in the pipe?
		packet.data = NULL;
		packet.size = 0;
		packet.stream_index = video_stream_idx;
		packet_data_size = 0;
		lookForDelayedPackets --;
		didRead = true; 
	}
	
	// another seek requested during our seek? cancel!
	if( requestedSeek ) return false;
	
	
	if( didRead ){
		int got_frame = 0;
		if (!decoded_frame) {
			if (!(decoded_frame = av_frame_alloc())) {
				fprintf(stderr, "Could not allocate audio frame\n");
				return false;
			}
		}
		else{
			av_frame_unref(decoded_frame);
		}
		
		// ----------------------------------------------
		// Handle audio stream
		// ----------------------------------------------
		if( packet.stream_index == audio_stream_idx ){
			goto decode_another;
		}
		// ----------------------------------------------
		// Handle video stream
		// ----------------------------------------------
		else if(packet.stream_index == video_stream_idx ){
			/* decode video frame */
			res = avcodec_decode_video2(video_context, decoded_frame, &got_frame, &packet);
			if (res < 0) {
				fprintf(stderr, "Error decoding video frame (%s)\n", ofxAvUtils::errorString(res).c_str());
				goto decode_another;
			}
			
			if (got_frame) {
				double nextT = av_q2d(video_stream->time_base)*decoded_frame->pkt_pts;
				bool stillBehind = nextT < t;
				bool notTooFarBehind = t - nextT < 5;
				bool notTooLittleBehind = nextT+1/getFps()/2 < t;
				if( stillBehind && notTooFarBehind && notTooLittleBehind ){
					goto decode_another;
				}

				queue_decoded_video_frame_vlocked();
				decoded_t = video_buffers[(video_buffers_pos)%video_buffers.size()]->t;
			}
			else{
				goto decode_another;
			}
			
			return true;
		}
		// ----------------------------------------------
		// Unknown data stream
		// ----------------------------------------------
		else{
			goto decode_another;
		}
	}
	else{
		return false;
	}
}

bool ofxAvVideoPlayer::queue_decoded_video_frame_vlocked(){
	if (decoded_frame->width != width || decoded_frame->height != height /*|| decoded_frame->format != pix_fmt*/) {
		/* To handle this change, one could call av_image_alloc again and
		 * decode the following frames into another rawvideo file. */
		fprintf(stderr, "Error: Width, height and pixel format have to be "
				"constant in a rawvideo file, but the width, height or "
				"pixel format of the input video changed:\n"
				"old: width = %d, height = %d, format = %s\n"
				"new: width = %d, height = %d, format = %s\n",
				width, height, av_get_pix_fmt_name(pix_fmt),
				decoded_frame->width, decoded_frame->height,
				av_get_pix_fmt_name((AVPixelFormat)decoded_frame->format));
		return -1;
	}
	
	video_buffers_pos ++;
	video_buffers_pos %= video_buffers.size();
	ofxAvVideoData * data = video_buffers[video_buffers_pos];
	AVL_MEASURE(uint64_t cp_start = ofGetSystemTime();)
	av_image_copy(data->video_dst_data, data->video_dst_linesize,
				  (const uint8_t**)decoded_frame->data, decoded_frame->linesize,
				  video_context->pix_fmt, video_context->width, video_context->height);
	AVL_MEASURE(cout << "\tdata->pts = " << decoded_frame->pkt_pts << endl);
	AVL_MEASURE(uint64_t cp_end = ofGetSystemTime();)
	AVL_MEASURE(cout << "cp = " << (cp_end-cp_start)/1000.0 << endl;)
	//cout << packet.pts << "\t" << packet.dts <<"\t" << video_stream->first_dts << "\t" <<  decoded_frame->pkt_pts << "\t" << decoded_frame->pkt_dts << endl;
	data->pts = decoded_frame->pkt_pts;
	data->t = av_q2d(video_stream->time_base)*decoded_frame->pkt_pts;
	AVL_MEASURE(cout << "V: t=" << data->t << "\t" << last_t << endl;)
	
	return true; 
}

bool ofxAvVideoPlayer::copy_to_pixels_vlocked( ofxAvVideoData * data ){
	if( sws_context == NULL ){
		// Create context
		AVPixelFormat pixFormat = video_context->pix_fmt;
		switch (video_stream->codec->pix_fmt) {
			case AV_PIX_FMT_YUVJ420P :
				pixFormat = AV_PIX_FMT_YUV420P;
				break;
			case AV_PIX_FMT_YUVJ422P  :
				pixFormat = AV_PIX_FMT_YUV422P;
				break;
			case AV_PIX_FMT_YUVJ444P   :
				pixFormat = AV_PIX_FMT_YUV444P;
				break;
			case AV_PIX_FMT_YUVJ440P :
				pixFormat = AV_PIX_FMT_YUV440P;
			default:
				pixFormat = video_stream->codec->pix_fmt;
				break;
		}
		sws_context = sws_getContext(
									 // source
									 video_context->width, video_context->height, video_context->pix_fmt,
									 // dest
									 video_context->width, video_context->height, AV_PIX_FMT_RGB24,
									 // flags / src filter / dest filter / param
									 SWS_FAST_BILINEAR, NULL, NULL, NULL );
		
		
		// transfer color space from decoder to encoder
		int *coefficients;
		const int *new_coefficients;
		int full_range;
		int brightness, contrast, saturation;
		bool use_full_range = 0;
		
		if ( sws_getColorspaceDetails( sws_context, &coefficients, &full_range, &coefficients, &full_range,
									  &brightness, &contrast, &saturation ) != -1 )
		{
			new_coefficients = sws_getCoefficients(video_stream->codec->colorspace);
			sws_setColorspaceDetails( sws_context, new_coefficients, full_range, new_coefficients, full_range,
									 brightness, contrast, saturation );
		}
	}
	
	if( !video_pixels.isAllocated() || video_pixels.getWidth() != width || video_pixels.getHeight() != height ){
		video_pixels.allocate(width, height, OF_IMAGE_COLOR);
	}
	
	AVL_MEASURE(uint64_t sws_start = ofGetSystemTime();)
	int linesize[4] = {3*width,0,0,0};
	
	uint8_t * video_pixel_data = video_pixels.getData();
	uint8_t * dst[4] = {video_pixel_data, 0, 0, 0};
	
	sws_scale(sws_context,
			  // src slice / src stride
			  data->video_dst_data, data->video_dst_linesize,
			  // src slice y / src slice h
			  0, height,
			  // destinatin / destination stride
			  dst, linesize );
	AVL_MEASURE(uint64_t sws_end = ofGetSystemTime();)
	AVL_MEASURE(cout << "sws = " << (sws_end-sws_start)/1000.0 << endl;)
	
	return true;
}

long long ofxAvVideoPlayer::av_time_to_millis( int64_t av_time ){
	if( audio_stream_idx >= 0 ){
		return av_rescale(1000*av_time,audio_stream->time_base.num,audio_stream->time_base.den);
	}
	else if( output_setup_called ){
		return av_rescale(1000*av_time, 1, output_sample_rate);
	}
	else if( video_stream_idx >= 0 ){
		return av_rescale(1000*av_time,video_stream->time_base.num,video_stream->time_base.den);
	}
	else{
		return 0;
	}
	//alternative:
	//return av_time*1000*av_q2d(fmt_ctx->streams[audio_stream_id]->time_base);
}

int64_t ofxAvVideoPlayer::millis_to_av_time( long long ms ){
	//TODO: fix conversion
	/*	int64_t timeBase = (int64_t(audio_context->time_base.num) * AV_TIME_BASE) / int64_t(audio_context->time_base.den);
	 int64_t seekTarget = int64_t(ms) / timeBase;*/
	if( audio_stream_idx >= 0 ){
		return av_rescale(ms,audio_stream->time_base.den,audio_stream->time_base.num)/1000;
	}
	else if( output_setup_called ){
		return av_rescale(ms, output_sample_rate, 1)/1000.0;
	}
	else if( video_stream_idx >= 0 ){
		return av_rescale(ms,video_stream->time_base.den,video_stream->time_base.num)/1000;
	}
	else{
		return 0;
	}
}



void ofxAvVideoPlayer::setPositionMS(long long ms){
	int64_t next = millis_to_av_time(min(ms,duration));
	if( next != next_seekTarget ){
		requestedSeek = true;
		next_seekTarget = next;
	}
}

int ofxAvVideoPlayer::getPositionMS(){
	if( !fileLoaded ) return 0;
	return audio_stream_idx<0&&output_setup_called?
		av_rescale(1000*texturePts,video_stream->time_base.num,video_stream->time_base.den)
		:av_time_to_millis( last_pts );
}

float ofxAvVideoPlayer::getPosition(){
	return duration == 0? 0 : (getPositionMS()/(float)duration);
}

void ofxAvVideoPlayer::setPosition(float percent){
	if(duration>0) setPositionMS((int)(percent*duration));
}

void ofxAvVideoPlayer::stop(){
	isPlaying =false;
}

void ofxAvVideoPlayer::play(){
	if( fileLoaded ){
		isPlaying = true;
	}
}

void ofxAvVideoPlayer::setPaused(bool paused){
	isPlaying = fileLoaded?!paused:false;
}

void ofxAvVideoPlayer::setLoop(bool loop){
	isLooping = loop;
}

void ofxAvVideoPlayer::setVolume(float vol){
	this->volume = vol;
}

float ofxAvVideoPlayer::getVolume(){
	return volume;
}

bool ofxAvVideoPlayer::isLoaded(){
	return fileLoaded;
}

long long ofxAvVideoPlayer::getDurationMs(){
	return duration;
}

bool ofxAvVideoPlayer::getIsPlaying(){
	return isPlaying;
}

string ofxAvVideoPlayer::getMetadata( string key ){
	if( fmt_ctx != NULL ){
		AVDictionaryEntry * entry = av_dict_get(fmt_ctx->metadata, key.c_str(), NULL, 0);
		if( entry == NULL ) return "";
		else return string(entry->value);
	}
	else{
		return "";
	}
}

map<string,string> ofxAvVideoPlayer::getMetadata(){
	map<string,string> meta;
	AVDictionary * d = fmt_ctx->metadata;
	AVDictionaryEntry *t = NULL;
	while ((t = av_dict_get(d, "", t, AV_DICT_IGNORE_SUFFIX))!=0){
		meta[string(t->key)] = string(t->value);
	}
	
	return meta; 
}

ofTexture & ofxAvVideoPlayer::getTexture(){
	return texture;
}

const ofPixels & ofxAvVideoPlayer::getPixels(){
	return video_pixels;
}

int ofxAvVideoPlayer::getFrameNumber(){
	if( !isLoaded() ) return 0;
	// this is not ideal! in fact: it's simply not working yet! 
	lock_guard<mutex> lock(video_buffers_mutex);
	return last_t*av_q2d(video_stream->r_frame_rate);
}

double ofxAvVideoPlayer::getFps(){
	if( !isLoaded() ) return 0; 
	return av_q2d(video_stream->r_frame_rate); 
}

ofxAvVideoData * ofxAvVideoPlayer::video_data_for_time_vlocked( double t ){
	ofxAvVideoData * data = video_buffers[0];
	double bestDistance = 10;
	
	bool needsMoreVideo = true;
	int j = 0, bestJ = 0;
	for( ofxAvVideoData * buffer : video_buffers ){
		double distance = fabs(buffer->t - last_t);
		if( buffer->t > -1 && distance < bestDistance ){
			data = buffer;
			bestDistance = distance;
			bestJ = j;
		}
		
		j++;
	}

	return data;
}


void ofxAvVideoPlayer::update(){
	if( !texture.isAllocated()){
		texture.allocate(1,1,GL_RGB);
	}
	
	if( !fileLoaded ){
		return;
	}
	
	if( true ){
		// fudge it, we don't need a lock! ^^
//		lock_guard<mutex> lock(video_buffers_mutex);
		double request_t = last_t;
		ofxAvVideoData * data = video_data_for_time_vlocked(request_t);
		
		if( texturePts != data->pts ){
			if( !texture.isAllocated() || texture.getWidth() != width || texture.getHeight() != height ){
				texture.allocate( width, height, GL_RGB );
			}
			copy_to_pixels_vlocked(data);
			texture.loadData(video_pixels);
			texturePts = data->pts;
		}
		
		// we're basically done, now check for the next frame, maybe.
		if( isPlaying || texturePts == -1 ){
			double dt = 1.0/getFps();
			bool useVideoClock = !output_setup_called;
			float numFramesPreloaded = useVideoClock?2.2:1.1;
			double targetT = request_t+numFramesPreloaded*dt;
			ofxAvVideoData * nextFrame = video_data_for_time_vlocked(targetT);
			if( nextFrame->t < request_t || nextFrame->t == -1) needsMoreVideo = true;
			if( nextFrame->t < request_t && (request_t-nextFrame->t)>4*dt && allowWaitForVideo && audio_stream_idx<0){
				AVL_MEASURE(cout << "CRAZY DELAY [" << (request_t-nextFrame->t) << "]. request skipframe!" << endl );
				waitForVideoDelta = request_t-nextFrame->t - 2*dt;
			}
			else if(nextFrame->t < request_t && (request_t-nextFrame->t)>2*dt && allowSkipFrames){
				requestSkipFrame = true;
			}
			
			if( useVideoClock ){
				last_t += ofGetLastFrameTime();
				last_pts = last_t/av_q2d(video_stream->time_base);
			}
		}
		
/*		for( int i = 0; i < video_buffers.size(); i++ ){
			if( i == bestJ ) ofSetColor(255);
			else ofSetColor(200);
			ofDrawBitmapString(ofToString(video_buffers[i]->t), 10+40*i, 200);
		}*/
		
	}
}

void ofxAvVideoPlayer::run_decoder(){
	while( isLoaded() && !wantsUnload){
		if( restart_loop && isPlaying ){
			restart_loop = false;
			// wait until we run out of samples!
			while( audio_frames_available > 0 && !wantsUnload && isPlaying){
				ofSleepMillis(1);
			}
			
			if( isPlaying ){
				next_seekTarget = 0;
				requestedSeek = true;
			}
			//last_pts = 0;
			needsMoreVideo = true;
		}
		
		if( requestedSeek && next_seekTarget >= 0 ){
			requestedSeek = false;
			//av_seek_frame(fmt_ctx,-1,next_seekTarget,AVSEEK_FLAG_ANY);
/*			avcodec_flush_buffers(video_context);
			avcodec_flush_buffers(audio_context);
			avformat_seek_file(fmt_ctx,audio_stream_idx,0,next_seekTarget,next_seekTarget,AVSEEK_FLAG_BACKWARD);
			avformat_seek_file(fmt_ctx,video_stream_idx,0,next_seekTarget,next_seekTarget,AVSEEK_FLAG_BACKWARD);
*/
			
			lock_guard<mutex> audio_guard(audio_queue_mutex);
			lock_guard<mutex> video_guard(video_buffers_mutex);
			
			int stream_index = video_stream_idx;
			avcodec_flush_buffers(video_context);
			if(audio_stream_idx>=0){
				avcodec_flush_buffers(audio_context);
			}

			int64_t seek_target;
			if( audio_stream_idx >= 0 ){
				seek_target = next_seekTarget*1000000*av_q2d(audio_stream->time_base)-1000000;
			}
			else if( output_setup_called ){
				seek_target = next_seekTarget*1000000/(double)output_sample_rate-1000000;
			}
			else if( video_stream_idx >= 0 ){
				seek_target = next_seekTarget*1000000*av_q2d(video_stream->time_base)-1000000;
			}
			else{
				isPlaying = false;
				continue;
			}

			
			int flags = AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD;
			
			if(av_seek_frame(fmt_ctx, -1, seek_target, flags)<0) {
				cerr << "error while seeking\n" << fileNameAbs << endl;
				//last_t = 0;
				//last_pts = 0;
			}
			else{
				// last_pts = next_seekTarget;
				if( audio_stream_idx >= 0 ){
					//last_t = av_q2d(audio_stream->time_base)*last_pts;
				}
				else if( output_setup_called ){
					//last_t = last_pts/(double)output_sample_rate;
				}
				else if( video_stream_idx >= 0 ){
					//last_t = av_q2d(video_stream->time_base)*last_pts;
				}
				else{
					isPlaying = false;
					continue;
				}
				
			}
			
			// cancel if there was another seek
			if( requestedSeek ) continue;
			
			double want_t;
			if( audio_stream_idx >= 0 ){
				want_t = next_seekTarget*av_q2d(audio_stream->time_base);;
			}
			else if( output_setup_called ){
				want_t = next_seekTarget/(double)output_sample_rate;
			}
			else if( video_stream_idx >= 0 ){
				 want_t = next_seekTarget*av_q2d(video_stream->time_base);;
			}
			else{
				isPlaying = false;
				continue;
			}
			
			double got_t = -1;
			if( next_seekTarget == 0 ){
				needsMoreVideo = true;
				//last_t = 0;
				//last_pts = 0;
				got_t = 0;
			}
			else{
				decode_until(want_t, got_t);
			}
			
			// cancel if there was another seek
			if( requestedSeek ) continue;
			
			// did we skip within half a frame accuracy? great!
			if( got_t > want_t+1/getFps()/2.0 ){
				cout << "went too far!" << endl;
				// seek to 0, go forward
				if( video_stream_idx >= 0 ){
					avcodec_flush_buffers(video_context);
				}
				if( audio_stream_idx >= 0 ){
					avcodec_flush_buffers(audio_context);
				}
				
				if( audio_stream_idx >= 0 ){
					seek_target = next_seekTarget*1000000*av_q2d(audio_stream->time_base)-2500000;
				}
				else if( output_setup_called ){
					seek_target = next_seekTarget*1000000/(double)output_sample_rate-2500000;
				}
				else if( video_stream_idx >= 0 ){
					seek_target = next_seekTarget*1000000*av_q2d(video_stream->time_base)-2500000;
				}
				else{
					isPlaying = false;
					continue;
				}
				
				av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD|AVSEEK_FLAG_ANY);
				//avformat_seek_file(fmt_ctx, video_stream_idx, 0, 0, 0, AVSEEK_FLAG_BYTE|AVSEEK_FLAG_BACKWARD);
				if( video_stream_idx >= 0 ){
					avcodec_flush_buffers(video_context);
				}
				if( audio_stream_idx >= 0 ){
					avcodec_flush_buffers(audio_context);
				}

				// cancel if there was another seek
				if( requestedSeek ) continue;

				decode_until(want_t, got_t);
				cout << "p2 tried decoding to " << want_t << ", got " << got_t << endl;
			}
			
			if( got_t == -1 ){
				// emergency back to 0
				av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD|AVSEEK_FLAG_ANY);
				if( video_stream_idx >= 0 ) avcodec_flush_buffers(video_context);
				if( audio_stream_idx >= 0 ) avcodec_flush_buffers(audio_context);
				// alright, jump to 0 :(
				last_pts = 0;
				last_t = 0;
			}
			else{
				last_pts = next_seekTarget;
				last_t = got_t;
			}
			
			while( audio_queue.size() > 0 ){
				ofxAvAudioData * data = audio_queue.front();
				delete data;
				audio_queue.pop();
				audio_frames_available = 0;
			}
			

			decoder_last_audio_pts = -1;
			next_seekTarget = -1;
		}
		
		bool needsMoreAudio = audio_stream_idx >= 0 && output_setup_called && (audio_frames_available < output_sample_rate*0.5);
		
		if( (needsMoreAudio && isPlaying) || (needsMoreVideo) ){
			AVL_MEASURE(uint64_t time_start = ofGetSystemTime();)
			decode_next_frame();
			AVL_MEASURE(uint64_t time_end = ofGetSystemTime();)
			AVL_MEASURE(cout << "decode_time = " << (time_end-time_start)/1000.0 << endl;)
		}
		else{
			ofSleepMillis(1);
		}
	}
}

string ofxAvVideoPlayer::getFile(){
	return fileNameAbs;
}

string ofxAvVideoPlayer::getInfo(){
	stringstream info;
	
	if( isLoaded() ){
		info << "File: " << fileNameBase << endl;
		if( video_stream_idx >= 0 ){
			info << "Video: " << video_context->codec->name << ", " << video_context->width << " x " << video_context->height;
			
			float aspect = video_context->width/(float)video_context->height;
			float eps = 0.001;
			
			if( fabsf(aspect-4.0f/3.0f) < eps ) info <<" (4:3)";
			else if( fabsf(aspect-16.0f/9.0f) < eps ) info <<" (16:9)";
			else if( fabsf(aspect-3.0f/2.0f) < eps ) info <<" (3:2)";
			else if( fabsf(aspect-2.35f/1.0f) < eps ) info <<" (2.35:1)";

			info << " @ " << av_q2d(video_stream->r_frame_rate) << "fps" << endl;
		}
		else{
			info << "Video: -" << endl;
		}
		if( audio_stream_idx >= 0 ){
			info << "Audio: " << audio_context->codec->name << ", " << audio_context->channels << " channels, " << audio_context->sample_rate << "kHz" << endl;
		}
		else{
			info << "Audio: -" << endl;
		}
	}
	else{
		info << "no video loaded";
	}
	
	return info.str();
}

int ofxAvVideoPlayer::getCurrentFrame() {
    return (int)(round(getPositionMS()/(1000.0/getFps())));
}

int ofxAvVideoPlayer::getTotalNumFrames() {
    if( !isLoaded() ) return 0;
    return (int)(duration * 1000 * getFps());
}

void ofxAvVideoPlayer::firstFrame(){
    if(duration>0) {
        setPositionMS(0.0);
    }
}

void ofxAvVideoPlayer::nextFrame(){
    if(duration>0) {
        setPositionMS(getPositionMS() + 1000.0/getFps());
    }
}

void ofxAvVideoPlayer::previousFrame(){
    if(duration>0) {
        setPositionMS(getPositionMS() - 1000.0/getFps());
    }}

float ofxAvVideoPlayer::getHeight() const {
   return (float)height;
}

float ofxAvVideoPlayer::getWidth() const {
    return (float)width;
}

void ofxAvVideoPlayer::draw(float _x, float _y, float _w, float _h) {
    getTexture().draw(_x,_y,_w,_h);
}

void ofxAvVideoPlayer::draw(float _x, float _y) {
    draw(_x, _y, getWidth(), getHeight());
}

AVCodecID ofxAvVideoPlayer::getVideoCodec(){
	if(!fileLoaded || wantsUnload || video_stream_idx<0 || !video_context){
		return AV_CODEC_ID_NONE;
	}
	else{
		return video_context->codec_id;
	}
}