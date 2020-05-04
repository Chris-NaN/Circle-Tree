#!/bin/bash
input_file="test/random_100m_input.txt"
size=100000000
output_file="output.txt"
echo "FAST-FAIR" > output.txt
./FAST-FAIR -i $input_file -n $size >> output.txt
echo "Circle-Tree" >> output.txt
./Circle-Tree -i $input_file -n $size >> output.txt
echo "Circle-Tree_buffer" >> output.txt
./Circle-Tree_buffer -i $input_file -n $size >> output.txt
echo "FP-Tree" >> output.txt
./FP-Tree -i $input_file -n $size >> output.txt
