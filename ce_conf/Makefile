TARGET	= ce_conf.prg

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
CFLAGS		= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030	= -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format -lgcc

HDR		= acsi.h scsi.h                  hdd_if.h find_ce.h keys.h stdlib.h
CSRC	= acsi.c scsi_falcon.c scsi_tt.c hdd_if.c find_ce.c main.c stdlib.c

CFLAGS	+= -nostdlib
STARTUP	= startup.s

OBJS = $(patsubst %.c,%.o,$(CSRC))
OBJS030	= cache030.o

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(OBJS030) $(LDFLAGS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

cache030.o: cache030.s
	@echo [Compiling 030 code] $(@:.o=.c)
	$(CC) $(CFLAGS030) -c $< -o $@

%.o: %.c 
	@echo [Compiling 68k code] $(@:.o=.c)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
