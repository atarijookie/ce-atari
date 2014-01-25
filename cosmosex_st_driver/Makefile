TARGET	= cedd.prg
SLIM	= yes

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -m68000 -fomit-frame-pointer -Wall -Wno-multichar -mpcrel
#-O2

HDR	= xbra.h acsi.h gemdos.h bios.h gemdos_errno.h translated.h main.h extern_vars.h helpers.h
OBJS	= gemdos_filefuncs.o gemdos_findfuncs.o gemdos_pathfuncs.o gemdos_rw.o gemdos_pexec.o bios.o xbra.o acsi.o main.o 
SSRC	= gemdos_asm.s bios_asm.s

ifeq ($(SLIM),yes)
CFLAGS	+= -nostdlib -DSLIM
STARTUP	= startup.s
OBJS	+= stdlib.o
HDR	+= stdlib.h
endif

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC)
	$(STRIP) -s $@

%.o: %.c 
	$(CC) $(CFLAGS) -c $<
	
clean:
	rm -f $(TARGET) *~ *.o
