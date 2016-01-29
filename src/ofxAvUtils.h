//
//  ofxAvUtils.h
//  emptyExample
//
//  Created by Hansi on 16.09.15.
//
//

#ifndef __emptyExample__ofxAvUtils__
#define __emptyExample__ofxAvUtils__

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
class ofxAvUtils{
public:
	
	// make sure ffmpeg is initialized
	static void init(); 
	
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
	// filename: yep, the name of the file
	// resolution: number of datapoints (ie. sizeof of the returned array)
	// durationInSeconds (optional): if you know the length of the file in advance, 
	static float * waveform( std::string filename, int resolution, float durationInSeconds = -1 );
	
	// amplitude preview as polyline
	// resolution: number of datapoints in x direction
	// meshWidth: pixel width of target mesh
	// meshHeight: pixel height of target mesh
	// fixedDurationInSeconds (optional): pretends the file is this length in seconds, no matter how long the file really is. (important if multiple files are displayed next to each other). 
	// the data will be plotted around meshHeight/2
	static ofPolyline waveformAsPolyline( std::string filename, int resolution, float meshWidth = 1, float meshHeight = 1, float fixedDuratioInSeconds = -1 );
	
	// same as waveformAsPath, but returns a mesh
	// converts polyline result to a mesh (thread safe!)
	static ofMesh waveformAsMesh( std::string filename, int resolution, float meshWidth = 1, float meshHeight = 1, float fixedDuratioInSeconds = -1 );
};

#endif /* defined(__emptyExample__ofxAvUtils__) */
