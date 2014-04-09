TARGET	= ce_fdd.prg

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format -lgem 

HDR		= $(wildcard *.h)
CSRC	= $(wildcard *.c) 
SSRC	= $(wildcard *.s) 

OBJS    = $(patsubst %.c,%.o,$(CRCS))

CFLAGS	+= -nostdlib -DSLIM 

OBJS = $(patsubst %.c,%.o,$(CSRC))

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
