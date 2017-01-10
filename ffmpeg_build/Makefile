# Download FFmpeg sources, unpack, build and install
# then build binary package

URLGET = $(shell which wget || echo "`which curl` -O")
DISTRO = $(shell if grep Yocto /etc/issue > /dev/null ; then \
           echo "$$( grep Yocto /etc/issue | cut -f 1 -d ' ' )-Yocto" ; else \
           echo Raspbian ; fi | tr A-Z a-z )

BIN_PACKAGE = ffmpeg-$(VERSION_FFMPEG)-$(DISTRO)-bin.tar.bz2

CONF_FLAGS_FFMPEG = --enable-gpl --disable-ffprobe --disable-ffserver \
                    --disable-debug --disable-doc
CONF_FLAGS_FFMPEG += --disable-libxcb --disable-xlib

VERSION_FFMPEG = 3.2.2
ARCHIVE_FFMPEG = ffmpeg-$(VERSION_FFMPEG).tar.bz2

VERSION_CURL = 7.52.1
ARCHIVE_CURL = curl-$(VERSION_CURL).tar.bz2


all:	$(BIN_PACKAGE)

$(BIN_PACKAGE):	ffmpeg.build
	cd / ; tar cjf $(PWD)/$@ /usr/local/bin/ffmpeg

ffmpeg.build:	ffmpeg-$(VERSION_FFMPEG)
	mkdir -p ffmpeg-build
	cd ffmpeg-build ; $(PWD)/$</configure $(CONF_FLAGS_FFMPEG)
	cd ffmpeg-build ; $(MAKE) && $(MAKE) install-progs
	touch $@

curl.build:	curl-$(VERSION_CURL)
	cd $< ; ./configure && $(MAKE) && $(MAKE) install
	touch $@

$(ARCHIVE_FFMPEG):
	$(URLGET) http://ffmpeg.org/releases/$@

ffmpeg-$(VERSION_FFMPEG):	$(ARCHIVE_FFMPEG)
	tar xjf $<
	touch $@

$(ARCHIVE_CURL):
	#$(URLGET) https://curl.haxx.se/download/$@
	$(URLGET) http://dl.uxnr.de/mirror/curl/$@

curl-$(VERSION_CURL):	$(ARCHIVE_CURL)
	tar xjf $<
	touch $@
