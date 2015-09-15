//
//  ofxAvAudioWriter.cpp
//  emptyExample
//
//  Created by Hansi on 13.09.15.
//
//

#include "ofxAvAudioWriter.h"
#include <algorithm>
#include "ofxAvAudioPlayer.h"
#include "ofMain.h"

using namespace std;

#define die(msg) { close(); cerr << msg << endl; return false; }
static int check_sample_fmt(AVCodec *codec, enum AVSampleFormat sample_fmt);
static int select_sample_rate(AVCodec *codec, int preferred);
static int select_channel_layout(AVCodec *codec, int preferred);

ofxAvAudioWriter::ofxAvAudioWriter(){
	swr_context = NULL;
	ofmt_ctx = NULL;
	out_stream = NULL;
	samples = NULL;
	codec = NULL;
	c= NULL;
	frame = NULL;
	pkt = new AVPacket();
}

ofxAvAudioWriter::~ofxAvAudioWriter(){
	delete pkt;
}

void ofxAvAudioWriter::setup(int sampleRate, int numChannels){
	in_sample_rate = sampleRate;
	in_num_channels = numChannels;
	in_channel_layout = av_get_default_channel_layout(in_num_channels);
}

bool ofxAvAudioWriter::open(string filename, string formatExtension, int sampleRate, int numChannels){
	return open(filename, codecForExtension(formatExtension), sampleRate, numChannels );
}

bool ofxAvAudioWriter::open(string filename, AVCodecID codec_id, int requestedOutSampleRate, int requestedOutNumChannels ){
	file_codec = codec_id;
	
	
	int i, j, k, ret, got_output;
	float t, tincr;
	
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename.c_str());
	if (!ofmt_ctx) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return AVERROR_UNKNOWN;
	}
	
	/* find the encoder */
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		die("codec not found");
	}
	
	out_stream = avformat_new_stream(ofmt_ctx, codec);
	if (!out_stream) {
		die("Failed allocating output stream\n");
	}
	
	c = out_stream->codec;
	// old!
	// c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate audio codec context\n");
		die("could not allocate audio codec context");
	}
	
	/* put sample parameters */
	// todo: what about this???
	// c->bit_rate = 64000;
	
	/* check that the encoder supports s16 pcm input */
	// TODO: find sample fmt automatically !
	c->sample_fmt = AV_SAMPLE_FMT_S16;
	if (!check_sample_fmt(codec, c->sample_fmt)) {
		fprintf(stderr, "Encoder does not support sample format %s",
				av_get_sample_fmt_name(c->sample_fmt));
		die("encoder does not support sample format");
	}
	
	/* select other audio parameters supported by the encoder */
	c->sample_rate = select_sample_rate(codec, requestedOutSampleRate==-1?in_sample_rate:requestedOutSampleRate);
	c->channel_layout = select_channel_layout(codec, av_get_default_channel_layout(requestedOutNumChannels==-1?in_num_channels:requestedOutNumChannels));
	c->channels       = av_get_channel_layout_nb_channels(c->channel_layout);
	//c->bit_rate = 64000;
	c->frame_size = 1024; //TODO: really???
	file_num_channels = c->channels;
	file_sample_rate = c->sample_rate;
	
	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		die("could not open codec");
	}
	
	
	
	if( c->frame_size == 0 ){
		cerr << "warning: frame size is 0, setting to 1024 manually again" << endl;
		c->frame_size = 1024;
	}
	
	av_dump_format(ofmt_ctx, 0, filename.c_str(), 1);
	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			die("Could not open output file");
		}
	}
	
	/* init muxer, write output file header */
	//TODO: put metadata here!
	AVDictionary *d = NULL;
	map<string,string>::iterator it = meta.begin();
	while( it != meta.end() ){
		av_dict_set(&d, (*it).first.c_str(), (*it).second.c_str(), 0);
		++it;
	}


	ofmt_ctx->metadata = d;
	ret = avformat_write_header(ofmt_ctx, NULL);
	// av_dict_free(&d);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		return ret;
	}
	
	/* frame containing input raw audio */
	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate audio frame\n");
		exit(1);
	}
	
	frame->nb_samples     = c->frame_size;
	frame->format         = c->sample_fmt;
	frame->channel_layout = c->channel_layout;
	
	/* the codec gives us the frame size, in samples,
	 * we calculate the size of the samples buffer in bytes */
	buffer_size = av_samples_get_buffer_size(NULL, c->channels, c->frame_size,
											 c->sample_fmt, 0);
	if (buffer_size < 0) {
		char error[512];
		av_make_error_string(error, 512, buffer_size);
		fprintf(stderr, "Could not get sample buffer size: %s\n", error );
		die("could not open sample buffer size");
	}
	samples = (uint16_t*)av_malloc(buffer_size);
	if (!samples) {
		fprintf(stderr, "Could not allocate %d bytes for samples buffer\n",
				buffer_size);
		exit(1);
	}
	/* setup the data pointers in the AVFrame */
	ret = avcodec_fill_audio_frame(frame, c->channels, c->sample_fmt,
								   (const uint8_t*)samples, buffer_size, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not setup audio frame\n");
		exit(1);
	}
	
	frame_pointer = 0;
	
	swr_context = swr_alloc_set_opts(NULL,
									 c->channel_layout, c->sample_fmt, c->sample_rate,
									 in_channel_layout, AV_SAMPLE_FMT_FLT, in_sample_rate,
									 0, NULL);
	swr_init(swr_context);
	
	if (!swr_context){
		die("Could not allocate resampler context\n");
	}
}

