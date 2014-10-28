TARGET	= cosmosex

#ONPC    = yes
# in the CFLAGS use ONPC_GPIO or ONPC_HIGHLEVEL

CC	    = g++
#STRIP	= strip
CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

ifeq ($(ONPC),yes)
    LDFLAGS	= -Llib -Llib -lrt -lpthread -lcurl -lcivetweb -ldl -lutil
    CFLAGS += -DONPC_HIGHLEVEL -I./
else
    LDFLAGS	= -Llib -lbcm2835 -lrt -lpthread -lcurl -lcivetweb -ldl -lutil
    CFLAGS += -I./
endif

# uncomment following line to get map file (ce.map)
#LDFLAGS += -Wl,-Map,ce.map

SUBDIR1	= config
SUBDIR2	= native
SUBDIR3	= translated
SUBDIR4	= floppy
SUBDIR5	= webserver
SUBDIR6	= lib/cjson-code-58
SUBDIR7	= service
SUBDIR8	= acsicommand

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

OBJS = $(patsubst %.cpp,%.o,$(SRCS))

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
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
#	$(STRIP) -s $@ 

%.o: %.cpp 
	$(CC) $(CFLAGS) -c $< -o $@

$(WEB_CSS_PATH)/combined.css.gz: ${WEB_CSS_MODULES}
	cat > $@ $^
	gzip -f $@
	mv $@.gz $@
    
$(WEB_JS_PATH)/zepto-combined.js.gz: $(WEB_JS_PATH)/zepto.js $(WEB_JS_PATH)/zepto.hack.js ${WEB_JS_MODULES}
	cat > $@ $^
	gzip -f $@
	mv $@.gz $@

$(WEB_JS_PATH)/jquery-combined.js.gz: $(WEB_JS_PATH)/jquery.js ${WEB_JS_MODULES}
	cat > $@ $^
	gzip -f $@
	mv $@.gz $@

clean:
	rm -f $(WEB_JS_PATH)/*.gz
	rm -f $(WEB_CSS_PATH)/*.gz
	find . -name "*.o" -print0| xargs -0 rm -rf
	rm -f *.o *~ cosmosex

