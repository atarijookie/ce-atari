TARGET	= ce_preinit

CXX	    = g++
CC	    = gcc
#STRIP	= strip
CFLAGS	= -Wall -g 
CXXFLAGS = $(CFLAGS)

LDFLAGS	= -Llib -lbcm2835
CFLAGS += -I./

SRCS  = $(wildcard *.cpp) 

HDRS  = $(wildcard *.h)

OBJS = $(patsubst %.cpp,%.o,$(SRCS:.c=.o))

#combine web scripts
all:	$(TARGET)

$(TARGET): $(OBJS) $(HDRS) $(WEBCOMPONENTS)
	$(CXX) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
#	$(STRIP) -s $@ 

clean:
	find . -name "*.o" -exec $(RM) {} \;
	$(RM) *.o *~ $(TARGET)

