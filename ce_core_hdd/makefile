# control verbosity. use make V=1
V = 0

CXX	    = g++
CC	    = gcc
#STRIP	= strip
CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE $(CPU_FLAGS)
CXXFLAGS = $(CFLAGS)
DEPFLAGS = -MM -MT $(patsubst %.d,%.o,$@) -MT $@
# -M : with system headers, -MM : without

VPATH = ./lib
LDFLAGS	= -Llib $(CPU_FLAGS)
LDLIBS = -ldospath -lrt -lanl -lpthread -ldl -lutil
OBJDIR = obj
TARGET = ce_hdd.elf

CFLAGS += -I./
LDLIBS += -ldl

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

SUBDIR2	= native
SUBDIR3	= translated
SUBDIR8	= acsicommand
SUBDIR18 = chipinterface_network
SUBDIR19 = misc
SUBDIR20 = extension

SRCS  = $(wildcard *.cpp) 
SRCS += $(wildcard $(SUBDIR2)/*.cpp) 
SRCS += $(wildcard $(SUBDIR3)/*.cpp)
SRCS += $(wildcard $(SUBDIR8)/*.cpp)
SRCS += $(wildcard $(SUBDIR18)/*.cpp)
SRCS += $(wildcard $(SUBDIR19)/*.cpp)
SRCS += $(wildcard $(SUBDIR20)/*.cpp)

HDRS  = $(wildcard *.h)
HDRS += $(wildcard $(SUBDIR2)/*.h)
HDRS += $(wildcard $(SUBDIR3)/*.h)
HDRS += $(wildcard $(SUBDIR8)/*.h)
HDRS += $(wildcard $(SUBDIR18)/*.h)
HDRS += $(wildcard $(SUBDIR19)/*.h)
HDRS += $(wildcard $(SUBDIR20)/*.h)
HDRS += ../libdospath/libdospath.h

OBJS = $(addprefix $(OBJDIR)/, $(patsubst %.cpp,%.o,$(SRCS:.c=.o)))
DEPS = $(patsubst %.o,%.d,$(OBJS))

all:	$(TARGET)

$(TARGET): $(LDLIBS) $(OBJS) $(HDRS)
	$(CXX_LINK) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@ $(LDLIBS)
#	$(STRIP) -s $@
	@ln -sf $(CURDIR)/$(TARGET) ./core/cosmosex.elf		# create symlink from whatever the output target is to just cosmosex.elf

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
