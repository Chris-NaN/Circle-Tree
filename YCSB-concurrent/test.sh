#!/bin/bash
load_file="test/data_a_load"
run_file="test/data_a_run"
size=1000
t_num=1
output_file="output.txt"
echo "FAST-FAIR" > output.txt
./FAST-FAIR -l $load_file -r $run_file -n $size -t $t_num >> output.txt
echo "FAST-FAIR_buffer" >> output.txt
./FAST-FAIR_buffer -l $load_file -r $run_file -n $size -t $t_num >> output.txt
echo "FAST-FAIR_fp" >> output.txt
./FAST-FAIR_fp -l $load_file -r $run_file -n $size -t $t_num >> output.txt
echo "Circle-Tree" >> output.txt
./Circle-Tree -l $load_file -r $run_file -n $size -t $t_num >> output.txt
echo "Circle-Tree_buffer" >> output.txt
./Circle-Tree_buffer -l $load_file -r $run_file -n $size -t $t_num >> output.txt
echo "FP-Tree" >> output.txt
./FP-Tree -l $load_file -r $run_file -n $size -t $t_num >> output.txt
