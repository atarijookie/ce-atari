TARGET	= ce_sting.prg
SLIM	= yes

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib 
LDFLAGS	= -Wl,--traditional-format -Wl,-Map=output.map -lgcc

HDR		= acsi.h stdlib.h setup.h tcp.h con_man.h icmp.h api.h port.h tpl_middle.h 
CSRC	= acsi.c stdlib.c setup.c tcp.c con_man.c icmp.c api.c port.c tpl_middle.c main.c 
SSRC	= vbl.s tpl_middle.s
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

	