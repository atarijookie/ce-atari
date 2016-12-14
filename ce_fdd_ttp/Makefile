TARGET	= ce_fdd.ttp

CC          = m68k-atari-mint-gcc
STRIP	    = m68k-atari-mint-strip
PRGFLAGS    = m68k-atari-mint-flags
CFLAGS      = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030   = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar

LDFLAGS	= -Wl,--traditional-format -lgcc

HDR     = $(wildcard *.h)
CSRC	= $(wildcard *.c) 

OBJS    = $(patsubst %.c,%.o,$(CRCS))

CFLAGS	+= -nostdlib -DSLIM 
STARTUP	 = startup.s

OBJS	 = $(patsubst %.c,%.o,$(CSRC))
OBJS030 = cache030.o

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(OBJS030) $(LDFLAGS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload
	
cache030.o: cache030.s
	@echo [Compiling 030 code] $(@:.o=.c)
	$(CC) $(CFLAGS030) -c $< -o $@    
    
%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
