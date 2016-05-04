TARGET	= tst_fsys.prg
SLIM	= yes

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib 
LDFLAGS	= -Wl,--traditional-format
LDLIBS	= -lgcc

HDR		= stdlib.h out.h
CSRC	= stdlib.c out.c main.c test01.c test02.c test03.c test04.c test05.c
SSRC	= 
STARTUP	= startup.s

OBJS = $(CSRC:.c=.o) $(SSRC:.s=.o)

$(TARGET): $(STARTUP) $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

clean:
	$(RM) $(TARGET) *~ *.o
