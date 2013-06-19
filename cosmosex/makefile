all: cosmosex

cosmosex: main.o settings.o configcomponent.o configstream.o
	g++ $^ -o cosmosex

%.o: %.cpp 
	g++ -c $<

clean:
	rm -rf *o cosmosex