//
//  ofxAvAudioPlayer.cpp
//  emptyExample
//
//  Created by Hansi on 13.07.15.
//
// a huge chunk of this file is based on the
// blog entry https://blinkingblip.wordpress.com/2011/10/08/decoding-and-playing-an-audio-stream-using-libavcodec-libavformat-and-libao/
// (had to make some adjustments for changes in ffmpeg
// and libavcodecs examples
// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c

#include "ofxAvAudioPlayer.h"

using namespace std;

#define die(msg) cerr << msg << endl; return false;

ofxAvAudioPlayer::ofxAvAudioPlayer(){
	f = NULL;
	decoded_frame = NULL;
	codec_context = NULL;
	buffer_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
	
	av_register_all();

	unloadSound();
}

bool ofxAvAudioPlayer::loadSound(string fileName, bool stream){
	unloadSound();
	
	const char * input_filename = fileName.c_str();
	// the first finds the right codec, following  https://blinkingblip.wordpress.com/2011/10/08/decoding-and-playing-an-audio-stream-using-libavcodec-libavformat-and-libao/
	container = 0;
	if (avformat_open_input(&container, input_filename, NULL, NULL) < 0) {
		die("Could not open file");
	}
 
	if (avformat_find_stream_info(container,NULL) < 0) {
		die("Could not find file info");
	}
 
	int stream_id = -1;
 
	// To find the first audio stream. This process may not be necessary
	// if you can gurarantee that the container contains only the desired
	// audio stream
	int i;
	for (i = 0; i < container->nb_streams; i++) {
		if (container->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			stream_id = i;
			break;
		}
	}
 
	if (stream_id == -1) {
		die("Could not find an audio stream");
	}
 
	// Extract some metadata
	AVDictionary* metadata = container->metadata;
 
	if( metadata != NULL ){
		const char* artist = av_dict_get(metadata, "artist", NULL, 0)->value;
		const char* title = av_dict_get(metadata, "title", NULL, 0)->value;
		fprintf(stdout, "Playing: %s - %s\n", artist, title);
	}
 
 
	// Find the apropriate codec and open it
	codec_context = container->streams[stream_id]->codec;
 
	AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
//	codec_context = avcodec_alloc_context3(codec);
//	codec_context->request_channels = 2;
//	codec_context->request_channel_layout = AV_CH_STEREO_LEFT|AV_CH_STEREO_RIGHT;
	if (avcodec_open2(codec_context, codec,NULL)) {
		die("Could not find open the needed codec");
	}
	
	// from here on it's mostly following
	// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c
	f = fopen(input_filename, "rb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", input_filename);
		return false;
	}
	
	packet.data = inbuf;
	packet.size = fread(inbuf, 1, AVCODEC_AUDIO_INBUF_SIZE, f);

	// we continue here:
	decodeNextFrame();
	
	swr_context = NULL;
/*	// convert from planar int16 to interleaved float32
	// also change sample rate from 8kHz to 16kHz.
	const int16_t in[2][4] = {{30000,1,2,3}, {10, 9, 8, 7}};
	//const uint8_t * in_ptr[2] = {(const uint8_t*)in[0],(const uint8_t*)in[0]};
	const uint8_t * in_ptr[2]={(const uint8_t*)in,(const uint8_t*)in[1]};
	cout << ((int16_t**)in_ptr)[0][0] << endl;
	cout << (int)in_ptr[1][1] << endl;
	float out[16] = {0};
	uint8_t * out_ptr = (uint8_t*)out;
	
	swr_context = swr_alloc_set_opts(NULL,
						AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16P, 8000,
						AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_FLTP,  8000,
						0, NULL);
	int res = swr_init(swr_context);
	res = swr_convert(swr_context, &out_ptr, 8, in_ptr, 4);
	cout << out << endl;
	die("DONE");*/
	

	isLoaded = true;

	return true;
}

bool ofxAvAudioPlayer::setupAudioOut( int numChannels, int sampleRate ){
	output_channel_layout = av_get_default_channel_layout(numChannels);
	output_sample_rate = sampleRate;
	output_num_channels = numChannels;

	return true;
}


