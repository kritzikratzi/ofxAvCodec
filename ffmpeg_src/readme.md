Useful infos for compiling FFmpeg/libavcodec
===

About 
---

This guide explains how to compile the shared libraries needed for ofxAvCodec V0.3. 
It features: 

* FFmpeg 3.2.2, compiled as a shared library (LGPL)
* Minimaly modified Cisco openh264 ~jan 2017 (BSD)


I have attempted my best to be as thorough as possible, but that doesn't mean following 
the guide on your system will yield precicesly the same result. 
Please read all instructions carefully and be prepared to make small adjustments during 
the build process.  

If you would like to avoid this alltogether, you can use a [binary release](https://github.com/kritzikratzi/ofxAvCodec/releases) instead.  

Links
---

* The [compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide) official contains instructions for virtually all platforms. 
* Shared libraries for Windows: [http://ffmpeg.zeranoe.com/builds/](http://ffmpeg.zeranoe.com/builds/)
* Git repo with full FFmpeg source: [https://github.com/kritzikratzi/FFmpeg/tree/openh264_hq](https://github.com/kritzikratzi/FFmpeg/tree/openh264_hq) (forked from [https://github.com/FFmpeg/FFmpeg](https://github.com/FFmpeg/FFmpeg) )
* FFmpeg version used: 3.2.2 with adjustments for higher quality h264 output [https://github.com/kritzikratzi/FFmpeg/commit/de0e273de33f1c213a5060157a176296e2fd2f11](https://github.com/kritzikratzi/FFmpeg/commit/de0e273de33f1c213a5060157a176296e2fd2f11)
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
	mkdir dist
	cd $openh264_src_dir
	sudo mv /usr/bin/nasm /usr/bin/nasm-osx
	brew install nasm
	
	# Let's make a universal lib (we need it later!)
	make clean && make OS=darwin ARCH=x86_64
	cp libopenh264.dylib dist/libopenh264-x86_64.dylib

	make clean && make OS=darwin ARCH=i386
	cp libopenh264.dylib dist/libopenh264-i386.dylib

	make clean && make OS=darwin ARCH=arm64
	cp libopenh264.dylib dist/libopenh264-arm64.dylib

	# create a fat lib
	lipo dist/libopenh264-arm64.dylib dist/libopenh264-i386.dylib dist/libopenh264-x86_64.dylib -output dist/libopenh264.dylib -create
	# make install path relative
	install_name_tool -id @executable_path/libopenh264.dylib libopenh264.dylib
	
	# 2a Build 64bit arm
	cd $openh264_src_dir
	make OS=darwin ARCH=arm64 install
	cd $ffmpeg_src_dir
	make clean
	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared  --shlibdir="@executable_path" --shlibdir="@executable_path" --disable-indevs --enable-libopenh264 --arch=arm64
	make && make install
	mv @executable_path libs_arm64
	
	# 2b Build 64bit intel
	cd $openh264_src_dir
	make OS=darwin ARCH=x86_64 install
	cd $ffmpeg_src_dir
	make clean
	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared  --shlibdir="@executable_path" --shlibdir="@executable_path" --disable-indevs --enable-libopenh264 --arch=x86_64
	make && make install
	mv @executable_path libs_x86_64
	
	# 2c. Build 32 bit intel
	cd $openh264_src_dir
	make OS=darwin ARCH=i386 clean install
	make clean
	./configure  --prefix=`pwd`/dist/ --enable-pic --enable-shared  --shlibdir="@executable_path" --shlibdir="@executable_path" --disable-indevs --extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --enable-libopenh264 --arch=i386
	make && make install
	mv @executable_path libs_i386

	# 3. Combine dylibs into fat libs and copy over symlinks
	mkdir libs
	for file in libs_x86_64/*.dylib
	do
		f=$(basename $file)
		if [ -h $file ];then;cp -av $file libs
		else;lipo libs_i386/$f libs_arm64/$f $file -output libs/$f -create
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
	rm -rf libs_i386 libs_x86_64 libs_arm64
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



Compiling on Ubuntu with GCC [new, incomplete]
---

	# First install the necessary dependencies and sources
	sudo apt-get install nasm yasm pkg-config gcc-multilib g++-multilib chrpath rsync
	cd ffmpeg_src
	git clone https://github.com/cisco/openh264.git
	cd openh264
	git checkout 47aedcf80821507029adf3adbd809aa8ee55f3b0

	# Build openh264 32bit and 64bit
 	make -j8 ARCH=x86_64
 	make install ARCH=x86_64 PREFIX=`pwd`/../libs_64
	make clean
 	make -j8 ARCH=x86
 	make install ARCH=x86 PREFIX=`pwd`/../libs_32


	# On to the fun part, let's build ffmpeg fpr 32bit and 64 bit
	cd ..
	git clone https://github.com/kritzikratzi/FFmpeg
	cd FFmpeg
	git checkout de0e273de33f1c213a5060157a176296e2fd2f11

	PKG_CONFIG_PATH=`pwd`/../libs_64/lib/pkgconfig ./configure --enable-shared --disable-static --enable-libopenh264 --disable-indevs --extra-cflags="-m64" --extra-cxxflags="-m64" --extra-ldflags="-m64" --enable-rpath --arch=x86_64 --prefix=`pwd`/../libs_64
	make -j8 && make install
	cd ../libs_64/lib
	chrpath -r '$ORIGIN' *.so
	cd ../../
	mkdir -p ../libs/avcodec/lib/linux64
	rsync -am --include='*.so' --include='*.so.*' --exclude='*' "libs_64/lib/" "../libs/avcodec/lib/linux64/"
	cd FFmpeg

	make clean

	PKG_CONFIG_PATH=`pwd`/../libs_32/lib/pkgconfig ./configure --enable-shared --disable-static --enable-libopenh264 --disable-indevs --extra-cflags="-m32" --extra-cxxflags="-m32" --extra-ldflags="-m32" --enable-rpath --arch=i386 --prefix=`pwd`/../libs_32
	make -j8 && make install
	cd ../libs_32/lib
	chrpath -r '$ORIGIN' *.so
	cd ../../
	mkdir -p ../libs/avcodec/lib/linux32
	rsync -am --include='*.so' --include='*.so.*' --exclude='*' "libs_64/lib/" "../libs/avcodec/lib/linux32/";
	cd FFmpeg

	# you should be in ffmpeg_src now
	mkdir -p 

Wonderful! Now copy the stuff to `addons/ofxAvCodec/libs/avcodec/lib/linux` and `addons/ofxAvCodec/libs/avcodec/lib/linux64`. Skip the `pkg-config` folder and all `.a` files!

