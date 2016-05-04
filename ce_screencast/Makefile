TARGET	= ce_cast.prg

CC	= m68k-atari-mint-gcc
AS	= m68k-atari-mint-as
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags 
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format -lgem

HDR	= $(wildcard *.h)
CSRC	= $(wildcard *.c) 
SSRC	= screenirq.s cache030.s
STARTUP	= startup.s

OBJS    = $(patsubst %.c,%.o,$(CRCS))

CFLAGS	+= -nostdlib -DSLIM 

OBJS	 = $(patsubst %.c,%.o,$(CSRC)) screenirq.o cache030.o

$(TARGET): $(STARTUP) $(OBJS) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(LDFLAGS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
