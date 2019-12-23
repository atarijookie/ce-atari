#! /bin/sh

VERSION=1.59
BCM="bcm2835-${VERSION}"
BCMARCHIVE="${BCM}.tar.gz"
URL="http://www.airspayce.com/mikem/bcm2835/${BCMARCHIVE}"

if [ -z "${HOST}" ] ; then
  HOST=arm-linux-gnueabihf
fi

wget ${URL} || exit 1
tar xzf ${BCMARCHIVE} || exit 1
cd ${BCM} || exit 1
CC="${HOST}-gcc" ./configure --prefix=/usr/${HOST} --host=${HOST} || exit 1
make || exit 1
sudo make install || exit 1

exit 0
