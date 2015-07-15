--------
MAC OS X 
--------
please update and push this file if it didn't work for you ... 

roughly follow the guide from 
https://trac.ffmpeg.org/wiki/CompilationGuide/MacOSX


1. install some thingies with brew
----------------------------------

brew install automake fdk-aac git lame libass libtool libvorbis libvpx \
opus sdl shtool texi2html theora wget x264 xvid yasm

2. configure and make
----------------------------------
./configure --cc=/usr/bin/clang --prefix=/opt/ffmpeg --as=yasm --extra-version=tessus --enable-gpl --enable-libmp3lame  --enable-libtheora --enable-libvorbis --enable-libvpx --enable-libx264 --enable-version3 --disable-ffplay --disable-indev=qtkit --disable-indev=x11grab_xcb