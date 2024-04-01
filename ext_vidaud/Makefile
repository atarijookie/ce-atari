TARGET	= ext_vidaud.elf

LINK.o   = $(LINK.cc)	# use $(CXX) for linking
CPPFLAGS += $(shell GraphicsMagick++-config --cppflags)
CXXFLAGS += -Wall -std=c++17 $(shell GraphicsMagick++-config --cxxflags)
LDFLAGS  += -lpthread $(shell GraphicsMagick++-config --ldflags)
LDLIBS   += $(shell GraphicsMagick++-config --libs)

all: $(TARGET)

$(TARGET): main.o fifo.o functions.o utils.o recv.o stream.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) *.o *~
