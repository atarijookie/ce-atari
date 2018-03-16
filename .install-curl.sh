#! /bin/sh

VERSION=7.59.0
CURL="curl-${VERSION}"
CURLARCHIVE="${CURL}.tar.bz2"
URL="https://curl.haxx.se/download/${CURLARCHIVE}"

if [ -z "${HOST}" ] ; then
  HOST=arm-linux-gnueabihf
fi

wget ${URL} || exit 1
tar xjf ${CURLARCHIVE} || exit 1
cd ${CURL} || exit 1
CC="${HOST}-gcc" ./configure --prefix=/usr/${HOST} --host=${HOST} --disable-shared --disable-manual || exit 1
make || exit 1
sudo make install || exit 1

exit 0
