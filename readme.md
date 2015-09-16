ofxAvCodec
===


**Warning**  This is the danger zone. There is currently zero support for this. I don't have time to fix bugs. Just putting this out there because it could be useful to a lot of people. 

So... what is it? <br>
A small wrapper around libavcodec - the magic library that is painful to use, but basically the only cross platform way to read and write in many different media formats. 

Advantages over the built in soundStreamPlayers: it gives you access to the raw buffer data. (well, semiraw. there is some resampling done first). 

Downsides: licensing is ... complicated. 


Example Read
---


`ofxAvAudioPlayer` can read an audio file and prepare it for playback through openframeworks audio callback. Before loading the sound you can tell the player your preferred number of output channels and samplerate, all the resampling and channel conversions are done automatically. 

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
		map<string,string> metadata = player.getMetadata(); 
	}

	void ofApp::audioOut( float * output, int bufferSize, int nChannels ){
		player.audioOut(output, bufferSize, nChannels); 
	}


Example Write
---
`ofxAvAudioWriter` needs to be set up with a sample rate and number of channels. After that you can open the file and start writing samples into it. Don't forget to close the file when you are done! 

	ofxAvAudioWriter writer;
	writer.setup(44100, 1); // 44.1kHz, 1 channel
	writer.addMeta("title", "my cool song"); 
	writer.open(ofToDataPath("testo.wav"), "wav" );
	
	int N = 100;
	float * buffer = new float[N];
	int num = 0;
	
	// write roughly 3000 samples in chunks of size N
	while( num < 3000 ){
		for( int i = 0; i < N; i++ ){
			buffer[i] = sinf(num*2*3.1415*400/44100.0);
			num ++;
		}
		writer.write(buffer, N);
	}
	cout << "wrote " << num << " samples" << endl;
	writer.close();


Metadata Read/Update
---


	// Read metadata without reading the file
	map<string,string> meta = ofxAvMetadata::read("myfile.wav"); 
	
	// Write metadata without re-encoding the file 
	// this still reads and writes the entire file though! 
	map<string,string> myMeta; 
	myMeta["title"] = "careless whisper"; 
	ofxAvMetadata::update("myfile.wav", myMeta ); 

Windows
---

Currently there is still some manual work involved: 

* Use the project generator and add the ofxAvCodec library. Then open if VS
* Right click your solution, select properties. Make sure to have the *all configurations* option selected. 
* `Configuration > C++ > General > additional include directories`<br>
  Remove the directories include\\*, but keep the include directory itself in the list
* `Configuration > Linker > Extended > SAFESH:NO`<br>
  set to no (if you cant find the option add `/SAFESEH:NO` as a custom linker flag)
* Still in VS, navigate to <br>
  `emptyExample > addons > ofxAvCodec > libs > avcodec`<br>
  `Right click > Add Elements > Add existing elements` <br>
  Add all .lib files from addons/ofxAvCodec/libs/avcodec/lib/win32 (or win64)


License
---
libavcodec comes with a GPL/LGPL license. For convenience the precompiled binaries are included (compiled as shared libs, gpl plugins not enabled). I hope i made no mistake, ffmpeg maintains a hall of shame and i don't want to end up there. To be safe you could make your own build. To do so follow the notes in ffmpeg_src/readme.txt. 


The license of this plugin itself is, as always, the WTFPL. 