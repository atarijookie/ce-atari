TARGET  = libacsiscsi.a
SLIM    = yes

# control verbosity. use make V=1 to show commands
V = 0

AR       = m68k-atari-mint-ar
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
LDLIBS   = -lgcc
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

HDR      = acsi.h cookiejar.h find_ce.h global.h hdd_if.h mutex.h scsi.h stdlib.h translated.h
CSRC     = acsi.c cookiejar.c find_ce.c hdd_if.c scsi_falcon.c scsi_tt.c stdlib.c
SSRC     = cache030.s 

CFLAGS    += -nostdlib -DSLIM
CSRC      += stdlib.c
HDR       += stdlib.h

DEPS    = $(CSRC:.c=.d)
OBJS    = $(CSRC:.c=.o) $(SSRC:.s=.o)
OBJS030 = cache030.o

$(TARGET): $(OBJS) $(OBJS030)
	$(AR) rcs $(TARGET) $(OBJS) $(OBJS030)
#   $(AT)$(STRIP) -s $@

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
