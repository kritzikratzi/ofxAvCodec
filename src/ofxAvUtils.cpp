//
//  ofxAvUtils.cpp
//  emptyExample
//
//  Created by Hansi on 16.09.15.
//
//

#include "ofxAvUtils.h"
#include "ofxAvAudioPlayer.h"
#include "ofMain.h"
#include <Poco/Path.h>
extern "C"{
	#include <libavutil/opt.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}

using namespace std;

// following lock impl as discussed in opencv http://code.opencv.org/issues/1369
static int ffmpeg_lockmgr_cb(void **mutex, enum AVLockOp op);

void ofxAvUtils::init(){
	static bool didInit = false;
	if( didInit ) return;
	didInit = true;
	cout << "Initializing AvCodec" << endl;
	av_register_all();
	av_lockmgr_register(ffmpeg_lockmgr_cb);
}

std::map<string,string> ofxAvUtils::read( std::string filename ){
	return readMetadata(filename);
}

std::map<string,string> ofxAvUtils::readMetadata( std::string filename ){
	init();
	string fileNameAbs = ofToDataPath(filename,true);
	const char * input_filename = fileNameAbs.c_str();
	// the first finds the right codec, following  https://blinkingblip.wordpress.com/2011/10/08/decoding-and-playing-an-audio-stream-using-libavcodec-libavformat-and-libao/
	
	AVFormatContext * container = 0;
	if (avformat_open_input(&container, input_filename, NULL, NULL) < 0) {
		cerr << "couldn't open the file :(" << endl;
		return map<string,string>();
	}
	
	map<string,string> meta;
	AVDictionary * d = container->metadata;
	AVDictionaryEntry *t = NULL;
	while ((t = av_dict_get(d, "", t, AV_DICT_IGNORE_SUFFIX))!=0){
		meta[string(t->key)] = string(t->value);
	}
	
	avformat_close_input(&container);
	avformat_free_context(container);
	
	return meta;
}

bool ofxAvUtils::update(std::string filename, std::map<std::string, std::string> newMetadata){
	return updateMetadata(filename, newMetadata);
}

bool ofxAvUtils::updateMetadata(std::string filename, std::map<std::string, std::string> newMetadata){
	init();
	string fileNameAbs = ofToDataPath(filename,true);
	const char * input_filename = fileNameAbs.c_str();
	AVCodecContext * codec_context = NULL;
	AVCodec* codec = NULL;
	AVFormatContext *ofmt_ctx = NULL;
	AVCodec * out_codec = NULL;
	AVStream * out_stream = NULL;
	AVCodecContext * out_c = NULL;
	int i, ret;
	int audio_stream_id = -1;
	AVDictionary *d = NULL;
	AVPacket * packet = NULL;
	AVFormatContext* container = NULL;
	map<string,string>::iterator it;
	bool success = false;
	Poco::Path filepath(fileNameAbs);
	filepath.setBaseName("." + filepath.getBaseName()+".meta_tmp");
	string destFile = filepath.toString();
	
	if (avformat_open_input(&container, input_filename, NULL, NULL) < 0) {
		cerr << ("Could not open file") << endl;
		goto panic;
	}
 
	if (avformat_find_stream_info(container,NULL) < 0) {
		cerr << ("Could not find file info") << endl;
		goto panic;
	}
 
	audio_stream_id = -1;
 
	// To find the first audio stream. This process may not be necessary
	// if you can gurarantee that the container contains only the desired
	// audio stream
	for (i = 0; i < container->nb_streams; i++) {
		if (container->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_stream_id = i;
			break;
		}
	}
 
	if (audio_stream_id == -1) {
		cerr << ("Could not find an audio stream") << endl;
		goto panic;
	}
 
	// Find the apropriate codec and open it
	codec_context = container->streams[audio_stream_id]->codec;
 
	codec = avcodec_find_decoder(codec_context->codec_id);
	if (avcodec_open2(codec_context, codec,NULL)) {
		cerr << ("Could not find open the needed codec") << endl;
		goto panic;
	}
	
	// ---------------------------------------------
	// OK OK OK
	// WE HAVE AN OPEN FILE!!!!
	// now let's create the output file ...
	// ---------------------------------------------
	
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, destFile.c_str());
	if (!ofmt_ctx) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		goto panic;
	}
	
	out_codec = avcodec_find_encoder(codec->id);
	if (!out_codec) {
		cerr << ("codec not found") << endl;
		return false;
	}
	
	out_stream = avformat_new_stream(ofmt_ctx, out_codec);
	if (!out_stream) {
		cerr << ("Failed allocating output stream\n") << endl;
		return false;
	}
	
	out_c = out_stream->codec;
	if (!out_c) {
		cerr << ("could not allocate audio codec context") << endl;
		return false;
	}
	
	out_c->sample_fmt = codec_context->sample_fmt;
	out_c->sample_rate = codec_context->sample_rate;
	out_c->channel_layout = codec_context->channel_layout;
	out_c->channels       = codec_context->channels;
	out_c->frame_size = codec_context->frame_size;
	
	if (avcodec_open2(out_c, out_codec, NULL) < 0) {
		cerr << ("could not open codec") << endl;
		goto panic;
	}
	
	
	
	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		int ret = avio_open(&ofmt_ctx->pb, destFile.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			cerr << ("Could not open output file") << endl;
			goto panic;
		}
	}
	
	it = newMetadata.begin();
	while( it != newMetadata.end() ){
		av_dict_set(&d, (*it).first.c_str(), (*it).second.c_str(), 0);
		++it;
	}
	
	ofmt_ctx->metadata = d;
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		goto panic;
	}
	
	
	
	// ---------------------------------------------
	// OK, We have all we need, really. let's start this party!
	// ---------------------------------------------
	
	//packet.data = inbuf;
	//packet.size = fread(inbuf, 1, AVCODEC_AUDIO_INBUF_SIZE, f);
	packet = new AVPacket();
	av_init_packet(packet);
	packet->data = NULL;
	packet->size = 0;
	
	// read all packet (frames? who the fuck knows)
	while(true){
		int res = av_read_frame(container, packet);
		if( res >= 0 ){
			av_write_frame(ofmt_ctx, packet);
		}
		else{
			break;
		}
	}
	
	av_write_trailer(ofmt_ctx);
	
	success = true;
	
