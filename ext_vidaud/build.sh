echo "compiling recv.cpp"
g++ -c recv.cpp

echo "compiling fifo.cpp"
g++ -c fifo.cpp

echo "compiling functions.cpp"
g++ -c functions.cpp

echo "compiling utils.cpp"
g++ -c utils.cpp

echo "compiling main.cpp"
g++ -c main.cpp

echo "linking..."
g++ -o ext_vidaud.elf main.o fifo.o functions.o utils.o recv.o -lpthread

