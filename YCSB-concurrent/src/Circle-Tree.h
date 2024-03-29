/*
   Copyright (c) 2018, UNIST. All rights reserved.  The license is a free
   non-exclusive, non-transferable license to reproduce, use, modify and display
   the source code version of the Software, with or without modifications solely
   for non-commercial research, educational or evaluation purposes. The license
   does not entitle Licensee to technical support, telephone assistance,
   enhancements or updates to the Software. All rights, title to and ownership
   interest in the Software, including all intellectual property rights therein
   shall remain in UNIST. 
*/

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string.h>
#include <cassert>
#include <climits>
#include <future>
#include <mutex>
#include <pthread.h>

#include "config.h"

#define CPU_FREQ_MHZ (1994)
#define DELAY_IN_NS (1000)
#define CACHE_LINE_SIZE 64 
#define QUERY_NUM 25

#define IS_FORWARD(c) (c % 2 == 0)

using entry_key_t = int64_t;

pthread_mutex_t print_mtx;

static inline void cpu_pause()
{
  __asm__ volatile ("pause" ::: "memory");
}
static inline unsigned long read_tsc(void)
{
  unsigned long var;
  unsigned int hi, lo;

  asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
  var = ((unsigned long long int) hi << 32) | lo;

  return var;
}

unsigned long write_latency_in_ns=0;
unsigned long long search_time_in_insert=0;
unsigned int gettime_cnt= 0;
unsigned long long clflush_time_in_insert=0;
unsigned long long update_time_in_insert=0;
int clflush_cnt = 0;
int node_cnt=0;

using namespace std;

inline void mfence()
{
  asm volatile("mfence":::"memory");
}

inline void clflush(char *data, int len)
{
  volatile char *ptr = (char *)((unsigned long)data &~(CACHE_LINE_SIZE-1));
  mfence();
  for(; ptr<data+len; ptr+=CACHE_LINE_SIZE){
    unsigned long etsc = read_tsc() + 
      (unsigned long)(write_latency_in_ns*CPU_FREQ_MHZ/1000);
    asm volatile("clflush %0" : "+m" (*(volatile char *)ptr));
    while (read_tsc() < etsc) cpu_pause();
    //++clflush_cnt;
  }
  mfence();
}

class page;

class btree{
  private:
    int height;
    char* root;

  public:

    btree();
		void setNewRoot(char *);
		void btree_update(entry_key_t key,const char* right, int offset);
		void btree_insert(entry_key_t, char*, int);
		void btree_insert_internal(char *, entry_key_t, char *,int ,uint32_t);
		void btree_delete(entry_key_t, int);
		void btree_delete_internal
			(entry_key_t, char *,int, uint32_t, entry_key_t *, bool *, page **, page**);
		char *btree_search(entry_key_t, int);
		void btree_search_range(entry_key_t, entry_key_t, unsigned long *); 
		void printAll();

    friend class page;
};

class entry{ 
  private:
    entry_key_t key; // 8 bytes
    uint64_t ptr; // 8 bytes

  public :
    entry(){
      key = LONG_MAX;
      ptr = (uint64_t)nullptr;
    }

    friend class page;
    friend class btree;
};

const int cardinality = record_size;

class header{
  private:
    entry* records;              // 8B
		uint16_t first_index;         // 4B
		uint16_t num_valid_key;        // 4B
		// mutex here
		page* leftmost_ptr;            // 8B
		page* right_sibling_ptr;             // 8B
		uint16_t level;             // 4 bytes
		// TODO: maybe delete switch_counter?
		uint16_t is_deleted;         // 1 bytes
    std::mutex *mtx;      // 8 bytes
    pthread_spinlock_t slock;

    friend class page;
    friend class btree;

  public:
    header() {
      mtx = new std::mutex();
      
      pthread_spin_init(&slock,0);

      records = new entry[cardinality];
			first_index = 0;
			num_valid_key = 0;
			leftmost_ptr = nullptr;  
			right_sibling_ptr = nullptr;
			is_deleted = false;
    }

    ~header() {
      delete mtx;
      delete records; 
      
    }
};



const int count_in_line = CACHE_LINE_SIZE / sizeof(entry);

class page{
  private:
    header hdr;  // header in persistent memory, 16 bytes

