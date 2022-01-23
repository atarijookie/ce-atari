#!/bin/sh

if [ ! -f /etc/issue ] ; then
    echo "unknown"
    exit
fi

release=$( lsb_release -a 2>/dev/null )
stretch=$( echo $release | grep -o stretch | wc -l)
jessie=$( echo $release | grep -o jessie | wc -l)

if [ "$stretch" -gt "0" ]; then
    echo "stretch"
    exit
fi

if [ "$jessie" -gt "0" ]; then
    echo "jessie"
    exit
fi

echo "unknown"

