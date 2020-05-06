#include <vector>
#include <algorithm>
#include <time.h>
#include <fstream>
#include <sstream>
#include <string>
#include "Circle-Tree.h"
using namespace std;

#pragma comment(linker, "/STACK:102400000,102400000")


void clear_cache() {
	// Remove cache
	int size = 256*1024*1024;
	char *garbage = new char[size];
	for(int i=0;i<size;++i)
		garbage[i] = i;
	for(int i=100;i<size;++i)
		garbage[i] += garbage[i-100];
	delete[] garbage;
}

char* hmset(istringstream &ss){
    string word, val;
    int offset = 0;
    int field_size = 100;
    char *vals = new char[1000];
    while(ss >> word){ // field info
        offset = *(word.end()-1) - '0';  // get offset
        ss >> val;  // val info
        strncpy(vals + offset*field_size, val.c_str(), val.length());
        // cout << offset << val << endl;
        
    }
    vals[999] = '\0';
    // cout << vals << endl;
    return vals;
}

int main(int argc, char** argv)
{

    int num_data = 0;
    int n_threads = 1;
    float selection_ratio = 0.0f;
    char *load_path = (char *)std::string("../sample_input.txt").data();
    char *run_path;

    int c;
    while((c = getopt(argc, argv, "n:w:t:s:l:r:")) != -1) {
        switch(c) {
        case 'n':
            num_data = atoi(optarg);
            break;
        case 'w':
            write_latency_in_ns = atol(optarg);
            break;
        case 't':
            n_threads = atoi(optarg);
            break;
        case 's':
            selection_ratio = atof(optarg);
        case 'l':
            load_path = optarg;
        case 'r':
            run_path = optarg;
        default:
            break;
        }
    }


    btree *bt;
    bt = new btree();
    struct timespec start, end;

    // Reading data
    entry_key_t* keys = new entry_key_t[num_data];

    ifstream ifs;
    ifs.open(load_path);
    if(!ifs) {
        cout << "load_data_file loading error!" << endl;
        
        delete[] keys;
        exit(-1);  
    }
    string line, word, key;
    int64_t i_key;
    char * vals = nullptr;
    const char* p_val = nullptr;
    int offset;
    long long load_time = 0, search_time = 0, update_time = 0;
    while(getline(ifs, line)){
        // cout<<line<<endl;
        istringstream cut_word(line);
        cut_word >> word;
        if (word == "HMSET"){
            cut_word >> key;  // user info
            key = key.substr(key.length() - 9);
            // cout << key << endl;
            vals = hmset(cut_word);
            i_key = stoi(key);
            clock_gettime(CLOCK_MONOTONIC,&start);
            bt->btree_insert(i_key, vals, -1);
            clock_gettime(CLOCK_MONOTONIC,&end);
            load_time += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
        }
        
        // return 0;
    }
    
    // cout << load_time << endl;
    
    ifs.close();
    ifs.clear();
    // bt->printAll();
    ifs.open(run_path);
    if(!ifs) {
        cout << "run_data_file loading error!" << endl;
        exit(-1);  
    }
    // return 0;
    while(getline(ifs, line)){
        
        // cout<<line<<endl;
        istringstream cut_word(line);
        cut_word >> word;
        if (word == "HGETALL"){
            cut_word >> key;  // user info
            key = key.substr(key.length() - 9);
            i_key = stoi(key);
            clock_gettime(CLOCK_MONOTONIC,&start);
            bt->btree_search(i_key, -1);
            
            clock_gettime(CLOCK_MONOTONIC,&end);
            search_time += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
            // cout << key << endl;
        }else if(word == "HMSET"){
            
            cut_word >> key;  // user info
            key = key.substr(key.length() - 9);
            cut_word >> word;  // field info
            offset = *(word.end() - 1) - '0';
            // cout << offset << endl;

            cut_word >> word; // val info;
            i_key = stoi(key);
            // cout << key << endl;
            p_val = word.c_str();
            clock_gettime(CLOCK_MONOTONIC,&start);
            bt->btree_update(i_key, p_val, offset);
            clock_gettime(CLOCK_MONOTONIC,&end);
            update_time += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
            
            
        }
    }
    cout << "Circle-Tree: "<<endl;
    load_time /= 1000, search_time /= 1000, update_time /= 1000;
    cout << "load_time: " << load_time << " average load_time: " << load_time / num_data <<endl;
    cout << "search_time: " << search_time << " average search_time: " << search_time / num_data <<endl;
    cout << "update_time: " << update_time << " average update_time: " << update_time / num_data <<endl;
    //bt->printAll();
    return 0;

    for(int i=0; i<num_data; ++i)
        ifs >> keys[i]; 

    ifs.close();

    {
        clock_gettime(CLOCK_MONOTONIC,&start);
        
        for(int i = 0; i < num_data; ++i) {
            char *val = new char[100];
            val[0] = '1';
            bt->btree_insert(keys[i], val, -1); 
        }

        clock_gettime(CLOCK_MONOTONIC,&end);

        long long elapsed_time = 
        (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
        elapsed_time /= 1000;

        printf("INSERT elapsed_time: %ld, Avg: %f\n", elapsed_time,
            (double)elapsed_time / num_data);
    }
    // bt->btree_update(keys[5], (char*)keys[4], 0);
    // bt->btree_update(keys[5], (char*)keys[3], 0);
    // bt->btree_update(4, (char*) keys[5], 0);
    clear_cache();

    {
    clock_gettime(CLOCK_MONOTONIC,&start);

    for(int i = 0; i < num_data; ++i) {
        bt->btree_search(keys[i], -1);
    }

    clock_gettime(CLOCK_MONOTONIC,&end);

    long long elapsed_time = 
        (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
    elapsed_time /= 1000;

    printf("SEARCH elapsed_time: %ld, Avg: %f\n", elapsed_time,
        (double)elapsed_time / num_data);
    }

    bt->printAll();

    


    delete bt;
    delete[] keys;

    return 0;
}