  public:
    friend class btree;

    page(uint32_t level = 0) {
      hdr.level = level;
      hdr.records[0].ptr = (uint64_t)nullptr;
    }

    // this is called when tree grows
    page(page* left, entry_key_t key, page* right, uint32_t level = 0) {
      hdr.leftmost_ptr = left;  
      hdr.level = level;
      hdr.records[0].key = key;
      hdr.records[0].ptr = (uint64_t) right;
      hdr.records[1].ptr = (uint64_t)nullptr;

      hdr.first_index = 0;
			hdr.num_valid_key = 1;

      clflush((char*)this, sizeof(page));
    }

    void *operator new(size_t size) {
      void *ret;
      posix_memalign(&ret,64,size);
      return ret;
    }

    inline int count() {
      return hdr.num_valid_key;
    }

    inline int get_last_idx(){
			return (hdr.first_index + hdr.num_valid_key - 1) & (cardinality - 1);
		}

		inline int get_index(int idx) { // make it inline -- wangc@2020.03.08
			return (idx & (cardinality - 1));
		}
    void set_leftmost_ptr(page* ptr){
			this->hdr.leftmost_ptr = ptr;
		}

    inline bool remove_key(entry_key_t key) {
     
			return true;
    }

    bool remove(btree* bt, entry_key_t key, bool only_rebalance = false, bool with_lock = true) {
      hdr.mtx->lock();
      // pthread_spin_lock(&hdr.slock);

      bool ret = remove_key(key);

      // pthread_spin_unlock(&hdr.slock);
      hdr.mtx->unlock();

      return ret;
    }

    /*
     * Although we implemented the rebalancing of B+-Tree, it is currently blocked for the performance.
     * Please refer to the follow.
     * Chi, P., Lee, W. C., & Xie, Y. (2014, August). 
     * Making B+-tree efficient in PCM-based main memory. In Proceedings of the 2014
     * international symposium on Low power electronics and design (pp. 69-74). ACM.
     */
    bool remove_rebalancing(btree* bt, entry_key_t key, bool only_rebalance = false, bool with_lock = true) {
      
      return true;
    }

    void update_key(entry_key_t key,const char* ptr, int offset, bool flush = true, bool with_lock=true){
			int i = 0;
			char *ret = nullptr;
			if(hdr.leftmost_ptr == nullptr) { // Search a leaf node
				for (i = 0; i < count(); ++i)
					if (key == hdr.records[(hdr.first_index + i) & (cardinality - 1)].key) {
						ret = (char *)hdr.records[(hdr.first_index + i) & (cardinality - 1)].ptr;
						break;
					}
				if(ret) {
					// ret[offset] = ptr;
					strncpy(ret + offset*field_size, ptr, strlen(ptr));
					// if (flush) clflush(ret[offset], sizeof(char*));
					return;
				}
			}
		}

		page* update(btree* bt, entry_key_t key,const char* ptr, int offset, bool flush = true, bool with_lock=true){
			if(with_lock) {
          hdr.mtx->lock(); // Lock the write lock
          // pthread_spin_lock(&hdr.slock);
        }
        if(hdr.is_deleted) {
          if(with_lock) {
            hdr.mtx->unlock();
            // pthread_spin_unlock(&hdr.slock);

          }
          return nullptr;
        }
      if(hdr.right_sibling_ptr && (hdr.right_sibling_ptr != nullptr)) {
				// Compare this key with the first key of the sibling
				if(key > hdr.right_sibling_ptr->hdr.records[hdr.right_sibling_ptr->hdr.first_index].key) {
					return hdr.right_sibling_ptr->update(bt, key, ptr, offset, true);
				}
			}
			update_key(key, ptr, offset, true);
      if(with_lock) {
        hdr.mtx->unlock(); // Unlock the write lock
        // hdr.slock->unlock();
        // pthread_spin_unlock(&hdr.slock);
      }
			return this;
		}

