TARGET	= cehwserver

CC	    = g++
CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64

LDFLAGS	= -lbcm2835 -lrt -lpthread

SRCS  = $(wildcard *.cpp) 
HDRS  = $(wildcard *.h)
OBJS = $(patsubst %.cpp,%.o,$(SRCS))

all:	$(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

%.o: %.cpp 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *~ $(TARGET)
	
