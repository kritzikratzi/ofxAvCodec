Useful infos for compiling ffmpeg/libavcodec
===


Links
---

* The [compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide) official contains instructions for virtually all platforms. 
* Shared libraries for Windows: [http://ffmpeg.zeranoe.com/builds/](http://ffmpeg.zeranoe.com/builds/)
* Git repo: git://source.ffmpeg.org/ffmpeg.git
* Lots of information about when which codec is built and which configure flags are required: https://www.ffmpeg.org/ffmpeg-codecs.html


Command Line
---

* ./configure --help will list a lot of options and their current values (eg. --enable-libgsm          enable GSM de/encoding via libgsm [no])
* `ffmpeg -codecs` to list of installed codecs
* `./configure --list-decoders` to list available decoders
* `./configure --list-encoders` to list available encoders


Configuration flags
---
The included version only doesn't contain gpl codecs. It was compiled with 

	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared --enable-nonfree
	
This will create 64bit libraries. 
For 32bit add these flags (if you built for 64bit earlier you must run a `make clean` first): 

	--extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --arch=i386
	
	
Btw: Directly after running configure you get a neat list of all enabled components like codecs, muxer, and other things I've never heard of. 


	make && make install
	
Hurray, now you have the shared library files in ffmpeg/dist. 
First clear out ofxAvCodec/