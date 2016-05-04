TARGET     = tst_stng.prg
SERVER     = test_server

# control verbosity. use make V=1 to show commands
V = 0

HOSTCC     = gcc
HOSTCFLAGS = -Wall -Wextra
HOSTLDLIBS = -lrt
CC         = m68k-atari-mint-gcc
AS         = m68k-atari-mint-as
STRIP      = m68k-atari-mint-strip
PRGFLAGS   = m68k-atari-mint-flags 
CFLAGS     = -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar
CFLAGS030  = -O2 -m68030 -fomit-frame-pointer -Wall -Wno-multichar
LDFLAGS    = -Wl,--traditional-format
LDLIBS     = -lgcc
DEPFLAGS   = -MM -MT $(patsubst %.d,%.o,$@) -MT $@

ACTUAL_HOSTCC := $(HOSTCC)
HOSTCC_0 = @echo "[HOST] Building $@"; $(ACTUAL_HOSTCC)
HOSTCC_1 = $(ACTUAL_HOSTCC)
HOSTCC = $(HOSTCC_$(V))
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

all:	$(TARGET) $(SERVER)

$(SERVER):	server_main.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^ $(HOSTLDLIBS)

$(TARGET): $(STARTUP) $(OBJS) $(OBJS030)
	$(CC_LINK) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(AT)$(STRIP) -s $@
	$(AT)$(PRGFLAGS) $@ --mno-fastram --mno-fastalloc --mno-fastload

#cache030.o: cache030.s
#	@echo [Compiling 030 code] $(@:.o=.c)
#	$(CC) $(CFLAGS030) -c $< -o $@

depend:	$(DEPS)

%.o: %.c %.d

%.d:	%.c
	$(AT)$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $^

clean:	depclean
	$(AT)$(RM) $(TARGET) *~ *.o
	$(AT)$(RM) $(SERVER)

depclean:
	$(AT)$(RM) $(DEPS)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),depclean)
-include $(DEPS)
endif
endif