    inline void 
      insert_key(entry_key_t key, char* ptr, int offset, int *num_entries, bool flush = true,
          bool update_last_index = true) {
        bool is_left = false;
				if(*num_entries == 0) {  // this page is empty
					entry* new_entry = (entry*) &hdr.records[0];
					entry* array_end = (entry*) &hdr.records[1];
					new_entry->key = (entry_key_t) key;
					new_entry->ptr = (uint64_t) ptr;

					array_end->ptr = (uint64_t)nullptr;
					hdr.first_index = 0;

					if(flush) { // FIXME -- wangc@2020.03.08
						clflush((char*) this, CACHE_LINE_SIZE);
					}
				}
				else {
					// TODO: wired here. Have to know the usage of these code.
					int i = *num_entries - 1, inserted = 0;

					if (hdr.leftmost_ptr != nullptr){
						for (i = *num_entries-1; i>=0; i--){
							if (key < hdr.records[i].key){
								hdr.records[i+1].ptr = hdr.records[i].ptr;
								hdr.records[i+1].key = hdr.records[i].key;
								if(flush) {
									uint64_t records_ptr = (uint64_t)(&hdr.records[i+1]);

									int remainder = records_ptr & (CACHE_LINE_SIZE - 1);
									bool do_flush = (remainder == 0) || 
										((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) 
										 && ((remainder+sizeof(entry))&(CACHE_LINE_SIZE - 1))!=0);
									if(do_flush) {
										clflush((char*)(&records_ptr),CACHE_LINE_SIZE);
									}
								}
							}else{
								hdr.records[i+1].ptr = hdr.records[i].ptr;
								hdr.records[i+1].key = key;
								hdr.records[i+1].ptr = (uint64_t)ptr;
								if(flush)
                	clflush((char*)&hdr.records[i+1],sizeof(entry));
								inserted = 1;
								break;
							}
						}
						if(inserted==0){
							// hdr.records[0].ptr =(char*) hdr.leftmost_ptr;
							hdr.records[0].key = key;
							hdr.records[0].ptr = (uint64_t)ptr;
							if(flush)
								clflush((char*) &hdr.records[0], sizeof(entry)); 
							hdr.first_index = 0;
						}
					}else{
						// circle tree insertion

						// shift the left part
						// FIXME, do not use division like "*num_entries / 2". It is slow. Replace with bit shift. -- wangc@2020.03.08
						if (key < hdr.records[(hdr.first_index + (*num_entries >> 1)) & (cardinality - 1)].key){
							// insert in the left part
							// copy the leftmost_ptr first.
							// FIXME, avoid 0.5 in code. Use integers. -- wangc@2020.03.08
							for(i = 0; i<(*num_entries >> 1); i++){
								int idx = (hdr.first_index + i) & (cardinality - 1);  // index = (nh.b + i) % N
								if (key > hdr.records[idx].key){
									int insert_idx = (idx - 1) & (cardinality - 1);
									hdr.records[insert_idx].ptr = hdr.records[idx].ptr;
									hdr.records[insert_idx].key = hdr.records[idx].key;
									// flush the cacheline if A[idx] is at the start of a cache line;
									if(flush) {
										uint64_t records_ptr = (uint64_t)(&hdr.records[idx]);

										int remainder = records_ptr & (CACHE_LINE_SIZE - 1);
										bool do_flush = (remainder == 0) || 
											((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) 
											&& ((remainder+sizeof(entry))&(CACHE_LINE_SIZE - 1))!=0);
										if(do_flush) {
											clflush((char*)(&hdr.records[insert_idx]),CACHE_LINE_SIZE);
										}
									}
								} else {
									break;
								}
							}// end for
							// insert the key and ptr to new position
							int insert_idx = (hdr.first_index + i - 1) & (cardinality - 1);
							hdr.records[insert_idx].key = key;
							hdr.records[insert_idx].ptr = (uint64_t)ptr;
							if(flush)
								clflush((char*)&hdr.records[insert_idx], sizeof(entry));
							is_left = true;
							inserted = 1;
							// TODO: update b_node, flush b_node;
						}else{  // shift the right part
							// copy the rightmost ptr firstly.

							for(i = *num_entries - 1; i>=(*num_entries >> 1); i--){
								int idx = (hdr.first_index + i) & (cardinality - 1);  // index = (nh.b + i) % N
								if (key < hdr.records[idx].key){
									int insert_idx = (idx + 1) & (cardinality - 1);
									hdr.records[insert_idx].ptr = hdr.records[idx].ptr;
									hdr.records[insert_idx].key = hdr.records[idx].key;
									// flush the cacheline if A[idx] is at the start of a cache line;
									if(flush) {
										uint64_t records_ptr = (uint64_t)(&hdr.records[insert_idx]);

										int remainder = records_ptr & (CACHE_LINE_SIZE - 1);
										bool do_flush = (remainder == 0) || 
											((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) 
											&& ((remainder+sizeof(entry))&(CACHE_LINE_SIZE - 1))!=0);
										if(do_flush) {
											clflush((char*)(&hdr.records[insert_idx]),CACHE_LINE_SIZE);
										
										}
									}
								}else{
									break;
								}
							}// end for
							// insert the key and ptr to new position
							int insert_idx = (hdr.first_index + i + 1) & (cardinality - 1);
							hdr.records[insert_idx].key = key;
							hdr.records[insert_idx].ptr = (uint64_t)ptr;
							inserted = 1;
							if(flush)
								clflush((char*)&hdr.records[insert_idx],sizeof(entry));
							// TODO: update b_node, flush b_node;
						}
						if(inserted==0){
							
							hdr.records[0].key = key;
							hdr.records[0].ptr = (uint64_t)ptr;
							if(flush)
								clflush((char*) &hdr.records[0], sizeof(entry)); 
							hdr.first_index = 0;
						}
					}
				}

				++(*num_entries);
				// important   TODO: colision here?
				++hdr.num_valid_key;
				if (is_left){
          hdr.first_index = (hdr.first_index - 1) & (cardinality - 1);
          clflush((char *)&(hdr.first_index), sizeof(uint32_t));
        } 
      }

