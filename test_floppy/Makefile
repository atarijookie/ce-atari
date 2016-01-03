TARGET	= ce_tstfd.prg

CC       = m68k-atari-mint-gcc
STRIP    = m68k-atari-mint-strip
PRGFLAGS = m68k-atari-mint-flags
CFLAGS    = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030 = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS  = -Wl,--traditional-format -lgcc

CFLAGS  += -nostdlib -DSLIM
HDR      = stdlib.h
CSRC     = stdlib.c main.c 
STARTUP  = startup.s
SSRC     = 

OBJS = $(patsubst %.c,%.o,$(CSRC))

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
