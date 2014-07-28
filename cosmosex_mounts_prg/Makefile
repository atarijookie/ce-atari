TARGET	= ce_mount.prg
SLIM	= yes

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format

HDR	= acsi.h translated.h
CSRC	= acsi.c main.c 
SSRC	= 

ifeq ($(SLIM),yes)
	CFLAGS	+= -nostdlib -DSLIM
	STARTUP	= startup.s
	CSRC	+= stdlib.c
	HDR	+= stdlib.h
endif

OBJS = $(patsubst %.c,%.o,$(CSRC))

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
