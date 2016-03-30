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

using namespace std;

#define die(msg) { unload(); cerr << msg << endl; return false; }

// this holds an image
class ofxAvVideoData{
public:
	uint8_t *video_dst_data[4];
	int      video_dst_linesize[4];
	int video_dst_bufsize;
	bool allocated;
	double pts;
	
	ofxAvVideoData() : allocated(false), video_dst_bufsize(0){}
};

class ofxAvAudioData{
public:
	double pts;
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
	volume = 1;
	
	forceNativeFormat = false;
	
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

bool ofxAvVideoPlayer::loadSound(string fileName, bool stream){
	return load(fileName, stream);
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
	
	string fileNameAbs = ofToDataPath(fileName,true);
	const char * input_filename = fileNameAbs.c_str();
	// the first finds the right codec, following  https://blinkingblip.wordpress.com/2011/10/08/decoding-and-playing-an-audio-stream-using-libavcodec-libavformat-and-libao/
	// and also demuxing_decoding.c in the ffmpeg examples
	// throughout all of it we are using "new api, reference counted" for memory management
	fmt_ctx = 0;
	if (avformat_open_input(&fmt_ctx, input_filename, NULL, NULL) < 0) {
		die("Could not open file");
	}
 
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
		// allocate a bunch of images!
		for( int i = 0; i < 10; i++ ){
			ofxAvVideoData * data = new ofxAvVideoData();
			int ret = av_image_alloc(data->video_dst_data, data->video_dst_linesize,
									 width, height, pix_fmt, 1);
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
	if( forceNativeFormat && audio_stream_idx >= 0){
		output_sample_rate = audio_context->sample_rate;
		output_channel_layout = audio_context->channel_layout;
		if( output_channel_layout == 0 ){
			output_num_channels = audio_context->channels;
			output_channel_layout = av_get_default_channel_layout(output_num_channels);
		}
		else{
			output_num_channels = av_get_channel_layout_nb_channels( output_channel_layout );
		}
		cout << "native audio thing: " << output_sample_rate << "Hz / " << output_num_channels << " channels" << endl;
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
	duration = av_time_to_millis(fmt_ctx->streams[audio_stream_idx]->duration);
	
	decoderThread = thread(&ofxAvVideoPlayer::run_decoder, this);
	return true;
}


bool ofxAvVideoPlayer::setupAudioOut( int numChannels, int sampleRate ){
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

void ofxAvVideoPlayer::unloadSound(){
	unload();
}

void ofxAvVideoPlayer::unload(){
	if( decoderThread.joinable() ){
		decoderThread.join();
	}
	
	len = 0;
	fileLoaded = false;
	isPlaying = false;
	packet_data_size = 0;
	
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

int ofxAvVideoPlayer::audioOut(float *output, int bufferSize, int nChannels){
	if( !fileLoaded ){ return 0; }
	if( !isPlaying ){ return 0; }
	
	
	int num_samples_read = 0;
	
	lock_guard<mutex> lock(audio_queue_mutex);
	
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
			audio_frames_available -= num_samples_read/nChannels;
			//TODO: increment timestamp?
			if( data->decoded_buffer_pos >= data->decoded_buffer_len ){
				audio_queue.pop();
				delete data;
			}
			
			if( num_samples_read >= bufferSize*nChannels ){
				return bufferSize;
			}
		}
	}
	
	cout << "not enough samples" << endl;
	return num_samples_read/nChannels;
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
		if( packet.stream_index == audio_stream_idx ){
			len = avcodec_decode_audio4(audio_context, decoded_frame, &got_frame, &packet);
			if (len < 0) {
				// no data
				return false;
			}
			
			if (got_frame) {
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
				
				/* if a frame has been decoded, resample to desired rate */
				int samples_per_channel = AVCODEC_MAX_AUDIO_FRAME_SIZE/output_num_channels;
				//samples_per_channel = 512;
				ofxAvAudioData * data = new ofxAvAudioData();
				uint8_t * out = (uint8_t*)data->decoded_buffer;
				int samples_converted = swr_convert(swr_context,
													(uint8_t**)&out, samples_per_channel,
													(const uint8_t**)decoded_frame->extended_data, decoded_frame->nb_samples);
				data->decoded_buffer_len = samples_converted*output_num_channels;
				data->decoded_buffer_pos = 0;
				lock_guard<mutex> lock(audio_queue_mutex);
				audio_frames_available += samples_converted;
				audio_queue.push(data);
			}
			
			//todo: what does this do, really?
			packet.size -= len;
			packet.data += len;
			//		packet->dts =
			//		packet->pts = AV_NOPTS_VALUE;
			return true;
		}
		// ----------------------------------------------
		// Handle video stream
		// ----------------------------------------------
		else if(packet.stream_index == video_stream_idx ){
			/* decode video frame */
			res = avcodec_decode_video2(video_context, decoded_frame, &got_frame, &packet);
			if (res < 0) {
				fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(res));
				return false;
			}
			
			if (got_frame) {
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
				
				if( sws_context == NULL ){
					// Create context
					sws_context = sws_getContext(
												 // source
												 video_context->width, video_context->height, video_context->pix_fmt,
												 // dest
												 video_context->width, video_context->height, AV_PIX_FMT_RGB24,
												 // flags / src filter / dest filter / param
												 SWS_FAST_BILINEAR, NULL, NULL, NULL );
				}
				
				//TODO: pass cached as param?
				//this will be required to handle the last frame correctly.
				bool cached = false;
/*				printf("video_frame%s n:%d coded_n:%d pts:%s\n",
					   cached ? "(cached)" : "",
					   video_frame_count++, frame->coded_picture_number,
					   av_ts2timestr(frame->pts, &video_dec_ctx->time_base));*/
				
				/* copy decoded frame to destination buffer:
				 * this is required since rawvideo expects non aligned data */
				// this will be relevant for filtering the video?
				/*av_image_copy(video_dst_data, video_dst_linesize,
							  (const uint8_t **)(decoded_frame->data), decoded_frame->linesize,
							  pix_fmt, width, height);*/
				lock_guard<mutex> lock(video_buffers_mutex);
				int index = (video_buffers_pos + video_buffers_size)%video_buffers.size();
				video_buffers_size ++;
				ofxAvVideoData * data = video_buffers[index];
				sws_scale(sws_context,
						  // src slice / src stride
						  decoded_frame->data, decoded_frame->linesize,
						  // src slice y / src slice h
						  0, height,
						  // destinatin / destination stride
						  data->video_dst_data, data->video_dst_linesize );
			}
			
			return true;
		}
		// ----------------------------------------------
		// Unknown data stream
		// ----------------------------------------------
		else{
			return false;
		}
	}
	else{
		// no data read...
		packet_data_size = 0;
		//TODO: clear out all buffers!
		if( isLooping ){
			avformat_seek_file(fmt_ctx,audio_stream_idx,0,0,0,AVSEEK_FLAG_ANY);
			avcodec_flush_buffers(audio_context);
			decode_next_frame();
		}
		else{
			isPlaying = false;
		}
		
		return false;
	}
}

unsigned long long ofxAvVideoPlayer::av_time_to_millis( int64_t av_time ){
	return av_rescale(1000*av_time,(uint64_t)fmt_ctx->streams[audio_stream_idx]->time_base.num,fmt_ctx->streams[audio_stream_idx]->time_base.den);
	//alternative:
	//return av_time*1000*av_q2d(fmt_ctx->streams[audio_stream_id]->time_base);
}

int64_t ofxAvVideoPlayer::millis_to_av_time( unsigned long long ms ){
	//TODO: fix conversion
	/*	int64_t timeBase = (int64_t(audio_context->time_base.num) * AV_TIME_BASE) / int64_t(audio_context->time_base.den);
	 int64_t seekTarget = int64_t(ms) / timeBase;*/
	return av_rescale(ms,fmt_ctx->streams[audio_stream_idx]->time_base.den,(uint64_t)fmt_ctx->streams[audio_stream_idx]->time_base.num)/1000;
}



void ofxAvVideoPlayer::setPositionMS(unsigned long long ms){
	next_seekTarget = millis_to_av_time(ms);
}

int ofxAvVideoPlayer::getPositionMS(){
	if( !fileLoaded ) return 0;
	int64_t ts = packet.pts;
	return av_time_to_millis( ts );
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
	isPlaying = fileLoaded?false:!paused;
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

unsigned long long ofxAvVideoPlayer::getDurationMs(){
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

void ofxAvVideoPlayer::update(){
	if( !texture.isAllocated() ){
		texture.allocate( width, height, GL_RGB );
	}
	
	if( video_buffers_size > 0 ){
		lock_guard<mutex> lock(video_buffers_mutex);
		int index = video_buffers_pos;
		video_buffers_pos ++;
		if( video_buffers_pos >= video_buffers.size() ){
			video_buffers_pos -= (int)video_buffers.size();
		}
		video_buffers_size --;
		ofxAvVideoData * data = video_buffers[index];
		texture.loadData(data->video_dst_data[0], width, height, GL_RGB);
	}
}

void ofxAvVideoPlayer::run_decoder(){
	while( isLoaded() ){
		if( next_seekTarget >= 0 ){
			//av_seek_frame(fmt_ctx,-1,next_seekTarget,AVSEEK_FLAG_ANY);
			avformat_seek_file(fmt_ctx,audio_stream_idx,0,next_seekTarget,next_seekTarget,AVSEEK_FLAG_ANY);
			next_seekTarget = -1;
			avcodec_flush_buffers(audio_context);
			lock_guard<mutex> lock(audio_queue_mutex);
			while( audio_queue.size() > 0 ){
				ofxAvAudioData * data = audio_queue.front();
				delete data;
				audio_queue.pop();
				audio_frames_available = 0;
			}
			lock_guard<mutex> videoLock(video_buffers_mutex);
			video_buffers_size = 0;
		}
		
		if( (audio_frames_available < output_sample_rate*0.1 || video_buffers_size < 3) && isPlaying ){
			decode_next_frame();
		}
		else{
			ofSleepMillis(10);
		}
	}
}