# control verbosity. use make V=1
V = 0

ONPC   = yes

CXX	    = g++
CC	    = gcc
#STRIP	= strip
ifneq ($(ONPC),yes)
CPU_FLAGS   = -march=armv6 -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -marm
endif
CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE $(CPU_FLAGS)
CXXFLAGS = $(CFLAGS)
DEPFLAGS = -MM -MT $(patsubst %.d,%.o,$@) -MT $@
# -M : with system headers, -MM : without

VPATH = ./lib
LDFLAGS	= -Llib $(CPU_FLAGS)
LDLIBS = -ldospath -lrt -lanl -lpthread -ldl -lutil

ifeq ($(ONPC),yes)
    OBJDIR = obj/onpc
    CFLAGS += -DONPC -I./
    TARGET = cosmosex_onpc
    LDLIBS += -ldl
else
    LDLIBS	+= -lbcm2835
    CFLAGS += -I./
    DISTRO := $(shell ./whichdistro.sh )
    ifeq ($(DISTRO),jessie)
        OBJDIR = obj/raspbian_jessie
        TARGET  =  cosmosex_raspbian_jessie
        LDLIBS += -ldl
        CFLAGS += -DDISTRO_RASPBIAN
        CFLAGS += -DDISTRO_JESSIE
    else
        OBJDIR = obj/raspbian_stretch
        TARGET =  cosmosex_raspbian_stretch
        LDLIBS += -ldl
        CFLAGS += -DDISTRO_RASPBIAN
        CFLAGS += -DDISTRO_STRETCH
    endif
    # add search path for libbcm2835.a
    # /usr/local/bin on rpi or more exotic path when cross-compiling
    VPATH += $(shell for d in `$(ACTUAL_CC) -print-search-dirs | \
                     sed '/^lib/b 1;d;:1;s,/[^/.][^/]*/\.\./,/,;t 1;s,:[^=]*=,:;,;s,;,;  ,g' | \
                     tr ';' \\\\012 | tail -n 1 | tr ':' \\\\012 ` /usr/local/lib ; do \
                       [ -f "$$d/libbcm2835.a" ] && echo $$d && break ; \
                     done )
endif

# uncomment following line to get map file (ce.map)
#LDFLAGS += -Wl,-Map,ce.map

ACTUAL_CC := $(CC)
CC_0 = @echo "$(if $(findstring .d,$@),Building dependencies for,Compiling) $<"; $(ACTUAL_CC)
CC_1 = $(ACTUAL_CC)
CC = $(CC_$(V))
ACTUAL_CXX := $(CXX)
CXX_0 = @echo "$(if $(findstring .d,$@),Building dependencies for,Compiling) $<"; $(ACTUAL_CXX)
CXX_1 = $(ACTUAL_CXX)
CXX = $(CXX_$(V))
CXXL_0 = @echo "Linking $@"; $(ACTUAL_CXX)
CXXL_1 = $(ACTUAL_CXX)
CXX_LINK = $(CXXL_$(V))

AT_0 := @
AT_1 :=
AT = $(AT_$(V))

SUBDIR1	= config
SUBDIR2	= native
SUBDIR3	= translated
SUBDIR4	= floppy
SUBDIR7	= service
SUBDIR8	= acsicommand
SUBDIR10 = dir2fdd
SUBDIR11 = ikbd
SUBDIR13 = display
SUBDIR14 = chipinterface_v1_v2
SUBDIR15 = chipinterface_network
SUBDIR16 = misc

SRCS  = $(wildcard *.cpp) 
SRCS += $(wildcard $(SUBDIR1)/*.cpp) 
SRCS += $(wildcard $(SUBDIR2)/*.cpp) 
SRCS += $(wildcard $(SUBDIR3)/*.cpp)
SRCS += $(wildcard $(SUBDIR4)/*.cpp)
SRCS += $(wildcard $(SUBDIR7)/*.cpp)
SRCS += $(wildcard $(SUBDIR8)/*.cpp)
SRCS += $(wildcard $(SUBDIR10)/*.cpp)
SRCS += $(wildcard $(SUBDIR11)/*.cpp)
SRCS += $(wildcard $(SUBDIR13)/*.cpp)
SRCS += $(wildcard $(SUBDIR14)/*.cpp)
SRCS += $(wildcard $(SUBDIR15)/*.cpp)
SRCS += $(wildcard $(SUBDIR16)/*.cpp)

HDRS  = $(wildcard *.h)
HDRS += $(wildcard $(SUBDIR1)/*.h)
HDRS += $(wildcard $(SUBDIR2)/*.h)
HDRS += $(wildcard $(SUBDIR3)/*.h)
HDRS += $(wildcard $(SUBDIR4)/*.h)
HDRS += $(wildcard $(SUBDIR7)/*.h)
HDRS += $(wildcard $(SUBDIR8)/*.h)
HDRS += $(wildcard $(SUBDIR10)/*.h)
HDRS += $(wildcard $(SUBDIR11)/*.h)
HDRS += $(wildcard $(SUBDIR13)/*.h)
HDRS += $(wildcard $(SUBDIR14)/*.h)
HDRS += $(wildcard $(SUBDIR15)/*.h)
HDRS += $(wildcard $(SUBDIR16)/*.h)
HDRS += ../libdospath/libdospath.h

OBJS = $(addprefix $(OBJDIR)/, $(patsubst %.cpp,%.o,$(SRCS:.c=.o)))
DEPS = $(patsubst %.o,%.d,$(OBJS))

all:	$(TARGET)

$(TARGET): $(LDLIBS) $(OBJS) $(HDRS)
	$(CXX_LINK) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@ $(LDLIBS)
#	$(STRIP) -s $@

print-%:	; @echo $* = $($*)

-ldospath:	lib/libdospath.a

lib/libdospath.a:
	@echo "-> Entering libdospath and making libdospath"
	cd ../libdospath && $(MAKE) all
	cp ../libdospath/libdospath.a lib/
	@echo "<- Leaving libdospath"

.PHONY:	clean depclean

clean: depclean
	$(AT)$(RM) $(OBJS)
	$(AT)$(RM) *.o *~ $(TARGET)

depclean:
	$(AT)$(RM) $(DEPS)

depend:	$(DEPS)

$(OBJDIR)/%.o:	%.cpp $(OBJDIR)/%.d
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

$(OBJDIR)/%.d:	%.cpp
	$(AT)mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -o $@ $<

$(OBJDIR)/%.o:	%.c $(OBJDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)/%.d:	%.c
	$(AT)mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $<

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),depclean)
-include $(DEPS)
endif
endif
