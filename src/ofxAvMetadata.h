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
	
	
	
};

#endif /* defined(__emptyExample__ofxAvMetadata__) */
