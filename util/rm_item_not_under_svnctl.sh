#!/bin/sh

function Usage { echo "Usage: $0 folder ..."; echo "only svn folder is supported"; exit 1;}

if [ $# -eq 0 ]
then
	Usage;
fi

for dir in $*
do
	if [ ! -d $dir ]
	then
		echo "$dir don't exist or is not folder";
		Usage;
	fi
done


for dir in $*
do
	if [ -d $dir ]
	then
		echo "clean svn $dir"
		svn st --no-ignore $1 | awk '/^[\?I]/ { print $2}' | xargs -r rm -r && \
		echo "success ..." || echo "failed !!!"
	fi
done
