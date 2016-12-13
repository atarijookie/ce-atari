# control verbosity. use make V=1
V = 0

#ONPC    = yes
# in the CFLAGS use ONPC_GPIO or ONPC_HIGHLEVEL or ONPC_NOTHING

CXX	    = g++
CC	    = gcc
#STRIP	= strip
CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CXXFLAGS = $(CFLAGS)
DEPFLAGS = -MM -MT $(patsubst %.d,%.o,$@) -MT $@
# -M : with system headers, -MM : without

LDFLAGS	= -Llib -lrt -lanl -lpthread -lcurl -ldl -lutil

ifeq ($(ONPC),yes)
    CFLAGS += -DONPC_NOTHING -I./
else
    LDFLAGS	+= -lbcm2835
    CFLAGS += -I./
endif

DISTRO := $(shell ./whichdistro.sh)
ifeq ($(DISTRO),yocto)
	TARGET   =  cosmosex_yocto
	LDFLAGS += -lcivetweb_yocto
	CFLAGS  += -DDISTRO_YOCTO
else
	TARGET   =  cosmosex_raspbian
	LDFLAGS += -lcivetweb_raspbian
	CFLAGS += -DDISTRO_RASPBIAN
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
SUBDIR5	= webserver
SUBDIR6	= lib/cjson-code-58
SUBDIR7	= service
SUBDIR8	= acsicommand
SUBDIR9	= network
SUBDIR10 = dir2fdd
SUBDIR11 = ikbd
SUBDIR12 = mediastreaming

SRCS  = $(wildcard *.cpp) 
SRCS += $(wildcard $(SUBDIR1)/*.cpp) 
SRCS += $(wildcard $(SUBDIR2)/*.cpp) 
SRCS += $(wildcard $(SUBDIR3)/*.cpp)
SRCS += $(wildcard $(SUBDIR4)/*.cpp)
SRCS += $(wildcard $(SUBDIR5)/*.cpp)
SRCS += $(wildcard $(SUBDIR5)/**/*.cpp)
SRCS += $(wildcard $(SUBDIR5)/**/**/*.cpp)
SRCS += $(wildcard $(SUBDIR6)/cJSON.c)
SRCS += $(wildcard $(SUBDIR7)/*.cpp)
SRCS += $(wildcard $(SUBDIR8)/*.cpp)
SRCS += $(wildcard $(SUBDIR9)/*.cpp)
SRCS += $(wildcard $(SUBDIR10)/*.cpp)
SRCS += $(wildcard $(SUBDIR11)/*.cpp)
SRCS += $(wildcard $(SUBDIR12)/*.cpp)

HDRS  = $(wildcard *.h)
HDRS += $(wildcard $(SUBDIR1)/*.h)
HDRS += $(wildcard $(SUBDIR2)/*.h)
HDRS += $(wildcard $(SUBDIR3)/*.h)
HDRS += $(wildcard $(SUBDIR4)/*.h)
HDRS += $(wildcard $(SUBDIR5)/*.h)
HDRS += $(wildcard $(SUBDIR5)/**/*.h)
HDRS += $(wildcard $(SUBDIR5)/**/**/*.h)
HDRS += $(wildcard $(SUBDIR6)/cJSON.h)
HDRS += $(wildcard $(SUBDIR7)/*.h)
HDRS += $(wildcard $(SUBDIR8)/*.h)
HDRS += $(wildcard $(SUBDIR9)/*.h)
HDRS += $(wildcard $(SUBDIR10)/*.h)
HDRS += $(wildcard $(SUBDIR11)/*.h)
HDRS += $(wildcard $(SUBDIR12)/*.h)

OBJS = $(patsubst %.cpp,%.o,$(SRCS:.c=.o))
DEPS = $(patsubst %.o,%.d,$(OBJS))

#combine web scripts
WEB_CSS_PATH=webroot/css
WEB_JS_PATH=webroot/js
WEB_CSS_MODULES = $(WEB_CSS_PATH)/bootstrap.light.css\
					$(WEB_CSS_PATH)/style.css 
WEB_JS_MODULES = $(WEB_JS_PATH)/actual.js\
					$(WEB_JS_PATH)/keylinux.js\
					$(WEB_JS_PATH)/modernizr.custom.50426.js\
					$(WEB_JS_PATH)/jquery.doubletap.js\
					$(WEB_JS_PATH)/atariscreen.min.js\
					$(WEB_JS_PATH)/screen.js\
					$(WEB_JS_PATH)/script.js

WEBCOMPONENTS = $(WEB_CSS_PATH)/combined.css.gz $(WEB_JS_PATH)/jquery-combined.js.gz $(WEB_JS_PATH)/zepto-combined.js.gz

all:	$(TARGET)

$(TARGET): $(OBJS) $(HDRS) $(WEBCOMPONENTS)
	$(CXX_LINK) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
#	$(STRIP) -s $@

$(WEB_CSS_PATH)/combined.css.gz: ${WEB_CSS_MODULES}
	$(AT)cat $^ | gzip -f > $@

$(WEB_JS_PATH)/zepto-combined.js.gz: $(WEB_JS_PATH)/zepto.js $(WEB_JS_PATH)/zepto.hack.js ${WEB_JS_MODULES}
	$(AT)cat $^ | gzip -f > $@

$(WEB_JS_PATH)/jquery-combined.js.gz: $(WEB_JS_PATH)/jquery.js ${WEB_JS_MODULES}
	$(AT)cat $^ | gzip -f > $@

.PHONY:	clean depclean

clean:	depclean
	$(AT)$(RM) $(WEB_JS_PATH)/*.gz
	$(AT)$(RM) $(WEB_CSS_PATH)/*.gz
	$(AT)$(RM) $(OBJS)
	$(AT)$(RM) *.o *~ $(TARGET)

depclean:
	$(AT)$(RM) $(DEPS)

depend:	$(DEPS)

%.o:	%.cpp %.d

%.d:	%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -o $@ $<

%.o:	%.c %.d

%.d:	%.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $<

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),depclean)
-include $(DEPS)
endif
endif
