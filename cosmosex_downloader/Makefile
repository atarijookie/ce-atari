TARGET	= ce_dwnld.prg
SLIM	= yes

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format

HDR	= acsi.h main.h
CSRC	= acsi.c main.c 
SSRC	= 

ifeq ($(SLIM),yes)
CFLAGS	+= -nostdlib -DSLIM
STARTUP	= startup.s
CSRC	+= stdlib.c
HDR	+= stdlib.h
endif

$(TARGET): $(STARTUP) $(CSRC) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(CSRC) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@

clean:
	rm -f $(TARGET) *~
