#!/bin/sh

buildareapath="/tmp/cebuild"

#-------------------------------------------------------------
# create one symlink
if [ "$1" == "symlink" ]; then
	wd=$( pwd )
	infile=$( echo $2 | sed "s|./|$wd/|" )
	outfile=$( echo $2 | sed "s|./|$buildareapath/|" )
	outdir=$( dirname "$outfile" )
	
	if [ ! -d "$outdir" ]; then
		mkdir -p "$outdir" 
	fi
		
	echo "symlink: $infile -> $outfile" 
	ln -s "$infile" "$outfile"
	exit
fi

#-------------------------------------------------------------
# create all symlinks in tmp
if [ -z "$1"]; then
	make clean

	rm -rf $buildareapath
	mkdir -p $buildareapath

	echo "Creating symlinks..."

	find . -type f -exec $0 symlink {} \;
	echo "Done."
	exit
fi

#-------------------------------------------------------------
