TARGET	= rw_user.prg
SLIM	= yes

CC      = m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib 
LDFLAGS	= -Wl,--traditional-format -lgcc

HDR     = acsi.h
OBJS	= acsi.o rw_user.o
SSRC	= 
STARTUP	= startup.s

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC) $(LDFLAGS) 
	$(STRIP) -s $@

%.o: %.c 
	$(CC) $(CFLAGS) -c $<
	
clean:
	rm -f $(TARGET) *~ *.o
