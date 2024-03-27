TARGET	= vidaud.tos

# control verbosity. use make V=1 to show commands
V = 0

AS		= m68k-atari-mint-as
CC		= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
PRGFLAGS= m68k-atari-mint-flags
ASFLAGS     = -m68000
ASFLAGS030  = -m68030
CFLAGS		= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030	= -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS	= -Wl,--traditional-format
LDLIBS	= -lgcc
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

HDR		= extension.h extensiondefs.h stdlib.h
CSRC	= main.c stdlib.c extension.c 

CFLAGS	+= -nostdlib
STARTUP	= startup.s

DEPS = $(CSRC:.c=.d)
OBJS = $(patsubst %.c,%.o,$(CSRC))
OBJS030	= 

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030)
	$(CC_LINK) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(AT)$(STRIP) -s $@
	$(AT)$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

%030.o: %030.s
	$(AS) $(ASFLAGS030) -c -o $@ $<

%.o:	%.c %.d

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
