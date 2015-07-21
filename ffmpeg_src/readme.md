Useful infos for compiling ffmpeg/libavcodec
===


Links
---

* The [compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide) official contains instructions for virtually all platforms. 
* Shared libraries for Windows: [http://ffmpeg.zeranoe.com/builds/](http://ffmpeg.zeranoe.com/builds/)
* Git repo with full source: git://source.ffmpeg.org/ffmpeg.git or from [https://github.com/FFmpeg/FFmpeg](https://github.com/FFmpeg/FFmpeg)
* Included binaries were built from commit [https://github.com/FFmpeg/FFmpeg/commit/d5fcca83b915df9536d595a1a44c24294b606836](https://github.com/FFmpeg/FFmpeg/commit/d5fcca83b915df9536d595a1a44c24294b606836)
* Lots of information about when which codec is built and which configure flags are required: https://www.ffmpeg.org/ffmpeg-codecs.html

Command Line
---

* ./configure --help will list a lot of options and their current values (eg. --enable-libgsm          enable GSM de/encoding via libgsm [no])
* `ffmpeg -codecs` to list of installed codecs
* `./configure --list-decoders` to list available decoders
* `./configure --list-encoders` to list available encoders


Compiling on Mac OS
---
The included binaries were compiled with 

	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared
	
This will create 64bit libraries. 
For 32bit add these flags (if you built for 64bit earlier you must run a `make clean` first): 

	--extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --arch=i386
	
	
Btw: Directly after running configure you get a neat list of all enabled components like codecs, muxer, and other things I've never heard of. 


	make && make install
	
Hurray, now you have all you need in the dist folder! 

* Copy only the dylibs to libs/avcodec/lib/osx (i copied the symlinks too, but not they're needed)
* Copy the header directory to libs/avcodec/include
* Make sure to keep the file libs/avcodec/include/libavutil/inttypes.h
  It's required for VS2012. This file is licensed as new bsd license. Included from [https://code.google.com/p/msinttypes/](https://code.google.com/p/msinttypes/). In the same folder in common.h replace `#include <inttypes.h>` with `#include "inttypes.h"`. 




Compiling for Windows using Mac OS
---
Compiled using the build scripts from [https://github.com/rdp/ffmpeg-windows-build-helpers](https://github.com/rdp/ffmpeg-windows-build-helpers). A great little script that sets up mingw and generates shared libraries. Create a directory `ffmpeg_src/win/` and run 
	
	wget https://raw.github.com/rdp/ffmpeg-windows-build-helpers/master/cross_compile_ffmpeg.sh -O cross_compile_ffmpeg.sh
	./cross_compile_ffmpeg.sh --build-ffmpeg-shared=y --build-ffmpeg-static=n
	
To the question: 
`Would you like to include non-free (non GPL compatible) libraries`, answer no (N). 

The resulting dll files are in <br>
`sandbox/x86_64/ffmpeg_git_shared.installed/bin/` <br>
`ffmpeg_git_shared.installed/bin/`



Special Licensing Considerations
---
You can enable aac with the configuration option `--enable-nonfree`. FFmpeg has the following to say about this: 

	The Fraunhofer AAC library, FAAC and aacplus are under licenses which
	are incompatible with the GPLv2 and v3. We do not know for certain if their
	licenses are compatible with the LGPL.
	If you wish to enable these libraries, pass `--enable-nonfree` to configure.
	But note that if you enable any of these libraries the resulting binary will
	be under a complex license mix that is more restrictive than the LGPL and that
	may result in additional obligations. It is possible that these
	restrictions cause the resulting binary to be unredistributeable.

To enable certain video codecs (like x264) the `--enable-gpl` flag can be added. This means your application will have to be released under the GPL as well. 