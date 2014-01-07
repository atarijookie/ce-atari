TARGET	= cosmosex

CC		= g++
STRIP	= strip
CFLAGS	= -Wall
LDFLAGS	= -lbcm2385

all:	cosmosex

$(TARGET): $(CSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(CSRC) $(LDFLAGS)
	$(STRIP) -s $@ 
	
HDR		=	global.h acsidatatrans.h conspi.h datatypes.h isettingsuser.h settings.h settingsreloadproxy.h ccorethread.h \
			config/configcomponent.h config/configstream.h config/keys.h \
			native/datamedia.h native/imedia.h native/nomedia.h native/scsi.h native/scsi_defs.h native/testmedia.h \
			translated/gemdos.h translated/gemdos_errno.h translated/translateddisk.h
			
CSRC	=	acsidatatrans.cpp conspi.cpp main.cpp settings.cpp settingsreloadproxy.cpp ccorethread.cpp \
			config/configcomponent.cpp config/configstream_general.cpp config/configstream_screens.cpp \
			native/datamedia.cpp native/nomedia.cpp native/scsi.cpp native/testmedia.cpp \
			translated/translateddisk_gemdos.cpp translated/translateddisk_general.cpp


clean:
	rm -rf *.o cosmosex
	
	





