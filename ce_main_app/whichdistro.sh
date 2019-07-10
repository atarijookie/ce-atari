#!/bin/sh

if [ ! -f /etc/issue ] ; then
    echo "unknown"
    exit
fi

# First count the lines of /etc/issue which contain word Yocto in it.
# (Was done by comparing part of the string to Yocto, but this substring comparing worked differently on Yocto and Raspbian.)
issue=$( cat /etc/issue | grep -o "Yocto" | wc -l )

# If at least once the Yocto was found, it's Yocto
if [ "$issue" -gt "0" ]; then
    echo "yocto"
else
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
fi

