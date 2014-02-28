TARGET	= cosmosex

#ONPC    = yes

CC	= g++
#STRIP	= strip
CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64

ifeq ($(ONPC),yes)
    LDFLAGS	= -lrt -lpthread -lcurl
    CFLAGS += -DONPC
else
    LDFLAGS	= -lbcm2835 -lrt -lpthread -lcurl
endif

SUBDIR1	= config
SUBDIR2	= native
SUBDIR3	= translated
SUBDIR4	= floppy

SRCS  = $(wildcard *.cpp) 
SRCS += $(wildcard $(SUBDIR1)/*.cpp) 
SRCS += $(wildcard $(SUBDIR2)/*.cpp) 
SRCS += $(wildcard $(SUBDIR3)/*.cpp)
SRCS += $(wildcard $(SUBDIR4)/*.cpp)

HDRS  = $(wildcard *.h)
HDRS += $(wildcard $(SUBDIR1)/*.h)
HDRS += $(wildcard $(SUBDIR2)/*.h)
HDRS += $(wildcard $(SUBDIR3)/*.h)
HDRS += $(wildcard $(SUBDIR4)/*.h)

OBJS = $(patsubst %.cpp,%.o,$(SRCS))

all:	$(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
#	$(STRIP) -s $@ 

%.o: %.cpp 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SUBDIR1)/*.o
	rm -f $(SUBDIR2)/*.o
	rm -f $(SUBDIR3)/*.o
	rm -f $(SUBDIR4)/*.o
	rm -f *.o cosmosex
	
