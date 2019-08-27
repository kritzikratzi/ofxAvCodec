//
//  AvCodecAudioPlayer.h
//  Oscilloscope
//
//  Created by Hansi on 22.06.15.
//
//

#ifndef Oscilloscope_AvCodecAudioPlayer_h
#define Oscilloscope_AvCodecAudioPlayer_h

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

//TODO: should/can we move this to cpp file?
//ok, be careful with these.
//with flac files written by audacity it's actually quite easy to cause serious read troubles
//when using frame=192k, inbuf=20480, thres=4096
#define AVCODEC_MAX_AUDIO_FRAME_SIZE (192000)
#define AVCODEC_AUDIO_INBUF_SIZE (20480)
#define AVCODEC_AUDIO_REFILL_THRESH (4096*3)

class ofxAvAudioPlayer{
public: 
	
	ofxAvAudioPlayer();
	~ofxAvAudioPlayer(); 
	
	// call this first after create the player
	bool setupAudioOut( int numChannels, int sampleRate );
	// call this from the audioOut callback.
	// returns the number of frames (0...bufferSize) that were played. 
	int audioOut( float * output, int bufferSize, int nChannels );
	
	/// \brief Tells the sound player which file to play.
	///
	/// Codec support is ginormous. this can even load most movie files and extract the audio.
	///
	/// \param fileName Path to the sound file, relative to your app's data folder.
	/// \param "stream" is always true, the argument is here only for compatibility with OFs built in sound player
	bool load(std::string fileName, bool stream = true);
	
	/// \brief Stops and unloads the current sound.
	void unload();
	
	bool loadSound(std::string fileName, bool stream = true);
	void unloadSound();
	
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
	void setPositionMS(unsigned long long ms);
	
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
	unsigned long long getDurationMs();

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
	bool forceNativeFormat; 
	
	/// \brief return a metadata item. available after load()
	/// \return the string contents of the metadata item
	std::string getMetadata( std::string key );
	
	/// \brief return all metadata. available after load()
	/// \return a map containing all metadata
	std::map<std::string,std::string> getMetadata();
	
	int getAudioOutNumChannels();
	int getAudioOutSampleRate();

private:
	unsigned long long duration;
	float volume;
	bool decode_next_frame();
	
	bool fileLoaded;
	bool isPlaying;
	bool isLooping;
	
	unsigned long long av_time_to_millis( int64_t av_time );
	int64_t millis_to_av_time( unsigned long long ms );
	
	// i think these could be useful public, rarely, but still ...
	AVPacket packet;
	int packet_data_size;
	int buffer_size; 
	uint8_t inbuf[AVCODEC_AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	int len;
	int audio_stream_id;
	
	// contains audio data, usually decoded as non-interleaved float array
	AVFrame *decoded_frame;
	AVCodecContext* codec_context;
	AVFormatContext* container; 

	SwrContext * swr_context;
	int output_sample_rate;
	int64_t output_channel_layout;
	int output_num_channels;
	
	int64_t next_seekTarget;
	
	// contains audio data, always in interleaved float format
	int decoded_buffer_pos;
	int decoded_buffer_len;
	float decoded_buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE];
	
	bool output_config_changed;
};

#endif
