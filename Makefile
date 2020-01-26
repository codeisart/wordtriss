all: main

main: main.cpp
	g++ -g -std=c++11 main.cpp -lncurses -o main 
clean:
	rm *.o
	rm ./main
