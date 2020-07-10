.PHONY: all clean
.DEFAULT_GOAL := all

LIBS=-lrt -lm -lpthread
INCLUDES=-I./include
CFLAGS=-O0 -std=c++11 -g 

output = Circle-Tree_concurrent Circle-Tree_concurrent_mixed FAST-FAIR_concurrent Circle-Tree_buffer_concurrent FAST-FAIR_buffer_concurrent Circle-Tree_fp_concurrent FAST-FAIR_fp_concurrent

all: main

main: src/test.cpp
	g++ $(CFLAGS) -o Circle-Tree_concurrent src/Circle-Tree_test.cpp $(LIBS) -DCONCURRENT
	g++ $(CFLAGS) -o Circle-Tree_concurrent_mixed src/Circle-Tree_test.cpp $(LIBS) -DCONCURRENT -DMIXED
	g++ $(CFLAGS) -o Circle-Tree_buffer_concurrent src/Circle-Tree_buffer_test.cpp $(LIBS) -DCONCURRENT
	g++ $(CFLAGS) -o Circle-Tree_fp_concurrent src/Circle-Tree_fp_test.cpp $(LIBS) -DCONCURRENT
	g++ $(CFLAGS) -o FAST-FAIR_concurrent src/FAST-FAIR_test.cpp $(LIBS) -DCONCURRENT
	g++ $(CFLAGS) -o FAST-FAIR_buffer_concurrent src/FAST-FAIR_buffer_test.cpp $(LIBS) -DCONCURRENT
	g++ $(CFLAGS) -o FAST-FAIR_fp_concurrent src/FAST-FAIR_fp_test.cpp $(LIBS) -DCONCURRENT

clean: 
	rm $(output)