    // Insert a new key - FAST and FAIR
    page *store
      (btree* bt, char* left, entry_key_t key, char* right, int offset, 
       bool flush, bool with_lock, page *invalid_sibling = nullptr) {
        int set_all = -1;
        if(with_lock) {
          hdr.mtx->lock(); // Lock the write lock
          // pthread_spin_lock(&hdr.slock);
        }
        if(hdr.is_deleted) {
          if(with_lock) {
            hdr.mtx->unlock();
            // pthread_spin_unlock(&hdr.slock);

          }

          return nullptr;
        }

        // If this node has a sibling node,
        if(hdr.right_sibling_ptr && (hdr.right_sibling_ptr != invalid_sibling)) {
          // Compare this key with the first key of the sibling
          if(key > hdr.right_sibling_ptr->hdr.records[0].key) {
            if(with_lock) { 
              hdr.mtx->unlock(); // Unlock the write lock
              // hdr.slock->unlock();
              // pthread_spin_unlock(&hdr.slock);
            }
            return hdr.right_sibling_ptr->store(bt, nullptr, key, right, 
                true, with_lock, invalid_sibling);
          }
        }

        register int num_entries = count();

        // FAST
        if(num_entries < cardinality - 1) {
          insert_key(key, right, offset, &num_entries, flush);

          if(with_lock) {
            hdr.mtx->unlock(); // Unlock the write lock
            // hdr.slock->unlock();
            // pthread_spin_unlock(&hdr.slock);
          }

          return this;
        }
        else {// FAIR
          // overflow
          // create a new node
          page* sibling = new page(hdr.level); 
          register int m = (hdr.first_index+(int)ceil(num_entries/2)) & (cardinality - 1);
          entry_key_t split_key = hdr.records[m].key;

          // migrate half of keys into the sibling
          int sibling_cnt = 0;
          int last_index = get_last_idx();
          int move_num = (m < last_index) ? (last_index - m) : (cardinality - m + last_index);
          if (hdr.leftmost_ptr == nullptr) { // leaf node
						for (int i=0; i<=move_num; ++i) {
							int idx = get_index(m + i); 
							sibling->insert_key(hdr.records[idx].key, (char*)hdr.records[idx].ptr, set_all, &sibling_cnt, false);
							// MARK
              hdr.records[idx].ptr = (uint64_t)nullptr;
						//	hdr.records[idx].key = nullptr;
						}
					}
					else{ // internal node
						for(int i=1;i<=move_num;++i){ 
							int idx = get_index(m + i);
							sibling->insert_key(hdr.records[idx].key, (char*)hdr.records[idx].ptr, set_all, &sibling_cnt, false);
							hdr.records[idx].ptr = (uint64_t)nullptr;
						//	hdr.records[idx].key = nullptr;
						}
						// TODO: have to do with the leftmost_ptr
						sibling->hdr.leftmost_ptr = (page*) hdr.records[m].ptr;
					}

          sibling->hdr.right_sibling_ptr = hdr.right_sibling_ptr;
          clflush((char *)sibling, sizeof(page));

          hdr.right_sibling_ptr = sibling;
          clflush((char*) &hdr, sizeof(hdr));

          
          hdr.records[m].ptr = (uint64_t)nullptr;
          clflush((char*) &hdr.records[m], sizeof(entry));

          hdr.num_valid_key -= sibling_cnt;
					clflush((char *)&(hdr.num_valid_key), sizeof(uint32_t));

          num_entries = hdr.num_valid_key;

          page *ret;

          // insert the key
          if(key < split_key) {
            insert_key(key, right, offset, &num_entries);
            ret = this;
          }
          else {
            sibling->insert_key(key, right, offset, &sibling_cnt);
            ret = sibling;
          }

          // Set a new root or insert the split key to the parent
          if(bt->root == (char *)this) { // only one node can update the root ptr
            page* new_root = new page((page*)this, split_key, sibling, 
                hdr.level + 1);
            bt->setNewRoot((char *)new_root);

            if(with_lock) {
              hdr.mtx->unlock(); // Unlock the write lock
              // hdr.slock->unlock();
              // pthread_spin_unlock(&hdr.slock);
            }
          }
          else {
            if(with_lock) {
              hdr.mtx->unlock(); // Unlock the write lock
              // hdr.slock->unlock();
              // pthread_spin_unlock(&hdr.slock);
            }
            bt->btree_insert_internal(nullptr, split_key, (char *)sibling, set_all,
                hdr.level + 1);
          }

          return ret;
        }

      }

