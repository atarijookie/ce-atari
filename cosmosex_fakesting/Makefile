TARGET	= ce_sting.prg

AS        = m68k-atari-mint-as
CC        = m68k-atari-mint-gcc
STRIP     = m68k-atari-mint-strip
PRGFLAGS  = m68k-atari-mint-flags
CFLAGS    = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib -DSLIM # -DDEBUG_STRING
CFLAGS030 = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib -DSLIM
ASFLAGS   = -m68000
ASFLAGS030= -m68030
LDFLAGS   = -Wl,--traditional-format
LDLIBS    = -lgcc

HDR     = $(wildcard *.h)
CSRC    = $(wildcard *.c)
SSRC    = tpl_middle_jump.s vbl.s
STARTUP = startup.s

OBJS = $(CSRC:.c=.o) $(SSRC:.s=.o)
OBJS030 = cache030.o

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030)
	@echo [Compiling final app] $(@:.o=.c)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

%030.o: %030.s
	$(AS) $(ASFLAGS030) -c -o $@ $<


clean:
	rm -f $(TARGET) *~ *.o

	