//
//  ofxAvAudioWriter.h
//  emptyExample
//
//  Created by Hansi on 13.09.15.
//
//

#ifndef __emptyExample__ofxAvAudioWriter__
#define __emptyExample__ofxAvAudioWriter__

#include <string>
#include <iostream>

#include <math.h>
#include <map>

extern "C"{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavformat/avformat.h>
	#include <libavutil/channel_layout.h>
	#include <libavutil/samplefmt.h>
	#include <libswresample/swresample.h>
}

class ofxAvAudioWriter{
public: 
	ofxAvAudioWriter();
	~ofxAvAudioWriter();
	
	// let the writer know which data you will feed into it
	void setup( int sampleRate, int numChannels );
	
	// sets the title that will be used in the file
	void addMeta( std::string key, std::string value );
	
	// open an output file.
	// formatExtension is wav or ogg
	bool open( std::string filename, std::string formatExtension, int sampleRate = -1, int numChannels = -1);
	
	// opens an output file.
	// sample rate and num channels is optional.
	//   if provided libavcodec will select a best match, depending on what the codec supports
	//   if not provided libavcodec will select a best match, depending on the data provided to setup()
	// either way automatic resampling will take place.
	bool open( std::string filename, AVCodecID av_codec_id, int requestedOutputSampleRate = -1, int requestedOutputNumChannels = -1 );
	
	// add samples
	// samples must be numFrames*numChannels long and interleaved
	// ie, numFrames = 4, numChannels = 2, samples = {l1, r1, l2, r2, l3, r3, l4, r4}
	void write( float * samples, int numFrames );
	
	void close();
	
	// sample rate and num channels used by the codec
	int file_sample_rate;
	int file_num_channels;
	int file_codec;
	
	// sample rate and num channels provided to setup
	int in_sample_rate;
	int in_num_channels;
	int in_channel_layout;
	std::map<std::string,std::string> meta;
	
	// tries to find a codec id from a file extension
	static AVCodecID codecForExtension( std::string extension );
	
private:
	SwrContext * swr_context;
	AVFormatContext *ofmt_ctx;
	AVStream *out_stream;
	uint16_t *samples;
	int buffer_size;

	AVCodec *codec;
	AVCodecContext *c;
	AVFrame *frame;
	AVPacket *pkt;
	
	int frame_pointer;
	int got_output;
	
};
#endif /* defined(__emptyExample__ofxAvAudioWriter__) */
