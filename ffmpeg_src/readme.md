Useful infos for compiling FFmpeg/libavcodec
===

About 
---

This guide explains how to compile the shared libraries needed for ofxAvCodec V0.2. 
It features: 

* FFmpeg 3.2.2, compiled as a shared library (LGPL)
* Cisco openh264 ~jan 2017 (BSD)


I have attempted my best to be as thorough as possible, but that doesn't mean following 
the guide on your system will yield precicesly the same result. 
Please read all instructions carefully and be prepared to make small adjustments during 
the build process.  

If you would like to avoid this alltogether, you can use a [binary release](https://github.com/kritzikratzi/ofxAvCodec/releases) instead.  


Links
---

* The [compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide) official contains instructions for virtually all platforms. 
* Shared libraries for Windows: [http://ffmpeg.zeranoe.com/builds/](http://ffmpeg.zeranoe.com/builds/)
* Git repo with full source: git://source.ffmpeg.org/ffmpeg.git or from [https://github.com/FFmpeg/FFmpeg](https://github.com/FFmpeg/FFmpeg)
* FFmpeg version used: 3.2.2 [https://github.com/FFmpeg/FFmpeg/commit/148c4fb8d203fdef8589ccef56a995724938918b](https://github.com/FFmpeg/FFmpeg/commit/148c4fb8d203fdef8589ccef56a995724938918b)
* OpenH264 version used: [https://github.com/cisco/openh264/commit/47aedcf80821507029adf3adbd809aa8ee55f3b0](https://github.com/cisco/openh264/commit/47aedcf80821507029adf3adbd809aa8ee55f3b0)
* Lots of information about when which codec is built and which configure flags are required: https://www.ffmpeg.org/ffmpeg-codecs.html

Command Line
---

* ./configure --help will list a lot of options and their current values (eg. --enable-libgsm          enable GSM de/encoding via libgsm [no])
* `ffmpeg -codecs` to list of installed codecs
* `./configure --list-decoders` to list available decoders
* `./configure --list-encoders` to list available encoders

Compiling on Mac OS
---
The to create a universal binary run the following commands in the ffmpeg source directory: 

	# 1. Prepare openh264
	cd $openh264_src_dir
	sudo mv /usr/bin/nasm /usr/bin/nasm-osx
	brew install nasm
	
	# Let's make a universal lib (we need it later!)
	make clean && make OS=darwin ARCH=x86_64
	mkdir dist
	cp libopenh264.dylib dist/libopenh264-64bit.dylib
	make clean && make OS=darwin ARCH=i386
	cp libopenh264.dylib dist/libopenh264-32bit.dylib
	# create a fat lib
	lipo dist/libopenh264-32bit.dylib dist/libopenh264-64bit.dylib -output dist/libopenh264.dylib -create
	# make install path relative
	install_name_tool -id @executable_path/libopenh264.dylib libopenh264.dylib
	
	# 2. Build 64 bit
	cd $openh264_src_dir
	make OS=darwin ARCH=x86_64 install
	cd $ffmpeg_src_dir
	make clean
	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared  --shlibdir="@executable_path" --shlibdir="@executable_path" --disable-indevs --enable-libopenh264
	make && make install
	mv @executable_path libs_64
	
	# 3. Build 32 bit
	cd $openh264_src_dir
	make OS=darwin ARCH=i386 clean install
	make clean
	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared  --shlibdir="@executable_path" --shlibdir="@executable_path" --disable-indevs --extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --enable-libopenh264 --arch=i386
	make && make install
	mv @executable_path libs_32

	# 4. Combine dylibs into fat libs and copy over symlinks
	mkdir libs
	for file in libs_64/*.dylib
	do
		f=$(basename $file)
		if [ -h $file ];then;cp -av $file libs
		else;lipo libs_32/$f $file -output libs/$f -create
		fi
	done
	
	#5. replace the h264 dylib path
	cd libs
	for file in *.dylib
	do
		oldPath=$(otool -L $file | grep libopenh264 | cut -f 2 | cut -d " " -f 1 | tail -n 1)
		newPath="@executable_path/libopenh264.dylib"
		if [ !  -z $oldPath ] && [ $oldPath != $newPath ]
		then
			echo "$(basename $file)\n old: $oldPath\n new: $newPath"
			install_name_tool -change "$oldPath" "$newPath" "$file"
		fi
	done
	
	# 5. Clean up a bit to be ready for the next build ... 
	rm -rf libs_32 libs_64
	mv libs libs-$(git rev-parse HEAD)

Done! Now copy the include dir to ofxAvCodec/libs/avcodec/include and the libs dir to ofxAvCodec/libs/avcodec/lib/osx

Btw: Directly after running configure you get a neat list of all enabled components like codecs, muxer, and other things I've never heard of. 


|Flag|Description|
|----|-----------|
|``--prefix=`pwd`/dist/``|Sets the output path to the dist folder|
|`--enable-pic`|not sure|
|`--enable-shared`|compile as shared libraries (disables static libs)|
|`--shlibdir="@executable_path"`|tells each dylib to look for other dylibs in the same folder (i think)|
|`--disable-indevs`|Disables input devices like qtkit. I added this flag to get rid of the shared lib dependency to jack audio|
|`--enable-libopenh264`|enable+link against openh264|
|`--extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --arch=i386`|Create 32 bit binaries|


	
* Copy only the dylibs to libs/avcodec/lib/osx (i copied the symlinks too, but not they're needed)
* Copy the header directory to libs/avcodec/include
* Make sure to keep the file libs/avcodec/include/libavutil/inttypes.h
  It's required for VS2012. This file is licensed as new bsd license. Included from [https://code.google.com/p/msinttypes/](https://code.google.com/p/msinttypes/). In the same folder in common.h replace `#include <inttypes.h>` with `#include "inttypes.h"`. 




Compiling for Windows using Msys2
---

Prerequisites: Install Msys2 and Visual Studio 2015. 

We will compile with MSVC as a compiler, and Msys2 to run the build scripts/have support for pkg-config.
Because of this, getting a build console is slightly funky: 

	# Start a "cmd.exe" window and set up environment variables
	# You might have to adjust your visual studio path
	"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
	# Now move into the msys-64 shell: 
	export PATH="C:\Program Files (x86)\Windows Kits\8.1\bin\x64":$PATH
	c:\msys64\msys2_shell.bat -full-path

	# now the msys64 shell will open. 
	# make sure the visual studio compiler works by typing:  
	cl

	# In msys 64 shell: (close the old cmd window, we don't need it anymore)
	# 1. Install build packages (i might be missing a few?)
	pacman -S yasm
	pacman -S pacman -S mingw-w64-x86_64-pkg-config
	pacman -S pkg-config
	pacman -S nasm

	# 2. Build + Install openh264
	cd $my_openh264_src_dir
	git clone https://github.com/cisco/openh264.git
	cd openh264
 	make ARCH=x86_64 OS=msvc
 	make install ARCH=x86_64 OS=msvc PREFIX=/mingw64
 	# instead of the prefix you can do: 
	#export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig/":$PKG_CONFIG_PATH

	# verify that this is now installed: 
	# expected output: -L/usr/local/lib -lopenh264
 	pkg-config --libs openh264

 	# 3. Build+ Install ffmpeg 
 	cd $my_ffmpeg_src_dir
 	./configure --toolchain=msvc --enable-shared --disable-static --enable-libopenh264 --disable-indevs --prefix=`pwd`/dist64

 	make -j 8
 	# if you get a linker error at this point: 
 	# on my system user32.dll wasn't linked, 
 	# a workaround is to edit config.mak, add user32.lib to LDFLAGS and then 
 	# run make -j 8 again. 

 	make install

Great! Now we do the same thing for 32 bit libs: 

	# Start again with a fresh cmd prompt: 
	"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
	# Move into msys32
	start "C:\msys64\msys2_shell.cmd" -mingw32 -full-path

	# 2. Build + Install openh264 (x86)
	cd $my_openh264_src_dir
 	make ARCH=x86 OS=msvc
 	make install ARCH=x86 OS=msvc PREFIX=/mingw32

	# verify that this is now installed: 
	# expected output: -L/usr/local/lib -lopenh264
 	pkg-config --libs openh264

 	# 3. Build ffmpeg
 	./configure --toolchain=msvc --enable-shared --disable-static --enable-libopenh264 --disable-indevs --prefix=`pwd`/dist32

Compiling for Linux using Ubuntu [outdated]
---

This is directly taken from the [https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu](FFmpeg guide for Ubuntu). The following steps assume a 64bit Ubuntu installation. 


	sudo apt-get update
	
	sudo apt-get -y --force-yes install git autoconf automake build-essential libass-dev \
		libfreetype6-dev libsdl1.2-dev libtheora-dev libtool libva-dev libvdpau-dev\
		libvorbis-dev libxcb1-dev libxcb-shm0-dev libxcb-xfixes0-dev pkg-config \
		texi2html zlib1g-dev gcc-multilib libc6-i386
	
	git clone git://source.ffmpeg.org/ffmpeg.git
	cd ffmpeg
	# check out specific revision if you want
	./configure  --prefix=`pwd`/dist/x86_64/ --enable-pic --enable-shared
	make && make install
	
	# now cross compile for 32 bit
	make clean 
	./configure  --prefix=`pwd`/dist/i386/ --enable-pic --enable-shared --extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --arch=i386
	make && make install
	
	
Good job! If all goes well you have two directories: `dist/x86_64` and `dist/i386`