panic:
	if( packet ){
		av_free_packet(packet);
	}
	
	if( ofmt_ctx ){
		// also closes out_c and out_stream and out_codec! ?
		avformat_free_context(ofmt_ctx);
	}
	
	if( codec_context ){
		avcodec_close(codec_context);
		av_free(codec_context);
	}
	
	if( success ){
		remove(fileNameAbs.c_str());
		rename(destFile.c_str(), fileNameAbs.c_str());
	}
	else{
		remove(destFile.c_str());
	}
	
	return success;
}

double ofxAvUtils::duration( std::string filename ){
	init();
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	string file = ofToDataPath(filename);
	avformat_open_input(&pFormatCtx, file.c_str(), NULL, NULL);
	if(!pFormatCtx ){
		avformat_free_context(pFormatCtx);
		return 0.00000001;
	}
	avformat_find_stream_info(pFormatCtx,NULL);
	int64_t duration = pFormatCtx->duration;
	avformat_close_input(&pFormatCtx);
	avformat_free_context(pFormatCtx);
	
	return duration/(double)AV_TIME_BASE;
}

double ofxAvUtils::duration_audio(std::string filename) {
	init();
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	string file = ofToDataPath(filename);
	avformat_open_input(&pFormatCtx, file.c_str(), NULL, NULL);
	if (!pFormatCtx) {
		avformat_free_context(pFormatCtx);
		return 0.0;
	}

	double duration = 0;

	int res = avformat_find_stream_info(pFormatCtx, NULL);
	if (res >= 0) {
		for (int i = 0; i < pFormatCtx->nb_streams; i++) {
			auto stream = pFormatCtx->streams[i];
			if (stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
				if (stream->time_base.den == 0) duration = 0;
				else duration = stream->duration / (double)stream->time_base.num / stream->time_base.den;
				break;
			}
		}
	}

	avformat_close_input(&pFormatCtx);
	avformat_free_context(pFormatCtx);

	return duration;
}

