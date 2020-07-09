.PHONY: all clean
.DEFAULT_GOAL := all

LIBS=-lrt -lm -pthread
INCLUDES=-I./include
CFLAGS=-O -std=c++11 -g 

output = FAST-FAIR FAST-FAIR_buffer Circle-Tree Circle-Tree_buffer FP-Tree

all: main

main: src/test.cpp
	#g++ $(CFLAGS) -o FAST-FAIR src/FAST-FAIR_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o FAST-FAIR_buffer src/FAST-FAIR_buffer_test.cpp $(LIBS)
	g++ $(CFLAGS) -o Circle-Tree src/Circle-Tree_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o Circle-Tree_buffer src/Circle-Tree_buffer_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o FP-Tree src/FP-Tree_test.cpp $(LIBS)

clean: 
	rm $(output)
