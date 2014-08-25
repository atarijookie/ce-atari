TARGET	= rw_user.prg

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -m68000 -fomit-frame-pointer -Wall -Wno-multichar -O2

HDR		= acsi.h
OBJS	= acsi.o rw_user.o
SSRC	= 

$(TARGET): $(STARTUP) $(OBJS) $(SSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(SSRC)
	$(STRIP) -s $@

%.o: %.c 
	$(CC) $(CFLAGS) -c $<
	
clean:
	rm -f $(TARGET) *~ *.o
