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

VPATH = ./lib
LDFLAGS	= -Llib
LDLIBS = -lrt -lanl -lpthread -lcurl -ldl -lutil

ifeq ($(ONPC),yes)
    OBJDIR = obj/onpc
    CFLAGS += -DONPC_NOTHING -I./
    TARGET = cosmosex_onpc
    LDLIBS += -lcivetweb_onpc -ldl
else
    LDLIBS	+= -lbcm2835
    CFLAGS += -I./
    DISTRO := $( shell ./whichdistro.sh )
    ifeq ($(DISTRO),yocto)
        OBJDIR = obj/yocto
        TARGET = cosmosex_yocto
        LDLIBS += -lcivetweb_yocto
        CFLAGS += -DDISTRO_YOCTO
    else
        ifeq ($(DISTRO),raspbian_jessie)
            OBJDIR = obj/raspbian_jessie
            TARGET  =  cosmosex_raspbian_jessie
            LDLIBS += -lcivetweb_raspbian_jessie -ldl
            CFLAGS += -DDISTRO_RASPBIAN
        else
            OBJDIR = obj/raspbian_stretch
            TARGET =  cosmosex_raspbian_stretch
            LDLIBS += -lcivetweb_raspbian_stretch -ldl
            CFLAGS += -DDISTRO_RASPBIAN
        endif
    endif
    # add search path for libbcm2835.a
    # /usr/local/bin on yocto or more exotic path when cross-compiling
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
SUBDIR5	= webserver
SUBDIR6	= lib/cjson-code-58
SUBDIR7	= service
SUBDIR8	= acsicommand
SUBDIR9	= network
SUBDIR10 = dir2fdd
SUBDIR11 = ikbd
SUBDIR12 = mediastreaming
SUBDIR13 = display

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
SRCS += $(wildcard $(SUBDIR13)/*.cpp)

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
HDRS += $(wildcard $(SUBDIR13)/*.h)

OBJS = $(addprefix $(OBJDIR)/, $(patsubst %.cpp,%.o,$(SRCS:.c=.o)))
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

WEBCOMPONENTS = $(WEB_CSS_PATH)/combined.css.gz \
    $(WEB_JS_PATH)/jquery-combined.js.gz \
    $(WEB_JS_PATH)/zepto-combined.js.gz \
    $(WEB_JS_PATH)/log.js.gz

all:	$(TARGET) $(WEBCOMPONENTS)

$(TARGET): $(OBJS) $(HDRS) $(LDLIBS)
	$(CXX_LINK) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@ $(LDLIBS)
#	$(STRIP) -s $@

$(WEB_CSS_PATH)/combined.css.gz: ${WEB_CSS_MODULES}
	$(AT)cat $^ | gzip -f > $@

$(WEB_JS_PATH)/zepto-combined.js.gz: $(WEB_JS_PATH)/zepto.js $(WEB_JS_PATH)/zepto.hack.js ${WEB_JS_MODULES}
	$(AT)cat $^ | gzip -f > $@

$(WEB_JS_PATH)/jquery-combined.js.gz: $(WEB_JS_PATH)/jquery.js ${WEB_JS_MODULES}
	$(AT)cat $^ | gzip -f > $@

$(WEB_JS_PATH)/log.js.gz: $(WEB_JS_PATH)/log.js
	$(AT)cat $^ | gzip -f > $@

ifeq ($(ONPC),yes)
-lcivetweb_onpc:	lib/libcivetweb_onpc.a

lib/libcivetweb_onpc.a: lib/civetweb-master.zip
	rm -rf lib/civetweb-master
	cd lib && unzip civetweb-master.zip
	$(MAKE) -C lib/civetweb-master lib WITH_CPP=1 CC=$(ACTUAL_CC) CXX=$(ACTUAL_CXX)
	mv lib/civetweb-master/libcivetweb.a $@
	rm -rf lib/civetweb-master
endif

.PHONY:	clean depclean

clean:	depclean
	$(AT)$(RM) $(WEB_JS_PATH)/*.gz
	$(AT)$(RM) $(WEB_CSS_PATH)/*.gz
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