float * ofxAvUtils::waveform( std::string filename, int resolution, float fixedDurationInSeconds ){
	if( resolution < 1 ) return NULL;
	
	//TODO: find memleaks of player
	ofxAvAudioPlayer player;
	player.setupAudioOut(1, 22050);
	player.load(filename);
	player.play();
	float buffer[512];
	int len;
	int lastPos = 0;

	float * result = new float[resolution];
	memset(result, 0, resolution*sizeof(float));
	
	if( player.getDurationMs() == 0 ){
		player.unload();
		return result;
	}
	
	int i = 0;
	int max = (int)(fixedDurationInSeconds == -1.0f?(22.050f*player.getDurationMs()):(22050*fixedDurationInSeconds));
	while( ( len = player.audioOut(buffer,512,1)) > 0 ){
		for( int j = 0; j < len; j++ ){
			// don't remove that cast to float, or we get int overflows!
			if( i > max ) goto unload;
			int pos = (int)min(resolution*(float)i/max, resolution-1.0f);
			result[pos] = MAX(result[pos],abs(buffer[j]));
			++i;
		}
	}
unload:

	player.unload();
	return result;
}

ofPolyline ofxAvUtils::waveformAsPolyline( std::string filename, int resolution, float meshWidth, float meshHeight, float fixedDuratioInSeconds ){
	ofPolyline polyline;
	float h = 0.5f*meshHeight-1;
	if( meshWidth < 1 || resolution < 2 ) return polyline;
	float * amp = waveform(filename, resolution, fixedDuratioInSeconds);
	
	if( amp != NULL ){
		for( int i = 0; i < resolution; i++ ){
			polyline.lineTo(i*meshWidth/(resolution-1), h*(1-amp[i])-0.5f );
		}
		
		for( int i = resolution-1; i >= 0; i-- ){
			polyline.lineTo(i*meshWidth/(resolution-1), h*(1+amp[i])+0.5f );
		}
		
		delete [] amp;
	}
	
	return polyline;
}

ofMesh ofxAvUtils::waveformAsMesh( std::string filename, int resolution, float meshWidth, float meshHeight, float fixedDuratioInSeconds ){
	ofPolyline polyline = waveformAsPolyline(filename, resolution, meshWidth, meshHeight, fixedDuratioInSeconds);
	ofTessellator tesselator;
	ofMesh mesh;
	tesselator.tessellateToMesh(polyline, OF_POLY_WINDING_ODD, mesh);
	
	return mesh;
}

