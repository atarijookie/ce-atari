#!/bin/sh
# vim: tabstop=4 shiftwidth=4
#
# installs ffmpeg binary to /usr/local/bin/ffmpeg

VERSION_FFMPEG=3.2.2
FFMPEG_BINARY=/usr/local/bin/ffmpeg
URLGET=$(which wget)

if [ -f /etc/issue ] && grep Yocto /etc/issue > /dev/null 2>&1 ; then
	DISTRO="$(grep Yocto /etc/issue | cut -f 1 -d ' ' | tr A-Z a-z )-yocto"
else
	DISTRO=raspbian
fi

FFMPEG_PACKAGE=ffmpeg-${VERSION_FFMPEG}-${DISTRO}-bin.tar.bz2
BASEURL=http://nanard.free.fr/CosmosEx/

if [ -f "${FFMPEG_BINARY}" ] && [ -x "${FFMPEG_BINARY}" ] ; then
	INSTALLED_VERSION=$(${FFMPEG_BINARY} -version |head -n 1 | cut -f 3 -d ' ')
	if [ "${INSTALLED_VERSION}" = "${VERSION_FFMPEG}" ] ; then
		echo "FFmpeg version ${INSTALLED_VERSION} is already installed"
		exit 0
	fi
fi

cd /tmp/
${URLGET} ${BASEURL}${FFMPEG_PACKAGE} || exit 1
cd /
tar xjf /tmp/${FFMPEG_PACKAGE} || exit 1
echo "$(${FFMPEG_BINARY} -version |head -n 1 | cut -f 1-3 -d ' ') installed to ${FFMPEG_BINARY}"
