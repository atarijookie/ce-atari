TARGET	= ce_fdd.prg

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format -lgem -lgcc

HDR	= $(wildcard *.h)
CSRC	= $(wildcard *.c) 

OBJS    = $(patsubst %.c,%.o,$(CRCS))

CFLAGS	+= -nostdlib -DSLIM 
STARTUP	 = startup.s

OBJS	 = $(patsubst %.c,%.o,$(CSRC))

$(TARGET): $(STARTUP) $(OBJS) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(LDFLAGS)
	$(STRIP) -s $@

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
