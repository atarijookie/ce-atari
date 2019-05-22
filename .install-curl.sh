#! /bin/sh

VERSION=7.65.0
CURL="curl-${VERSION}"
CURLARCHIVE="${CURL}.tar.bz2"
URL="https://curl.haxx.se/download/${CURLARCHIVE}"

if [ -z "${HOST}" ] ; then
  HOST=arm-linux-gnueabihf
fi
if [ -z "${MAKE}" ] ; then
  MAKE=make
fi

wget ${URL} || exit 1
tar xjf ${CURLARCHIVE} || exit 1
cd ${CURL} || exit 1
CC="${HOST}-gcc" ./configure --prefix=/usr/${HOST} --host=${HOST} --disable-shared --disable-manual || exit 1
${MAKE} -j3 || exit 1
sudo ${MAKE} install || exit 1

exit 0
