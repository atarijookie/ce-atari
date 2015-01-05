TARGET	= ce_dd.prg
SLIM	= yes

#ONPC    = yes

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -fcall-saved-a1 -fcall-saved-d1
LDFLAGS	= -Wl,--traditional-format -lgcc 

HDR		= xbra.h acsi.h gemdos.h bios.h gemdos_errno.h translated.h main.h
CSRC	= gemdos.c gemdos_rw.c gemdos_pexec.c bios.c xbra.c acsi.c main.c 
SSRC	= gemdos_asm.s bios_asm.s

ifeq ($(SLIM),yes)
CFLAGS	+= -nostdlib -DSLIM
STARTUP	= startup.s
CSRC	+= stdlib.c
HDR		+= stdlib.h
endif

ifeq ($(ONPC),yes)
CFLAGS	+= -DONPC
endif


OBJS = $(patsubst %.c,%.o,$(CSRC))

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload
	
%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