string ofxAvUtils::errorString(int errNum) {
	char str[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(errNum, str, AV_ERROR_MAX_STRING_SIZE);
	return string(str);
}

static int ffmpeg_lockmgr_cb(void **mutex, enum AVLockOp op){
	switch(op){
		case AV_LOCK_CREATE:
			*mutex = new ofMutex();
			return *mutex?0:1;
		case AV_LOCK_DESTROY:
			delete (ofMutex*)*mutex;
			return 0;
		case AV_LOCK_OBTAIN:
			((ofMutex*)*mutex)->lock();
			return 0;
		case AV_LOCK_RELEASE:
			((ofMutex*)*mutex)->unlock();
			return 0;
	}
	return 0; 
}

vector<shared_ptr<ofPixels>> ofxAvUtils::readVideoFrames( std::string filename, int startFrame, int endFrame){
	init();
	
	string input_filename = ofToDataPath(filename,true);
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* video_context = nullptr;
	AVStream * video_stream = nullptr;
	AVPixelFormat pix_fmt;
	int video_stream_idx;
	SwsContext * sws_context = nullptr;
	int width;
	int height;

	// packet with encoded data (who knows what's inside)
	AVFrame *decoded_frame = nullptr;
	AVPacket packet;
	int packet_data_size;

	// decoded data (any format, but maybe some kind of pixels already)
	uint8_t *video_dst_data[4] = {nullptr};
	int      video_dst_linesize[4];
	int video_dst_bufsize;
	
	// we need those later during video decoding
	bool readMore = true;
	int lookForDelayedPackets = 20;
	int currentFrame = -1;


	vector<shared_ptr<ofPixels>> result;
	
	
	if (avformat_open_input(&fmt_ctx, input_filename.c_str(), NULL, NULL) < 0) {
		cout << "Could not open file" << endl;
		goto readVideoFramesExitEarly;
	}
	
	if (avformat_find_stream_info(fmt_ctx,NULL) < 0) {
		cout << "Could not find file info" << endl;
		goto readVideoFramesExitEarly;
	}

	if (openCodecContext(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
		video_stream = fmt_ctx->streams[video_stream_idx];
		video_context = video_stream->codec;
		
		/* allocate image where the decoded image will be put */
		// we will use the swr resampling context to fill out the data!
		width = video_context->width;
		height = video_context->height;
		pix_fmt = AV_PIX_FMT_RGB24;
		
		//LATER:
		int ret = av_image_alloc(video_dst_data, video_dst_linesize,
								 video_context->width, video_context->height, video_context->pix_fmt, 1);
		if (ret < 0) {
			cout << "Could not allocate raw video buffer" << endl;
			goto readVideoFramesExitEarly;
		}

	}
	
	if( video_stream_idx == -1 ){
		cout << "Could not find a video stream" << endl;
		goto readVideoFramesExitEarly;
	}
	
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;
	
	
	// decode frames ...
	while( readMore ){
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
			continue;
		}
		else if(!didRead){
			break;
		}
		
		if( didRead ){
			int got_frame = 0;
			if (!decoded_frame) {
				if (!(decoded_frame = av_frame_alloc())) {
					fprintf(stderr, "Could not allocate data frame\n");
					goto readVideoFramesExitEarly;
				}
			}
			else{
				av_frame_unref(decoded_frame);
			}

			// Handle video stream
			if(packet.stream_index == video_stream_idx ){
				/* decode video frame */
				res = avcodec_decode_video2(video_context, decoded_frame, &got_frame, &packet);
				if (res < 0) {
					fprintf(stderr, "Error decoding video frame (%s)\n", ofxAvUtils::errorString(res).c_str());
					continue;
				}
				
				if (got_frame) {
					double t = av_q2d(video_stream->time_base)*decoded_frame->pkt_pts;
					cout << "got frame at " << t << endl;
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
						goto readVideoFramesExitEarly;
					}
					else{
						currentFrame ++;
						if(currentFrame<startFrame) continue;
						if(endFrame>0 && currentFrame>endFrame) break;
						
						av_image_copy(video_dst_data, video_dst_linesize,
									  (const uint8_t**)decoded_frame->data, decoded_frame->linesize,
									  video_context->pix_fmt, video_context->width, video_context->height);

					
					
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
						
						
						shared_ptr<ofPixels> video_pixels = make_shared<ofPixels>();
						video_pixels->allocate(width, height, OF_IMAGE_COLOR);
						uint8_t * video_pixel_data = video_pixels->getData();
						uint8_t * dst[4] = {video_pixel_data, 0, 0, 0};
						int linesize[4] = {3*width,0,0,0};
						sws_scale(sws_context,
								  // src slice / src stride
								  video_dst_data, video_dst_linesize,
								  // src slice y / src slice h
								  0, height,
								  // destinatin / destination stride
								  dst, linesize );
						
						result.push_back(video_pixels);
					}
				}
			}
		}
	}
	
	
readVideoFramesExitEarly:
	// clean up
	av_freep(&video_dst_data[0]);
	av_free_packet(&packet);
	if(video_context){
		avcodec_close(video_context);
		video_context = NULL;
	}

	if( decoded_frame ){
		//TODO: do we really need the unref here?
		av_frame_unref(decoded_frame);
		av_frame_free(&decoded_frame);
		decoded_frame = NULL;
	}

	if( fmt_ctx ){
		avformat_close_input(&fmt_ctx);
		avformat_free_context(fmt_ctx);
		av_free(fmt_ctx);
		fmt_ctx = NULL;
		video_context = NULL;
	}
	
	if( sws_context ){
		sws_freeContext(sws_context);
		sws_context = NULL;
	}

	return result;
}

int ofxAvUtils::openCodecContext(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type){
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
