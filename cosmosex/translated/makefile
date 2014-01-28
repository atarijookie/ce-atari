CFLAGS	= -Wall

all:	translateddisk_gemdos.o translateddisk_general.o dirtranslator.o filenameshortener.o
			 
%.o: %.cpp 
	g++ $(CFLAGS) -c $<
	
clean:
	rm -rf *.o
