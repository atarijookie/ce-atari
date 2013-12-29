TARGET	= bootsect.prg
SLIM	= yes

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -mpcrel 
LDFLAGS	= -Wl,--traditional-format

HDR	= 
CSRC	= 
SSRC	= bootsector.s

ifeq ($(SLIM),yes)
CFLAGS	+= -nostdlib -DSLIM
#STARTUP	= startup.s
endif

$(TARGET): $(STARTUP) $(CSRC) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(CSRC) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@

clean:
	rm -f $(TARGET) *~
