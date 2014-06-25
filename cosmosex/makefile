TARGET	= cosmosex

#ONPC    = yes

CC	    = g++
#STRIP	= strip
CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64

ifeq ($(ONPC),yes)
    LDFLAGS	= -lrt -lpthread -lcurl
    CFLAGS += -DONPC
else
    LDFLAGS	= -lbcm2835 -lrt -lpthread -lcurl
endif

# uncomment following line to get map file (ce.map)
#LDFLAGS += -Wl,-Map,ce.map

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
	rm -f $(SUBDIR1)/*.o $(SUBDIR1)/*~
	rm -f $(SUBDIR2)/*.o $(SUBDIR2)/*~
	rm -f $(SUBDIR3)/*.o $(SUBDIR3)/*~
	rm -f $(SUBDIR4)/*.o $(SUBDIR4)/*~
	rm -f *.o *~ cosmosex
	
