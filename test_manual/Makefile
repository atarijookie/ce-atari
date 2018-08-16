TARGET    = tst_manu.prg

CC        = m68k-atari-mint-gcc
STRIP     = m68k-atari-mint-strip
PRGFLAGS  = m68k-atari-mint-flags
CFLAGS    = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030 = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS   = -Wl,--traditional-format -lgcc
LDLIBS   = ../ce_hdd_if/ce_hdd_if.a -lgcc

HDR       = 
CSRC      = main.c 

CFLAGS  += -nostdlib
STARTUP = startup.s

OBJS    = $(patsubst %.c,%.o,$(CSRC))
OBJS030 = 

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(OBJS030) $(LDFLAGS) $(LDLIBS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

%.o: %.c 
	@echo [Compiling 68k code] $(@:.o=.c)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
