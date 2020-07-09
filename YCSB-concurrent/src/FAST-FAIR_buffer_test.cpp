#include <vector>
#include <algorithm>
#include <time.h>
#include <fstream>
#include <sstream>
#include <string>
#include "FAST-FAIR_buffer.h"
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

long long insertion(btree *bt, char *load_path){
    cout<<load_path<<endl;
    struct timespec start, end;
    ifstream ifs;
    ifs.open(load_path);
    if(!ifs) {
        cout << "load_data_file loading error!" << endl;
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
    return load_time;
}

vector<long long> search_update(btree *bt, char *run_path){
    ifstream ifs;
    struct timespec start, end;
    ifs.open(run_path);
    if(!ifs) {
        cout << "run_data_file loading error!" << endl;
        exit(-1);  
    }
    string line, word, key;
    int64_t i_key;
    char * vals = nullptr;
    const char* p_val = nullptr;
    int offset;
    long long load_time = 0, search_time = 0, update_time = 0;
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
            // cout<<"search test: "<<search_time<<endl;
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
    vector<long long> rslt;
    
    rslt.push_back(search_time);
    rslt.push_back(update_time);
    return rslt;
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
    // ifs.open(load_path);
    // if(!ifs) {
    //     cout << "load_data_file loading error!" << endl;
        
    //     delete[] keys;
    //     exit(-1);  
    // }

    vector<future<long long>> futures(n_threads);
    long data_per_thread = num_data / n_threads;
    for(int tid = 0; tid < n_threads; tid++) {
        
        string tmp_str = string(load_path) + "_" + to_string(tid) + ".txt";
        char *tmp_char = (char *)tmp_str.data();
        char *load_path_i = new char[50];
        strcpy(load_path_i, tmp_char);
        // char *load_path_i = (char *)tmp_str.data();
        // TODO: copy load file into test/ and change this string.
        // cout<<load_path_i<<endl;
        auto f = async(launch::async,insertion , bt, load_path_i);
        futures.push_back(move(f));
    }
    for(auto &&f : futures) 
        if(f.valid())
            cout<<(double)f.get()/(1000*num_data)<<endl; 
    // bt->printAll();  
    vector<future<vector<long long>>> futures_search(n_threads);
    for(int tid = 0; tid < n_threads; tid++) {
        
        string tmp_str = string(run_path) + "_" + to_string(tid) + ".txt";
        char *tmp_char = (char *)tmp_str.data();
        char *run_path_i = new char[50];
        strcpy(run_path_i, tmp_char);
        // char *load_path_i = (char *)tmp_str.data();
        // TODO: copy load file into test/ and change this string.
        // cout<<load_path_i<<endl;
        auto f = async(launch::async,search_update , bt, run_path_i);
        futures_search.push_back(move(f));
    }
    for(auto &&f : futures_search) 
        if(f.valid()){
            vector<long long> rslt = f.get();
            cout<<"search time:"<<(double)rslt[0]/(1000*num_data/2)<<endl;
            // cout<<"search time:"<<(double)rslt[0]<<endl;
            cout<<"update time:"<<(double)rslt[1]/(1000*num_data/2)<<endl; 
        }
    //bt->printAll();
    return 0;

   

    return 0;
}
