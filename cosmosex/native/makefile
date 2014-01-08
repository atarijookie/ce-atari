all:	datamedia.o nomedia.o scsi.o testmedia.o
			 
%.o: %.cpp 
	g++ -c $<
	
clean:
	rm -rf *.o
	