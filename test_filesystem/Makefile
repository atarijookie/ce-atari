TARGET	= tst_fsys.prg
SLIM	= yes

CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -nostdlib 
LDFLAGS	= -Wl,--traditional-format
LDLIBS	= -lgcc
DEPFLAGS= -MM -MT $(patsubst %.d,%.o,$@) -MT $@

HDR		= stdlib.h out.h
CSRC	= stdlib.c out.c main.c test01.c test02.c test03.c test04.c test05.c
SSRC	= 
STARTUP	= startup.s

OBJS = $(CSRC:.c=.o) $(SSRC:.s=.o)
DEPS = $(OBJS:.o=.d)

$(TARGET): $(STARTUP) $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

depend:	$(DEPS)

%.o: %.c %.d

%.d:	%.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $^

clean:	depclean
	$(RM) $(TARGET) *~ *.o

depclean:
	$(RM) $(DEPS)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),depclean)
-include $(DEPS)
endif
endif
