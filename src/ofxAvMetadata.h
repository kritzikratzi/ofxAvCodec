//
//  ofxAvMetadata.h
//  emptyExample
//
//  Created by Hansi on 16.09.15.
//
//

#ifndef __emptyExample__ofxAvMetadata__
#define __emptyExample__ofxAvMetadata__

#include <map>
#include <string>
#include "ofPath.h"
extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
class ofxAvMetadata{
public: 
	// read metadata of a file
	static std::map<std::string,std::string> read( std::string filename );
	
	// updates the metadata.
	// the entire file has to be read and written during the process, so this is not fast!
	// however, the file will not be reencoded, so there is no quality loss.
	static bool update( std::string filename, std::map<std::string,std::string> newMetadata );
	
	// file duration in seconds
	static double duration( std::string filename );
	
	// mono amplitude preview
	// you must delete[] this yourself!
	// resolution: number of datapoints (ie. sizeof of the returned array)
	static float * waveform( std::string filename, int resolution );
	
	// amplitude preview as path
	// resolution: number of datapoints in x direction
	// meshWidth: pixel width of target mesh
	// meshHeight: pixel height of target mesh
	// the data will be plotted around meshHeight/2
	static ofPath waveformAsPath( std::string filename, int resolution, float meshWidth = 1, float meshHeight = 1 );
};

#endif /* defined(__emptyExample__ofxAvMetadata__) */
