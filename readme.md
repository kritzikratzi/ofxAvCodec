ofxAvCodec
===


**Warning**  This is the danger zone. There is currently zero support for this. I don't have time to fix bugs. Just putting this out there because it could be useful to a lot of people. 

So... what is it? <br>
A small wrapper around libavcodec - the magic library that is painful to use, but basically the only cross platform way to read and write in many different media formats. 

Advantages over the built in soundStreamPlayers: it gives you access to the raw buffer data. (well, semiraw. there is some resampling done first). 


Example
---

	class ofApp : public ofBaseApp{
		// ...
		ofSoundStream soundStream;
		ofxAvAudioPlayer player;
		void audioOut( float * output, int bufferSize, int nChannels );
	};
	
	//--------------------------------------------------------------
	void ofApp::setup(){
		soundStream.setup(this, 2, 0, 44100, 512, 4);
		player.setupAudioOut(2, 44100); // required to resample properly
		player.loadSound(ofToDataPath("testo.flac"));
	}

	void ofApp::audioOut( float * output, int bufferSize, int nChannels ){
		player.audioOut(output, bufferSize, nChannels); 
	}



License
---
libavcodec comes with a GPL/LGPL license. For convenience the precompiled binaries are included (compiled as shared libs, gpl plugins not enabled). I hope i made no mistake, ffmpeg maintains a hall of shame and i don't want to end up there. To be safe you could make your own build. To do so follow the notes in ffmpeg_src/readme.txt. 


The license of this plugin itself is, as always, the WTFPL. 