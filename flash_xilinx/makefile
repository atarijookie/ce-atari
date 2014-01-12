all: flash_xilinx

flashxilinx: micro.o lenval.o ports.o
	g++ $^ -o flashxilinx -l bcm2835 -l rt

%.o: %.cpp 
	g++ -c $<

clean:
	rm -rf *.o $(TARGET)
	