void ofxAvAudioPlayer::unloadSound(){
	len = 0;
	isLoaded = false;
	packet_data_size = 0;
	decoded_buffer_pos = 0;
	decoded_buffer_len = 0;

	
	if( f ){
		fclose(f);
		f = NULL; 
	}
	if( codec_context ){
		avcodec_close(codec_context);
		av_free(codec_context);
		codec_context = NULL;
	}
	if( decoded_frame ){
		av_frame_free(&decoded_frame);
		decoded_frame = NULL;
	}
	
	if( swr_context ){
		swr_free(&swr_context);
		swr_context = NULL;
	}
}

void ofxAvAudioPlayer::audioOut(float *output, int bufferSize, int nChannels){
	if( !isLoaded ){ return; }
	
	int max_read_packets = 2;
	if( decoded_frame && decoded_frame);
	// number of samples read per channel (up to bufferSize)
	int num_samples_read = 0;
	
	if( decoded_frame == NULL ){
		decodeNextFrame();
	}
	
	while( decoded_frame != NULL && max_read_packets > 0 ){
		max_read_packets --;
		
		int missing_samples = bufferSize*nChannels - num_samples_read;
		int available_samples = decoded_buffer_len - decoded_buffer_pos;
		if( missing_samples > 0 && available_samples > 0 ){
			int samples = min( missing_samples, available_samples );
			memcpy(output+num_samples_read, decoded_buffer+decoded_buffer_pos, samples*sizeof(float) );
			
			decoded_buffer_pos += samples;
			num_samples_read += samples;
		}
		
		if( num_samples_read >= bufferSize*nChannels ){
			return;
		}
		else{
			cout << "read packet" << endl; ;
			decodeNextFrame();
		}
	}
}

void ofxAvAudioPlayer::decodeNextFrame(){
	if(packet.size > 0) {
		int got_frame = 0;
		if (!decoded_frame) {
			if (!(decoded_frame = av_frame_alloc())) {
				fprintf(stderr, "Could not allocate audio frame\n");
				exit(1);
			}
		} else
			av_frame_unref(decoded_frame);
		len = avcodec_decode_audio4(codec_context, decoded_frame, &got_frame, &packet);
		if (len < 0) {
			fprintf(stderr, "Error while decoding\n");
			exit(1);
		}
		if (got_frame) {
			
			if( swr_context == NULL ){
				int input_channel_layout = decoded_frame->channel_layout;
				if( input_channel_layout == 0 ){
					input_channel_layout = av_get_default_channel_layout( codec_context->channels );
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
			uint8_t * out = (uint8_t*)decoded_buffer;
			int samples_converted = swr_convert(swr_context,
												(uint8_t**)&out, samples_per_channel,
												(const uint8_t**)decoded_frame->extended_data, decoded_frame->nb_samples);
			decoded_buffer_len = samples_converted*output_num_channels;
			decoded_buffer_pos = 0;
		}

		packet.size -= len;
		packet.data += len;
		packet.dts =
		packet.pts = AV_NOPTS_VALUE;

		
		if (packet.size < AVCODEC_AUDIO_REFILL_THRESH) {
			/* Refill the input buffer, to avoid trying to decode
			 * incomplete frames. Instead of this, one could also use
			 * a parser, or use a proper container format through
			 * libavformat. */
			memmove(inbuf, packet.data, packet.size);
			packet.data = inbuf;
			len = fread(packet.data + packet.size, 1,
						AVCODEC_AUDIO_INBUF_SIZE - packet.size, f);
			if (len > 0)
				packet.size += len;
		}
	}
	else{
		packet_data_size = 0;
		decoded_buffer_len = 0;
		decoded_buffer_pos = 0;
		// seek to beginning!
		//			av_seek_frame(container,0,0,AVSEEK_FLAG_ANY);
		//			avcodec_flush_buffers(codec_context);
		/*int64_t timeBase = (int64_t(codec_context->time_base.num) * AV_TIME_BASE) / int64_t(codec_context->time_base.den);
		int64_t seekTarget = int64_t(0) * timeBase;
		if(av_seek_frame(container, -1, seekTarget, AVSEEK_FLAG_ANY) < 0)
			die("av_seek_frame failed.");*/
		 //avformat_seek_file(container, -1, INT64_MIN, 0, 0, 0);

	}
}
