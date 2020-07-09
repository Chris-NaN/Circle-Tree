#!/bin/bash
load_file="test/data_a_1m_load.txt"
run_file="test/data_a_1m_run.txt"
size=1000000
output_file="output.txt"
echo "FAST-FAIR" > output.txt
./FAST-FAIR -l $load_file -r $run_file -n $size >> output.txt
echo "Circle-Tree" >> output.txt
./Circle-Tree -l $load_file -r $run_file -n $size >> output.txt
echo "Circle-Tree_buffer" >> output.txt
./Circle-Tree_buffer -l $load_file -r $run_file -n $size >> output.txt
echo "FP-Tree" >> output.txt
./FP-Tree -l $load_file -r $run_file -n $size >> output.txt