    // Search keys with linear search
    void linear_search_range
      (entry_key_t min, entry_key_t max, unsigned long *buf) {
        return;
      }

    char *linear_search(entry_key_t key, int offset) {
                                int i = 1;
                                char *ret = nullptr;
                                char *t;
                                entry_key_t k;

                                if(hdr.leftmost_ptr == nullptr) { // Search a leaf node
                                        for (i = 0; i < count(); ++i)
                                                if (key == hdr.records[(hdr.first_index + i) & (cardinality - 1)].key) {
                                                        ret = (char*)hdr.records[(hdr.first_index + i) & (cardinality - 1)].ptr;
                                                        break;
                                                }
                                        if(ret) {
                                                return ret;
                                        }
                                        if((t = (char *)hdr.right_sibling_ptr) != nullptr && key >= ((page *)t)->hdr.records[(((page *)t)->hdr).first_index].key)
                                                return t;

                                        return nullptr;
                                }
                                else { // internal node, which you do not have circular design. -- wangc@2020.03.22
                                        ret = nullptr;

                                        if(key < (k = hdr.records[0].key)) {
                                                ret = (char *)hdr.leftmost_ptr;
                                        } else {

                                                for(i = 1; i < count(); ++i) {
                                                        if(key < (k = hdr.records[i].key)) {
                                                                ret = (char*)hdr.records[i - 1].ptr;
                                                                break;
                                                        }
                                                }

                                                if(!ret) {
                                                        ret = (char*)hdr.records[i - 1].ptr;
                                                }
                                        }
                                        if ((t = (char *)hdr.right_sibling_ptr) != nullptr) {
                                                if(key >= ((page *)t)->hdr.records[0].key)
                                                        return t;
                                        }

                                        if (ret) {
                                                return ret;
                                        } else {
                                                return (char *)hdr.leftmost_ptr;
                                        }
                                }

                                return nullptr;
                        }

