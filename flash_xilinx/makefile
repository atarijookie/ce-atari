all: flashxilinx

flashxilinx: micro.o lenval.o ports.o
	g++ $^ -o flashxilinx

%.o: %.cpp 
	g++ -c $<

clean:
	rm -rf *o flashxilinx
	