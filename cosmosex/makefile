TARGET	= cosmosex

CC		= g++
STRIP	= strip
CFLAGS	= -Wall
LDFLAGS	= -lbcm2835 -lrt -lpthread

HDR		=	global.h acsidatatrans.h conspi.h datatypes.h isettingsuser.h settings.h settingsreloadproxy.h \
			ccorethread.h gpio.h utils.h devfinder.h devchangeshandler.h debug.h mounter.h \
			config/configcomponent.h config/configstream.h config/keys.h config/netsettings.h \
			native/imagefilemedia.h native/imedia.h native/nomedia.h native/scsi.h native/scsi_defs.h native/testmedia.h native/devicemedia.h \
			translated/gemdos.h translated/gemdos_errno.h translated/translateddisk.h translated/dirtranslator.h translated/filenameshortener.h
			
OBJS	=	acsidatatrans.o conspi.o main.o settings.o settingsreloadproxy.o ccorethread.o gpio.o \
			utils.o devfinder.o debug.o mounter.o \
			config/configcomponent.o config/configstream_general.o config/configstream_screens.o config/netsettings.o \
			native/imagefilemedia.o native/nomedia.o native/scsi.o native/testmedia.o native/devicemedia.o \
			translated/translateddisk_gemdos.o translated/translateddisk_general.o translated/dirtranslator.o translated/filenameshortener.o

SUBDIR1	=	config
SUBDIR2	=	native
SUBDIR3	=	translated
			
all:	cosmosex

$(TARGET): $(OBJS) $(HDR)
	$(MAKE) -C $(SUBDIR1)
	$(MAKE) -C $(SUBDIR2)
	$(MAKE) -C $(SUBDIR3)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
#	$(STRIP) -s $@ 

%.o: %.cpp 
	g++ $(CFLAGS) -c $<

clean:
	$(MAKE) -C $(SUBDIR1) clean
	$(MAKE) -C $(SUBDIR2) clean
	$(MAKE) -C $(SUBDIR3) clean
	rm -f *.o cosmosex
	
