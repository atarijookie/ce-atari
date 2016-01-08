TARGET	= ce_sting.prg
SLIM	= yes

CC		  = m68k-atari-mint-gcc
STRIP	  = m68k-atari-mint-strip
PRGFLAGS  = m68k-atari-mint-flags
CFLAGS	  = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib # -DDEBUG_STRING
CFLAGS030 = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib # -DDEBUG_STRING
LDFLAGS	  = -Wl,--traditional-format -lgcc

HDR     = $(wildcard *.h)
CSRC    = $(wildcard *.c)
SSRC    = startup.s tpl_middle.s vbl.s

OBJS = $(patsubst %.c,%.o,$(CSRC))
OBJS030 = cache030.o

$(TARGET): $(OBJS) $(OBJS030) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(OBJS030) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload
	
cache030.o: cache030.s
	@echo [Compiling 030 code] $(@:.o=.s)
	$(CC) $(CFLAGS030) -c $< -o $@
    
%.o: %.c 
	@echo [Compiling 68k code] $(@:.o=.c)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o

	