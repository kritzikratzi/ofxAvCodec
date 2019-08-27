//
//  AvCodecVideoPlayer.h
//
//  The video player always uses a thread to fetch the data.
//  You have to decide for on of two time stepping mechanisms:
//  1. Synchronize by audio -- calling videoPlayer.audioOut(...) is enough
//  2. Synchronize by video -- call videoPlayer.update(ofGetLastFrameTime()).
//
//  You have to use one of the two methods, never both!
//
//  Created by Hansi on 30.04.16.
//

#ifndef Oscilloscope_AvCodecVideoPlayer_h
#define Oscilloscope_AvCodecVideoPlayer_h

#include <string>
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>

#include <math.h>
#include <map>
#include "ofMain.h"

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#define AVCODEC_MAX_AUDIO_FRAME_SIZE (192000)
#define AVCODEC_AUDIO_INBUF_SIZE (20480)
#define AVCODEC_AUDIO_REFILL_THRESH (4096*3)

class ofxAvAudioData;
class ofxAvVideoData;
class ofxAvVideoPlayer{
public:
	
	
	ofxAvVideoPlayer();
	~ofxAvVideoPlayer();
	
	// call this first after create the player
	bool setupAudioOut( int numChannels, int sampleRate );
	// call this from the audioOut callback.
	// returns the number of frames (0...bufferSize) that were played.
	struct AudioResult{
		int numFrames; // = numSamples/nChannels. number of audio frames that was written
		int64_t pts; // presentation timestamp in sample frames
		double t; // timecode in seconds
	};
	AudioResult audioOut( float * output, int bufferSize, int nChannels );
	
	/// \brief Tells the sound player which file to play.
	///
	/// Codec support is ginormous. this can even load most movie files and extract the audio.
	///
	/// \param fileName Path to the sound file, relative to your app's data folder.
	/// \param "stream" is always true, the argument is here only for compatibility with OFs built in sound player
	bool load(std::string fileName, bool stream = true);
	
	/// \brief Stops and unloads the current sound.
	void unload();
	
	/// \brief Starts playback.
	void play();
	
	/// \brief Stops playback.
	void stop();
	
	/// \brief Sets playback volume.
	/// \param vol range is 0 to 1.
	void setVolume(float vol);
	
	/// \brief Sets stereo pan.
	/// \param pan range is -1 to 1 (-1 is full left, 1 is full right).
	// void setPan(float pan);
	
	/// \brief Sets playback speed.
	/// \param speed set > 1 for faster playback, < 1 for slower playback.
	//void setSpeed(float speed);
	
	/// \brief Enables pause / resume.
	/// \param paused "true" to pause, "false" to resume.
	void setPaused(bool paused);
	
	/// \brief Sets whether to loop once the end of the file is reached.
	/// \param loop "true" to loop, default is false.
	void setLoop(bool loop);
	
	/// \brief Enables playing multiple simultaneous copies of the sound.
	/// \param multiplay "true" to enable, default is false.
	//void setMultiPlay(bool multiplay);
	
	/// \brief Sets position of the playhead within the file (aka "seeking").
	/// \param percent range is 0 (beginning of file) to 1 (end of file).
	void setPosition(float percent);
	
	/// \brief Sets position of the playhead within the file (aka "seeking").
	/// \param ms number of milliseconds from the start of the file.
	void setPositionMS(long long ms);
	
	/// \brief Gets position of the playhead.
	/// \return playhead position in milliseconds.
	int getPositionMS();
	
	/// \brief Gets position of the playhead (0..1).
	/// \return playhead position as a float between 0 and 1.
	float getPosition();
	
	/// \brief Gets current playback state.
	/// \return true if the player is currently playing a file.
	bool getIsPlaying();
	
	/// \brief Gets duration in
	/// \return true if the player is currently playing a file.
	long long getDurationMs();
	
	/// \brief Gets playback speed.
	/// \return playback speed (see ofSoundPlayer::setSpeed()).
	//float getSpeed();
	
	/// \brief Gets stereo pan.
	/// \return stereo pan in the range -1 to 1.
	//float getPan();
	
	/// \brief Gets current volume.
	/// \return current volume in the range 0 to 1.
	float getVolume();
	
