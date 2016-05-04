TARGET     = tst_stng.prg

CC         = m68k-atari-mint-gcc
AS         = m68k-atari-mint-as
STRIP      = m68k-atari-mint-strip
PRGFLAGS   = m68k-atari-mint-flags 
CFLAGS     = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030  = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS    = -Wl,--traditional-format
LDLIBS     = -lgcc
DEPFLAGS   = -MM -MT $(patsubst %.d,%.o,$@) -MT $@


HDR        = $(wildcard *.h)
CSRC       = $(wildcard *.c)
SSR        =
STARTUP    = startup.s

CSRC      := $(filter-out server_main.c,$(CSRC))

OBJS       = $(patsubst %.c,%.o,$(CRCS))
OBJS030    =

CFLAGS     += -nostdlib -DSLIM

OBJS       = $(patsubst %.c,%.o,$(CSRC))
DEPS       = $(CSRC:.c=.d)


$(TARGET): $(STARTUP) $(OBJS) $(OBJS030)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(STRIP) -s $@
	$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

#cache030.o: cache030.s
#	@echo [Compiling 030 code] $(@:.o=.c)
#	$(CC) $(CFLAGS030) -c $< -o $@

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