void ofxAvAudioWriter::write(float *src, int numFrames){
	if( !c || !out_stream || !frame ) return;
	/* encode a single tone sound */
	while( numFrames > 0 ){
		
		if( frame_pointer == 0 ){
			av_init_packet(pkt);
			pkt->data = NULL; // packet data will be allocated by the encoder
			pkt->size = 0;
		}
		
		int to_copy = FFMIN(frame_pointer+numFrames, c->frame_size )-frame_pointer;
		
		//memcpy(samples+sizeof(float)*frame_pointer*in_num_channels, src, to_copy);
		/* resample to desired rate */ //TODO: sizeof(uint16_t) depends on format!!!!
		uint8_t * sw_dest = (uint8_t*)(samples+frame_pointer*in_num_channels);
		const uint8_t * sw_src = (uint8_t*)src;
		int samples_converted = swr_convert(swr_context,
											&sw_dest, to_copy,
											&sw_src, to_copy);

		src += to_copy*in_num_channels;
		frame_pointer += numFrames;
		numFrames -= to_copy;
		
		/* encode the samples */
		if( frame_pointer >= c->frame_size ){
			int ret = avcodec_encode_audio2(c, pkt, frame, &got_output);
			if (ret < 0) {
				fprintf(stderr, "Error encoding audio frame\n");
				return;
			}
			
			frame_pointer = 0;
			
			if (got_output) {
				//fwrite(pkt->data, 1, pkt->size, f);
				ret = av_write_frame(ofmt_ctx, pkt);
				av_free_packet(pkt);
			}
		}
	}
}


void ofxAvAudioWriter::close(){
	if( swr_context && pkt && out_stream && samples && frame ){
		if( frame_pointer > 0 ){
			cerr << "adding final " << frame_pointer << " frames" << endl;
			frame->nb_samples = frame_pointer;
			int ret = avcodec_encode_audio2(c, pkt, frame, &got_output);
			if (ret < 0) {
				fprintf(stderr, "Error encoding audio frame\n");
				return;
			}
			
			frame_pointer = 0;
			
			if (got_output) {
				//fwrite(pkt->data, 1, pkt->size, f);
				cout << "got special packet with " << pkt->size << " bytes" << endl;
				ret = av_write_frame(ofmt_ctx, pkt);
				av_free_packet(pkt);
			}
		}
		/* write the delayed frames */
		while(got_output){
			int ret = avcodec_encode_audio2(c, pkt, NULL, &got_output);
			if (ret < 0) {
				fprintf(stderr, "Error encoding frame\n");
				goto bye;
			}
			
			if (got_output) {
				// fwrite(pkt->data, 1, pkt->size, f);
				ret = av_write_frame(ofmt_ctx, pkt);
				if( ret < 0 ){
					cerr << "error write: " << ret << endl;
				}
				av_free_packet(pkt);
			}
		}

	}
	
	av_write_trailer(ofmt_ctx);

bye:
	if( swr_context ){
		swr_free(&swr_context);
		swr_context = NULL;
	}
	
	if( pkt ){
		av_free_packet(pkt);
		// pkt = NULL;
	}

	if( samples ){
		av_freep(&samples);
		samples = NULL;
	}
	
	if( frame ){
		av_frame_free(&frame);
		frame = NULL;
	}

	if( ofmt_ctx ){
		//avcodec_close(ofmt_ctx->streams[0]->codec);
		avformat_free_context(ofmt_ctx);
		c = NULL;
		ofmt_ctx = NULL;
	}
	
	/*if( c ){
		avcodec_close(c);
		av_free(c);
		c = NULL;
	}*/
}




AVCodecID ofxAvAudioWriter::codecForExtension( std::string ext ){
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	if( ext == "wav" ) return AV_CODEC_ID_PCM_S16LE;
	else if( ext == "ogg" ) return AV_CODEC_ID_VORBIS;
	else return AV_CODEC_ID_NONE;
}



