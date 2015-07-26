#! /bin/bash
#

if [ -d work ]
then
	echo "Work directory exists. Deleting directory `pwd`/work"
	read -p "Are you sure? [Y/n] " -n 1 -r
	echo    # (optional) move to a new line
	if [[ $REPLY =~ ^[Nn]$ ]]
	then
	    exit
	else
		echo "Cleaning up ..."
		rm -rf work
		mkdir work
	fi
else
	echo Creating work directory ...
	mkdir work
fi

	

echo Zipping up the ffmpeg source ...
pushd ../ffmpeg_src/ffmpeg >/dev/null
commit=$(git rev-parse HEAD)
git archive --format=tar --prefix=ffmpeg/ HEAD | gzip >../../libs/work/ffmpeg-src-$commit.tgz
popd >/dev/null

echo Copying library files ... 
cp -R avcodec work/avcodec

echo Copying readme ...
cp -R readme.md work/

echo Zipping it all up ...
pushd work >/dev/null
zip -r ../libs-${commit}.zip * >/dev/null
popd >/dev/null


size=`du -h work | tail -n 1 | cut -f 1`
zipSize=`du -h libs-${commit}.zip | tail -n 1 | cut -f 1`

echo 
echo Done! 
echo   Unpacked size: $size
echo   Zipped size: $zipSize