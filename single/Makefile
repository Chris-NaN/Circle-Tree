.PHONY: all clean
.DEFAULT_GOAL := all

LIBS=-lrt -lm
INCLUDES=-I./include
CFLAGS=-O -std=c++11 -g -pthread

output = FAST-FAIR Circle-Tree Circle-Tree_buffer FP-Tree FAST-FAIR_buffer B+Tree B+Tree_binary FAST-FAIR_fp B+Tree_content_sensitive

all: main

main: src/test.cpp
	g++ $(CFLAGS) -o FAST-FAIR src/FAST-FAIR_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o FAST-FAIR_buffer src/FAST-FAIR_buffer_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o FAST-FAIR_fp src/FAST-FAIR_fp_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o Circle-Tree src/Circle-Tree_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o Circle-Tree_buffer src/Circle-Tree_buffer_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o FP-Tree src/FP-Tree_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o B+Tree src/B+Tree_test.cpp $(LIBS)
	#g++ $(CFLAGS) -o B+Tree_binary src/B+Tree_binary_test.cpp $(LIBS)
	g++ $(CFLAGS) -o B+Tree_content_sensitive src/B+Tree_content_sensitive_test.cpp $(LIBS)
	
clean: 
	rm $(output)
