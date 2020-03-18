# Circle-Tree
* Directories 
  * single - a single thread version without lock
  * concurrent - a multi-threaded version with std::mutex in C++11

* How to test (single)
1. git clone https://github.com/Kakarot007/Circle-Tree.git
2. cd Circle-Tree/single
3. make
4. `./btree -n {data_size} -i {input_file} > {output_file}`  (e.g../btree -n 100 -i ./test/random_1m_input.txt > ./test/random_100_result.txt ) (In this version, program would read the data from input file and output the insertion and search result.)

* 

The test results are shown in single/src/test. normal_input.txt and it's result normal_result.txt shows the sequence data from 0 to 1023 are inserted into the Circle-Tree and random_input.txt and random_result.txt shows the random sequence from 0 to 100 are inserted to the tree orderly.   

The performance compare record is in single/record.txt. You could check the insertion and search performance of FAST-FAIR and Circle-Tree.