TARGET	= ce_conf.prg

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format -lgcc

HDR		= acsi.h scsi.h                  find_ce.h keys.h stdlib.h
CSRC	= acsi.c scsi_falcon.c scsi_tt.c find_ce.c main.c stdlib.c

CFLAGS	+= -nostdlib
STARTUP	= startup.s

OBJS = $(patsubst %.c,%.o,$(CSRC))

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *.o
