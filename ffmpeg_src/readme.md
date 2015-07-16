Useful infos for compiling ffmpeg/libavcodec
===


Links
---

* The [compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide) official contains instructions for virtually all platforms. 
* Shared libraries for Windows: [http://ffmpeg.zeranoe.com/builds/](http://ffmpeg.zeranoe.com/builds/)
* Git repo with full source: git://source.ffmpeg.org/ffmpeg.git
* Included binaries were built from commit d5fcca83b915df9536d595a1a44c24294b606836
* Lots of information about when which codec is built and which configure flags are required: https://www.ffmpeg.org/ffmpeg-codecs.html
* There are flac (and possibly other) bugs when compiling with newer gccs. Use 

Command Line
---

* ./configure --help will list a lot of options and their current values (eg. --enable-libgsm          enable GSM de/encoding via libgsm [no])
* `ffmpeg -codecs` to list of installed codecs
* `./configure --list-decoders` to list available decoders
* `./configure --list-encoders` to list available encoders


Configuration flags
---
The included binaries were compiled with 

	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared --enable-nonfree
	
This will create 64bit libraries. 
For 32bit add these flags (if you built for 64bit earlier you must run a `make clean` first): 

	--extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --arch=i386
	
	
Btw: Directly after running configure you get a neat list of all enabled components like codecs, muxer, and other things I've never heard of. 


	make && make install
	
Hurray, now you have the shared library files! From dist/libs you can now run: in ffmpeg/dist. 
First clear out ofxAvCodec/


Special Licensing Considerations
---
Ok, so libavcodec is this really powerful library. And it's practically unrivaled. However, the icensing is a topic in itself. Currently aac is enabled (using --enable-nonfree), that might have been a mistake and i'll probably have to remove that flag, because their license file mentions: 

	The Fraunhofer AAC library, FAAC and aacplus are under licenses which
	are incompatible with the GPLv2 and v3. We do not know for certain if their
	licenses are compatible with the LGPL.
	If you wish to enable these libraries, pass `--enable-nonfree` to configure.
	But note that if you enable any of these libraries the resulting binary will
	be under a complex license mix that is more restrictive than the LGPL and that
	may result in additional obligations. It is possible that these
	restrictions cause the resulting binary to be unredistributeable.


The full license text can be found in libs/avcodec/LICENSE.md. 
While it is your obligation to bundle that with your application, I have taken the liberty to place a second copy in `data/license-libavcodec.md`. That way (if everything is set up correctly) it should get copied to the resulting data folder of your application automatically. 
