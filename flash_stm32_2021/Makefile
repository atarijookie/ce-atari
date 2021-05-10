PREFIX = /usr/local
CFLAGS += -Wall -g
LDFLAGS	= -lbcm2835

ifndef CC
	$(error CC is not defined)
endif

ifndef AR
	$(error AR is not defined)
endif

INSTALL = install

OBJS =	dev_table.o	\
	gpio.o		\
	i2c.o		\
	init.o		\
	main.o		\
	port.o		\
	serial_common.o	\
	serial_platform.o	\
	stm32.o		\
	utils.o

LIBOBJS = parsers/parsers.a

all: stm32flash

serial_platform.o: serial_posix.c serial_w32.c

parsers/parsers.a: force
	cd parsers && $(MAKE) parsers.a

stm32flash: $(OBJS) $(LIBOBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBOBJS)

clean:
	rm -f $(OBJS) stm32flash
	cd parsers && $(MAKE) $@

install: all
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 755 stm32flash $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/share/man/man1
	$(INSTALL) -m 644 stm32flash.1 $(DESTDIR)$(PREFIX)/share/man/man1

force:

.PHONY: all clean install force
