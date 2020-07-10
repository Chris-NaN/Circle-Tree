#!/bin/bash
n_threads=1
input_file="test/random_1m_input.txt"
size=1000000
output_file="output.txt"
echo "FAST-FAIR" > output.txt
./FAST-FAIR_concurrent -t $n_threads -i $input_file -n $size >> output.txt
echo "FAST-FAIR_bufer:"  >> output.txt
./FAST-FAIR_buffer_concurrent -t $n_threads -i $input_file -n $size >> output.txt
echo "FAST-FAIR_fp:"  >> output.txt
./FAST-FAIR_fp_concurrent -t $n_threads -i $input_file -n $size >> output.txt
echo "Circle-Tree" >> output.txt
./Circle-Tree_concurrent -t $n_threads -i $input_file -n $size >> output.txt
echo "Circle-Tree_buffer" >> output.txt
./Circle-Tree_buffer_concurrent -t $n_threads -i $input_file -n $size >> output.txt
echo "Circle-Tree_fp" >> output.txt
./Circle-Tree_fp_concurrent -t $n_threads -i $input_file -n $size >> output.txt
#echo "B+Tree" > output.txt
#./B+Tree -i $input_file -n $size >> output.txt
#echo "B+Tree_binary" >> output.txt
#./B+Tree_binary -i $input_file -n $size >> output.txt
