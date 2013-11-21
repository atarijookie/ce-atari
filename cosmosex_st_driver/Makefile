TARGET	= cedd.prg
SLIM	= no

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format

HDR		= xbra.h acsi.h gemdos.h gemdos_errno.h translated.h main.h
CSRC	= gemdos.c xbra.c acsi.c main.c
SSRC	= gemdos_asm.S

ifeq ($(SLIM),yes)
CFLAGS	+= -nostdlib -DSLIM
STARTUP	= startup.S
endif

$(TARGET): $(STARTUP) $(CSRC) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(CSRC) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@

clean:
	rm -f $(TARGET) *~
