#!/bin/sh

# First count the lines of /etc/issue which contain word Yocto in it.
# (Was done by comparing part of the string to Yocto, but this substring comparing worked differently on Yocto and Raspbian.)
issue=$( cat /etc/issue | grep -o "Yocto" | wc -l )

# If at least once the Yocto was found, it's Yocto
if [ "$issue" -gt "0" ]; then
	echo "yocto"
else
	echo "raspbian"
fi

