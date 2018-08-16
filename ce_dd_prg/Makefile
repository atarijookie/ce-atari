TARGET  = ce_dd.prg
SLIM    = yes
USE_LOWLEVEL_PEXEC = yes

# control verbosity. use make V=1 to show commands
V = 0

#ONPC    = yes

AS       = m68k-atari-mint-as
CC       = m68k-atari-mint-gcc
STRIP    = m68k-atari-mint-strip
PRGFLAGS = m68k-atari-mint-flags
ASFLAGS     = -m68000
ASFLAGS030  = -m68030
CFLAGSCOM   = -fomit-frame-pointer -Wall -Wno-multichar
CFLAGSCOM   += -fno-common
CFLAGS      = -O2 -m68000 $(CFLAGSCOM)
CFLAGS030   = -O2 -m68030 $(CFLAGSCOM)
LDFLAGS  = -Wl,--traditional-format
LDLIBS   = -lgcc ../ce_hdd_if/ce_hdd_if.a
DEPFLAGS = -MM -MT $(patsubst %.d,%.o,$@) -MT $@

ACTUAL_CC := $(CC)
CC_0 = @echo "[$(if $(findstring .d,$@),Building dependencies for,Compiling $(if $(findstring 030,$@),030,68k) code)] $<"; $(ACTUAL_CC)
CC_1 = $(ACTUAL_CC)
CC = $(CC_$(V))
CCL_0 = @echo "[Linking final app] $@"; $(ACTUAL_CC)
CCL_1 = $(ACTUAL_CC)
CC_LINK = $(CCL_$(V))
ACTUAL_AS := $(AS)
AS_0 = @echo "[Assembling $(if $(findstring 030,$@),030,68k) code] $<"; $(ACTUAL_AS)
AS_1 = $(ACTUAL_AS)
AS = $(AS_$(V))

AT_0 := @
AT_1 :=
AT = $(AT_$(V))

HDR      = xbra.h bios.h main.h \
           gemdos.h gemdos_errno.h translated.h \
           ../ce_hdd_if/hdd_if.h global.h screen.h
CSRC     = xbra.c gemdos.c gemdos_rw.c \
           bios.c main.c screen.c
SSRC     = gemdos_asm.s bios_asm.s screenirq.s

ifeq ($(USE_LOWLEVEL_PEXEC),yes)
    CSRC += gemdos_pexec_lowlevel.c
    SSRC += harddrive_lowlevel.s
else
    CSRC += gemdos_pexec.c
    CFLAGS += -DMANUAL_PEXEC
endif

ifeq ($(SLIM),yes)
    CFLAGS    += -nostdlib -DSLIM
    STARTUP    = startup.s
#   CSRC      += stdlib.c
#   HDR       += stdlib.h
endif

ifeq ($(ONPC),yes)
    CFLAGS    += -DONPC
endif

DEPS    = $(CSRC:.c=.d)
OBJS    = $(CSRC:.c=.o) $(SSRC:.s=.o)

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030)
	$(CC_LINK) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(AT)$(STRIP) -s $@
	$(AT)$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

%030.o: %030.s
	$(AS) $(ASFLAGS030) -c -o $@ $<

depend:	$(DEPS)

%.o: %.c %.d

%.d:	%.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $<

clean:	depclean
	$(AT)$(RM) $(TARGET) *~ $(OBJS) $(OBJS030)

depclean:
	$(AT)$(RM) $(DEPS)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),depclean)
-include $(DEPS)
endif
endif