    // print a node 
    void print() {
      if(hdr.leftmost_ptr == nullptr) 
        printf("[%d] leaf %x \n", this->hdr.level, this);
      else 
        printf("[%d] internal %x \n", this->hdr.level, this);

      printf("fitst_index: %d\n", hdr.first_index);
      printf("last_index: %d\n", get_last_idx());
      printf("num_valid_key: %d\n", hdr.num_valid_key);

      if(hdr.leftmost_ptr!=nullptr) 
        printf("%x ",hdr.leftmost_ptr);

      for(int i=0; i < hdr.num_valid_key;++i){
        int idx = get_index(hdr.first_index + i);
        printf("K:%ld, ", hdr.records[idx].key);
        printf("V:%x. ",hdr.records[idx].ptr);
      }
        
			printf("\n");
      printf("Right_sibling: %x ", hdr.right_sibling_ptr);

      printf("\n");
    }

    void printAll() {
      if(hdr.leftmost_ptr==nullptr) {
        printf("printing leaf node: ");
        print();
      }
      else {
        printf("printing internal node: ");
        print();
        ((page*) hdr.leftmost_ptr)->printAll();
        for(int i=0;i < hdr.num_valid_key;++i){
          int idx = get_index(hdr.first_index + i);
          ((page*) hdr.records[idx].ptr)->printAll();
        }
      }
    }
};

/*
 * class btree
 */
btree::btree(){
	root = (char*)new page();
	height = 1;
}

void btree::setNewRoot(char *new_root) {
	this->root = (char*)new_root;
	clflush((char*)&(this->root),sizeof(char*));
	++height;
}

char *btree::btree_search(entry_key_t key, int offset){
	page* p = (page*)root;

	while(p->hdr.leftmost_ptr != nullptr) {
		p = (page *)p->linear_search(key, offset);
	}

	page *t;
	while((t = (page *)p->linear_search(key, offset)) == p->hdr.right_sibling_ptr) {
		p = t;
		if(!p) {
			break;
		}
	}

	if(!t) {	
		printf("NOT FOUND %lu, t = %x\n", key, t);
		return nullptr;
	}

	return (char *)t;
}

void btree::btree_update(entry_key_t key,const char* right, int offset){
	page* p = (page*)root;

	while(p->hdr.leftmost_ptr != nullptr) {
		p = (page*)p->linear_search(key, offset);
	}

	if(!p->update(this, key, right, offset, true)) { // store 
		btree_update(key, right, offset);
	}
}

// insert the key in the leaf node
void btree::btree_insert(entry_key_t key, char* right, int offset){ //need to be string
	page* p = (page*)root;

	while(p->hdr.leftmost_ptr != nullptr) {
		p = (page*)p->linear_search(key, offset);
	}

	if(!p->store(this, nullptr, key, right, offset, true, true)) { // store 
		btree_insert(key, right, offset);
	}
}

// store the key into the node at the given level 
void btree::btree_insert_internal
(char *left, entry_key_t key, char *right, int offset,uint32_t level) {
	if(level > ((page *)root)->hdr.level)
		return;

	page *p = (page *)this->root;

	while(p->hdr.level > level) 
		p = (page *)p->linear_search(key, offset);

	if(!p->store(this, nullptr, key, right, offset, true, true)) {
		btree_insert_internal(left, key, right, offset, level);
	}
}

void btree::btree_delete(entry_key_t key, int offset) {
	page* p = (page*)root;

	while(p->hdr.leftmost_ptr != nullptr){
		p = (page*) p->linear_search(key, offset);
	}

	page *t;
	while((t = (page *)p->linear_search(key, offset)) == p->hdr.right_sibling_ptr) {
		p = t;
		if(!p)
			break;
	}

	if(p) {
		if(!p->remove(this, key)) {
			btree_delete(key, offset);
		}
	}
	else {
		printf("not found the key to delete %lu\n", key);
	}
}

void btree::printAll(){
  pthread_mutex_lock(&print_mtx);
  int total_keys = 0;
	page *leftmost = (page *)root;
	printf("root: %x\n", root);
	if(root) {
		do {
			page *sibling = leftmost;
			while(sibling) {
				if(sibling->hdr.level == 0) {
					total_keys += sibling->hdr.num_valid_key;
				}
				sibling->print();
				sibling = sibling->hdr.right_sibling_ptr;
			}
			printf("-----------------------------------------\n");
			leftmost = leftmost->hdr.leftmost_ptr;
		} while(leftmost);
	}

	printf("total number of keys: %d\n", total_keys);
  pthread_mutex_unlock(&print_mtx);
}
