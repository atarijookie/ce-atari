CFLAGS	= -Wall
	
all:	configcomponent.o configstream_general.o configstream_screens.o netsettings.o
			 
%.o: %.cpp 
	g++ $(CFLAGS) -c $<
	
clean:
	rm -rf *.o
