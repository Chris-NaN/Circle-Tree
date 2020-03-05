# Circle-Tree
* Directories 
  * single - a single thread version without lock
  * concurrent - a multi-threaded version with std::mutex in C++11

* How to test (single)
1. git clone https://github.com/Kakarot007/Circle-Tree.git
2. cd Circle-Tree/single
3. make
4. `./btree -n {data_size} -i {input_file} > {output_file}`  (e.g../btree -n 100 -i ./test/random_input.txt > ./test/random_result.txt ) (In this version, the code could generate random data with data_size, the only thing to do is fill in a file name. )

* 

The test results are shown in single/src/test. normal_input.txt and it's result normal_result.txt shows the sequence data from 0 to 1023 are inserted into the Circle-Tree and random_input.txt and random_result.txt shows the random sequence from 0 to 100 are inserted to the tree orderly.