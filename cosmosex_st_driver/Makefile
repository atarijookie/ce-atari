TARGET	= gemdos.prg
SLIM	= no

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar

HDR	= xbra.h
CSRC	= gemdos.c xbra.c acsi.c
SSRC	= gemdos_asm.S

ifeq ($(SLIM),yes)
CFLAGS	+= -nostdlib -DSLIM
STARTUP	= startup.S
endif

$(TARGET): $(STARTUP) $(CSRC) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(CSRC) $(SSRC)
	$(STRIP) -s $@

clean:
	rm -f $(TARGET) *~
