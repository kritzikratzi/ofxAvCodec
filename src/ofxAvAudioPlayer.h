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
	
	
	// call this first after create the player
	bool setupAudioOut( int numChannels, int sampleRate );
	// call this from the audioOut callback.
	void audioOut( float * output, int bufferSize, int nChannels );
	
	// ok, so ... it _would_ be nice if this was mostly compatible with the OF sound players.
	// for now i've copied everything in here, if things go well
	// i (or you) can implement one after the other.
	
	/// \brief Tells the sound player which file to play.
	///
	/// Codec support varies by platform but wav, aif, and mp3 are safe.
	///
	/// \param fileName Path to the sound file, relative to your app's data folder.
	/// \param stream set "true" to enable streaming from disk (for large files).
	// hansi: currently stream is always yes
	bool loadSound(std::string fileName, bool stream = true);
	
	/// \brief Stops and unloads the current sound.
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
	//void setPan(float pan);
	
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
	void setPositionMS(int ms);
	
	/// \brief Gets position of the playhead.
	/// \return playhead position in milliseconds.
	int getPositionMS();
	
	/// \brief Gets position of the playhead (0..1).
	/// \return playhead position as a float between 0 and 1.
	float getPosition();
	
	/// \brief Gets current playback state.
	/// \return true if the player is currently playing a file.
	bool getIsPlaying();
	
	/// \brief Gets playback speed.
	/// \return playback speed (see ofSoundPlayer::setSpeed()).
	//float getSpeed();
	
	/// \brief Gets stereo pan.
	/// \return stereo pan in the range -1 to 1.
	//float getPan();
	
	/// \brief Gets current volume.
	/// \return current volume in the range 0 to 1.
	//float getVolume();
	
	/// \brief Queries the player to see if its file was loaded successfully.
	/// \return whether or not the player is ready to begin playback.
	//bool isLoaded();
	
	bool isLoaded;
	bool isPlaying;
	bool isLooping; 
	unsigned long long duration;
	float volume; 

	std::string getMetadata( std::string key );
	std::map<std::string,std::string> getMetadata();

	static std::map<std::string,std::string> readMetadata( std::string filename );
	
private:
	bool decode_next_frame();
	unsigned long long av_time_to_millis( int64_t av_time ); 
	int64_t millis_to_av_time( unsigned long long ms );
	
	// i think these could be useful public, rarely, but still ...
	AVPacket * packet;
	int packet_data_size;
	int buffer_size; 
	uint8_t inbuf[AVCODEC_AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
	int len;
	int audio_stream_id;
	
	FILE *f;
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
};

#endif
