#!/bin/bash
input_file="test/random_100m_input.txt"
size=1000000
output_file="output.txt"
echo "FAST-FAIR" > output.txt
./FAST-FAIR -i $input_file -n $size >> output.txt
echo "FAST-FAIR_bufer:"  >> output.txt
./FAST-FAIR_buffer -i $input_file -n $size >> output.txt
echo "FAST-FAIR_fp:"  >> output.txt
./FAST-FAIR_fp -i $input_file -n $size >> output.txt
echo "Circle-Tree" >> output.txt
./Circle-Tree -i $input_file -n $size >> output.txt
echo "Circle-Tree_buffer" >> output.txt
./Circle-Tree_buffer -i $input_file -n $size >> output.txt
echo "FP-Tree" >> output.txt
./FP-Tree -i $input_file -n $size >> output.txt
#echo "B+Tree" > output.txt
#./B+Tree -i $input_file -n $size >> output.txt
#echo "B+Tree_binary" >> output.txt
#./B+Tree_binary -i $input_file -n $size >> output.txt
