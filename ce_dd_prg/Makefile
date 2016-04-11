TARGET  = ce_dd.prg
SLIM    = yes

#ONPC    = yes

CC       = m68k-atari-mint-gcc
STRIP    = m68k-atari-mint-strip
PRGFLAGS = m68k-atari-mint-flags
CFLAGS      = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030   = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS  = -Wl,--traditional-format -lgcc

HDR      = xbra.h acsi.h scsi.h                  gemdos.h gemdos_errno.h translated.h bios.h main.h find_ce.h hdd_if.h global.h screen.h
CSRC     = xbra.c acsi.c scsi_falcon.c scsi_tt.c gemdos.c gemdos_rw.c bios.c main.c find_ce.c hdd_if.c screen.c gemdos_pexec_lowlevel.c # gemdos_pexec.c
SSRC     = gemdos_asm.s bios_asm.s screenirq.s   harddrive_lowlevel.s

ifeq ($(SLIM),yes)
    CFLAGS    += -nostdlib -DSLIM
    STARTUP    = startup.s
    CSRC      += stdlib.c
    HDR       += stdlib.h
endif

ifeq ($(ONPC),yes)
    CFLAGS    += -DONPC
endif

OBJS    = $(patsubst %.c,%.o,$(CSRC))
OBJS030 = cache030.o

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030) $(SSRC) $(HDR)
	@echo [Compiling final app] $(@:.o=.c)
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(OBJS) $(OBJS030) $(SSRC) $(LDFLAGS)
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
