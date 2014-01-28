CFLAGS	= -Wall

all:	imagefilemedia.o devicemedia.o nomedia.o scsi.o testmedia.o
			 
%.o: %.cpp 
	g++ $(CFLAGS) -c $<
	
clean:
	rm -rf *.o
	