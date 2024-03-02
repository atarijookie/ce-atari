echo "compiling main.o"
g++ -c main.cpp

echo "compiling functions.o"
g++ -c functions.cpp

echo "linking..."
g++ -o extension_example_c.elf main.o functions.o