bool ofxAvAudioWriter::updateMetadata(std::string filename, std::map<std::string, std::string> newMetadata){
	string fileNameAbs = ofToDataPath(filename,true);
	const char * input_filename = fileNameAbs.c_str();
	AVFormatContext* container = 0;
	if (avformat_open_input(&container, input_filename, NULL, NULL) < 0) {
		cerr << ("Could not open file") << endl;
		return false;
	}
 
	if (avformat_find_stream_info(container,NULL) < 0) {
		cerr << ("Could not find file info") << endl;
		return false;
	}
 
	int audio_stream_id = -1;
 
	// To find the first audio stream. This process may not be necessary
	// if you can gurarantee that the container contains only the desired
	// audio stream
	int i;
	for (i = 0; i < container->nb_streams; i++) {
		if (container->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_stream_id = i;
			break;
		}
	}
 
	if (audio_stream_id == -1) {
		cerr << ("Could not find an audio stream") << endl;
		return false;
	}
 
	// Find the apropriate codec and open it
	AVCodecContext * codec_context = container->streams[audio_stream_id]->codec;
 
	AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
	if (avcodec_open2(codec_context, codec,NULL)) {
		cerr << ("Could not find open the needed codec") << endl;
		return false;
	}
	
	// from here on it's mostly following
	// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c
	FILE * f = fopen(input_filename, "rb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", input_filename);
		return false;
	}
	
	
	// ---------------------------------------------
	// OK OK OK
	// WE HAVE AN OPEN FILE!!!!
	// now let's create the output file ...
	// ---------------------------------------------
	
	AVFormatContext *ofmt_ctx = NULL;
	// todo: improve this crap!
	string destFile = ofToDataPath("__" +filename);
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, destFile.c_str());
	if (!ofmt_ctx) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return false;
	}
	
	/* find the encoder */
	AVCodec * out_codec = avcodec_find_encoder(codec->id);
	if (!out_codec) {
		cerr << ("codec not found") << endl;
		return false;
	}
	
	AVStream * out_stream = avformat_new_stream(ofmt_ctx, out_codec);
	if (!out_stream) {
		cerr << ("Failed allocating output stream\n") << endl;
		return false;
	}
	
	AVCodecContext * out_c = out_stream->codec;
	// old!
	if (!out_c) {
		cerr << ("could not allocate audio codec context") << endl;
		return false;
	}
	
	/* check that the encoder supports s16 pcm input */
	// TODO: find sample fmt automatically !
	out_c->sample_fmt = AV_SAMPLE_FMT_S16;
	/* select other audio parameters supported by the encoder */
	out_c->sample_rate = codec_context->sample_rate;
	out_c->channel_layout = codec_context->channel_layout;
	out_c->channels       = codec_context->channels;
	out_c->frame_size = codec_context->frame_size;
	
	/* open it */
	if (avcodec_open2(out_c, out_codec, NULL) < 0) {
		cerr << ("could not open codec") << endl;
	}
	
	
	
	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		int ret = avio_open(&ofmt_ctx->pb, destFile.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			cerr << ("Could not open output file") << endl;
		}
	}
	
	/* init muxer, write output file header */
	//TODO: put metadata here!
	AVDictionary *d = NULL;
	map<string,string>::iterator it = newMetadata.begin();
	while( it != newMetadata.end() ){
		av_dict_set(&d, (*it).first.c_str(), (*it).second.c_str(), 0);
		++it;
	}
	
	
	ofmt_ctx->metadata = d;
	int ret = avformat_write_header(ofmt_ctx, NULL);
	// av_dict_free(&d);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		return ret;
	}
	
	
	
	// ---------------------------------------------
	// OK, We have all we need, really. let's start this party!
	// ---------------------------------------------

	//packet.data = inbuf;
	//packet.size = fread(inbuf, 1, AVCODEC_AUDIO_INBUF_SIZE, f);
	AVPacket * packet = new AVPacket();
	av_init_packet(packet);
	packet->data = NULL;
	packet->size = 0;
	
	// read a packet (frame? who the fuck knows)

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
	
	//TODO: cleanup!
	return true;
	
die:
	//TODO: release resources
	return false;
}



/* check that a given sample format is supported by the encoder */
static int check_sample_fmt(AVCodec *codec, enum AVSampleFormat sample_fmt)
{
	const enum AVSampleFormat *p = codec->sample_fmts;
	
	while (*p != AV_SAMPLE_FMT_NONE) {
		if (*p == sample_fmt)
			return 1;
		p++;
	}
	return 0;
}

/* just pick the highest supported samplerate */
static int select_sample_rate(AVCodec *codec, int preferred)
{
	const int *p;
	int best_samplerate = 0;
	
	if (!codec->supported_samplerates)
		return 44100;
	
	p = codec->supported_samplerates;
	while (*p) {
		int rate = *p;
		if( rate == preferred ) return preferred;
		best_samplerate = FFMAX(*p, best_samplerate);
		p++;
	}
	return best_samplerate;
}

/* select layout with the highest channel count */
static int select_channel_layout(AVCodec *codec, int preferred)
{
	const uint64_t *p;
	uint64_t best_ch_layout = 0;
	int best_nb_channels   = 0;
	
//	if (!codec->channel_layouts)
//		return AV_CH_LAYOUT_STEREO;
	
	// lets just hope that the codec can do what we want!
	if( !codec->channel_layouts) return preferred;
	
	p = codec->channel_layouts;
	while (*p) {
		int nb_channels = av_get_channel_layout_nb_channels(*p);
		if( nb_channels == preferred ) return *p;
		if (nb_channels > best_nb_channels) {
			best_ch_layout    = *p;
			best_nb_channels = nb_channels;
		}
		p++;
	}
	return best_ch_layout;
}

