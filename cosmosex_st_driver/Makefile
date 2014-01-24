TARGET	= cedd.prg
SLIM	= yes

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -mpcrel

HDR	= xbra.h acsi.h gemdos.h bios.h gemdos_errno.h translated.h main.h extern_vars.h
#CSRC	= gemdos.c gemdos_rw.c gemdos_pexec.c bios.c xbra.c acsi.c main.c 
OBJS	= gemdos.o gemdos_rw.o gemdos_pexec.o bios.o xbra.o acsi.o main.o 
SSRC	= gemdos_asm.s bios_asm.s

ifeq ($(SLIM),yes)
CFLAGS	+= -nostdlib -DSLIM
STARTUP	= startup.s
#CSRC	+= stdlib.c
OBJS	+= stdlib.o
HDR	+= stdlib.h
endif

#$(TARGET): $(STARTUP) $(CSRC) $(SSRC) $(HDR)
$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC)
	$(STRIP) -s $@

%.o: %.c 
	$(CC) -c $<
	
clean:
	rm -f $(TARGET) *~ *.o