	/// \brief Queries the player to see if its file was loaded successfully.
	/// \return whether or not the player is ready to begin playback.
	bool isLoaded();
	
	
	// by default resampling is taken care of automatically (set it with setupAudioOut())
	// with this you can disable the sampling and force the file's native data format.
	// set it before .load
	bool forceNativeAudioFormat = false;
	
	/// \brief return a metadata item. available after load()
	/// \return the string contents of the metadata item
	std::string getMetadata( std::string key );
	
	/// \brief return all metadata. available after load()
	/// \return a map containing all metadata
	std::map<std::string,std::string> getMetadata();
	
	
	// \brief returns the video texture
	ofTexture &getTexture();
	const ofPixels & getPixels();
	ofPixels getPixelsForFrameIfAvailable(int frameNum);
	
	int getFrameNumber();
	double getFps(); 
	string getFile();
	
	// \brief updates the video textures
	void update();
	
	string getInfo();

    int getCurrentFrame();
    int getTotalNumFrames();

    void    firstFrame();
    void    nextFrame();
    void    previousFrame();

    float   getHeight() const;
    float   getWidth() const;

    void    draw(float x, float y, float w, float h);
    void    draw(float x, float y);

	AVCodecID getVideoCodec();
	
	bool hasAudioTrack(); 
	
private:
	long long duration;
	float volume;
	bool decode_next_frame();
	bool decode_video_frame( int & got_frame, bool maySkip = true );
	bool decode_audio_frame( int & got_frame );
	bool decode_until( double t, double & decoded_t );
	bool queue_decoded_video_frame_vlocked();
	bool queue_decoded_audio_frame_alocked();
	bool copy_to_pixels_vlocked( ofPixels & video_pixels, ofxAvVideoData * data );
	ofxAvVideoData * video_data_for_time_vlocked( double t );
	
	bool fileLoaded;
	bool wantsUnload; 
	bool isPlaying;
	bool isLooping;
	
	long long av_time_to_millis( int64_t av_time );
	int64_t millis_to_av_time( long long ms );
	
	// i think these could be useful public, rarely, but still ...
	AVPacket packet;
	int packet_data_size;
	int buffer_size;
	uint8_t inbuf[AVCODEC_AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	int len;
	
	// contains audio data, usually decoded as non-interleaved float array
	AVFrame *decoded_frame;
	AVCodecContext* video_context;
	AVCodecContext* audio_context;
	AVFormatContext* fmt_ctx;
	AVStream * video_stream;
	AVStream * audio_stream;
	AVPixelFormat pix_fmt;
	int video_stream_idx;
	int audio_stream_idx; 
	
	SwsContext * sws_context; // context for video resampling/color conversion

	SwrContext * swr_context; // context for audio resampling/channel conversion
	int output_sample_rate;
	int64_t output_channel_layout;
	int output_num_channels;
	bool output_setup_called;
	
	bool requestedSeek; 
	int64_t next_seekTarget;
	
	bool output_config_changed;
	
	int width;
	int height;
	
	ofTexture texture;
	int64_t texturePts;
	
	
	mutex video_buffers_mutex;
	// here we store the decoded buffers in native format.
	// most of the time this is some funky yuv frame which is convert
	// and pushed into the texture in update().
	vector<ofxAvVideoData*> video_buffers;
	int video_buffers_pos; // position where we write into the buffer
	ofPixels video_pixels;
	
	mutex audio_queue_mutex;
	queue<ofxAvAudioData*> audio_queue;
	int audio_frames_available; // total num samples still available on audio queue
	
	thread decoderThread;
	void run_decoder();
	
	int64_t last_pts;
	double last_t;
	int64_t decoder_last_audio_pts; // pts to fix resampling issues with the precise audio position
	double decoder_last_video_t;
	
	// currently only active when there is no audio track in the file
	// very experimental, currently disabled in favor of allowSkipFrames
	bool allowWaitForVideo = true; // allow video waiting
	float waitForVideoDelta = 0;
	
	bool allowSkipFrames = true; // allow video frame skipping to catch up (only for the situation when there is no audio stream, but audio output is setup)
	bool requestSkipFrame = false;
	
	bool needsMoreVideo;
	bool want_restart_loop = false; 
	bool restart_loop;
	string fileNameAbs;
	string fileNameBase;
};

#endif
