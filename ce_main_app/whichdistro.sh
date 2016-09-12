#!/bin/sh

issue=$( cat /etc/issue )
if [[ $issue == *"Yocto"* ]]; then
	echo "yocto"
else
	echo "raspbian"
fi

