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
#include"config.h"

// #include <boost/atomic.hpp>

// class spinlock {
// private:
//   typedef enum {Locked, Unlocked} LockState;
//   boost::atomic<LockState> state_;

// public:
//   spinlock() : state_(Unlocked) {}

//   void lock()
//   {
//     while (state_.exchange(Locked, boost::memory_order_acquire) == Locked) {
//       /* busy-wait */
//     }
//   }
//   void unlock()
//   {
//     state_.store(Unlocked, boost::memory_order_release);
//   }
// };


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
unsigned char cal_hash(unsigned long x) 
{

	if (x > UINT_MAX) {
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = (x >> 16) ^ x;
	} else {
		x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
		x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
		x = x ^ (x >> 31);
	}
	return (unsigned char) x;
}

class page;

class btree{
  private:
    int height;
    char* root;

  public:

    btree();
    void setNewRoot(char *);
    void getNumberOfNodes();
    void btree_insert(entry_key_t, char*);
    void btree_insert_internal(char *, entry_key_t, char *, uint32_t);
    void btree_delete(entry_key_t);
    void btree_delete_internal
			(entry_key_t, char *, uint32_t, entry_key_t *, bool *, page **, page**);
    char *btree_search(entry_key_t);
    void btree_search_range(entry_key_t, entry_key_t, unsigned long *); 
    void printAll();

    friend class page;
};

class entry{ 
  private:
    entry_key_t key; // 8 bytes
    char* ptr; // 8 bytes

  public :
    entry(){
      key = LONG_MAX;
      ptr = nullptr;
    }

    friend class page;
    friend class btree;
};

const int cardinality = record_size;
const int count_in_line = CACHE_LINE_SIZE / sizeof(entry);

class header{
  private:
    entry* records;              // 8B
    unsigned char* buffer_records;
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
      buffer_records = new unsigned char[cardinality];
			first_index = 0;
			num_valid_key = 0;
			leftmost_ptr = nullptr;  
			right_sibling_ptr = nullptr;
			is_deleted = false;
    }

    ~header() {
      delete mtx;
      delete[] records; 
      delete[] buffer_records;
      
    }
};





class page{
  private:
    header hdr;  // header in persistent memory, 16 bytes

  public:
    friend class btree;

    page(uint32_t level = 0) {
      hdr.level = level;
      hdr.records[0].ptr = nullptr;
    }

    // this is called when tree grows
    page(page* left, entry_key_t key, page* right, uint32_t level = 0) {
      hdr.leftmost_ptr = left;  
      hdr.level = level;
      hdr.records[0].key = key;
      hdr.records[0].ptr = (char*) right;
      hdr.records[1].ptr = nullptr;

      hdr.buffer_records[0] = cal_hash(key);

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
      int last_index = get_last_idx();

			// The key under deletion falls inside LN;
			bool shift = false;
			bool is_left = false;
			int i;

			register int m = (hdr.first_index+(int)ceil(hdr.num_valid_key>>1)) & (cardinality - 1);
			
			if (key < hdr.records[m].key){ // deletion in left part
				for(i = (hdr.num_valid_key >> 1) - 1; i>=0; --i) {
					uint32_t idx = (hdr.first_index + i) & (cardinality - 1);  // index = (nh.b + i) % N
					// TODO: something wrong about leftmost_ptr
					if(!shift && hdr.records[idx].key == key) {
						// the key in the first_position is going to be removed.
						hdr.records[idx].ptr = (idx == hdr.first_index ) ? 
							(char *)hdr.leftmost_ptr : hdr.records[(idx-1) & (cardinality - 1)].ptr;
						shift = true;
						is_left = true;
					}

					if(shift) {
						int prev_idx = get_index(idx-1);
						hdr.records[idx].key = (idx==hdr.first_index) ? hdr.records[idx].key : hdr.records[prev_idx].key;
						hdr.records[idx].ptr = (idx==hdr.first_index)? nullptr : hdr.records[prev_idx].ptr;

						// flush
						uint64_t records_ptr = (uint64_t)(&hdr.records[idx]);
						int remainder = records_ptr & (CACHE_LINE_SIZE - 1);
						//Q: how??
						bool do_flush = (remainder == 0) || 
							((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) && 
							 ((remainder + sizeof(entry)) & (CACHE_LINE_SIZE - 1)) != 0);
						if(do_flush) {
							clflush((char *)records_ptr, CACHE_LINE_SIZE);
						}
					}
				}
			}else{ // del in right part
				for(i = (hdr.num_valid_key) >> 1; i < hdr.num_valid_key; ++i) {
					uint32_t idx = (hdr.first_index + i) & (cardinality - 1);  // index = (nh.b + i) % N
					if(!shift && hdr.records[idx].key == key) {
						// the key in the first_position is going to be removed.
						hdr.records[idx].ptr = (idx == hdr.first_index ) ? 
							(char *)hdr.leftmost_ptr : hdr.records[(idx-1) & (cardinality - 1)].ptr; 
						shift = true;
					}

					if(shift) {
						int next_idx = get_index(idx + 1);
						hdr.records[idx].key = (idx==last_index)? hdr.records[idx].key : hdr.records[next_idx].key;
						hdr.records[idx].ptr = (idx==last_index)? nullptr : hdr.records[next_idx].ptr;

						// flush
						uint64_t records_ptr = (uint64_t)(&hdr.records[idx]);
						int remainder = records_ptr & (CACHE_LINE_SIZE - 1);
						//Q: how??
						bool do_flush = (remainder == 0) || 
							((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) && 
							 ((remainder + sizeof(entry)) & (CACHE_LINE_SIZE - 1)) != 0);
						if(do_flush) {
							clflush((char *)records_ptr, CACHE_LINE_SIZE);
						}
					}
				}
			}
			// Modify the first or last index;
			if(shift) {
				--hdr.num_valid_key;
				if (is_left){
					hdr.first_index = (hdr.first_index + 1) & (cardinality - 1);
					clflush((char *)&(hdr.first_index), sizeof(uint32_t));
				} 
			}
			return shift;
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
      if(with_lock) {
        hdr.mtx->lock();
      }
      if(hdr.is_deleted) {
        if(with_lock) {
          hdr.mtx->unlock();
        }
        return false;
      }
      if(!only_rebalance) {
        register int num_entries_before = count();

        // This node is root
        if(this == (page *)bt->root) {
          if(hdr.level > 0) {
            if(num_entries_before == 1 && !hdr.right_sibling_ptr) {
              bt->root = (char *)hdr.leftmost_ptr;
              clflush((char *)&(bt->root), sizeof(char *));

              hdr.is_deleted = 1;
            }
          }

          // Remove the key from this node
          bool ret = remove_key(key);

          if(with_lock) {
            hdr.mtx->unlock();
          }
          return true;
        }

        bool should_rebalance = true;
        // check the node utilization
        if(num_entries_before - 1 >= (int)((cardinality - 1) * 0.5)) { 
          should_rebalance = false;
        }

        // Remove the key from this node
        bool ret = remove_key(key);

        if(!should_rebalance) {
          if(with_lock) {
            hdr.mtx->unlock();
          }
          return (hdr.leftmost_ptr == NULL) ? ret : true;
        }
      } 

      //Remove a key from the parent node
      entry_key_t deleted_key_from_parent = 0;
      bool is_leftmost_node = false;
      page *left_sibling = nullptr;
      page* left_left_sibling = nullptr;
			// return true;
			bt->btree_delete_internal(key, (char *)this, hdr.level + 1,
					&deleted_key_from_parent, &is_leftmost_node, &left_sibling, &left_left_sibling);

      if(is_leftmost_node) {
        if(with_lock) {
          hdr.mtx->unlock();
        }

        if(!with_lock) {
          hdr.right_sibling_ptr->hdr.mtx->lock();
        }
        hdr.right_sibling_ptr->remove(bt, hdr.right_sibling_ptr->hdr.records[hdr.right_sibling_ptr->hdr.first_index].key, true,
						with_lock);
        if(!with_lock) {
          hdr.right_sibling_ptr->hdr.mtx->unlock();
        }
        return true;
      }

      if(with_lock) {
        left_sibling->hdr.mtx->lock();
      }

      while(left_sibling->hdr.right_sibling_ptr != this) {
        if(with_lock) {
          page *t = left_sibling->hdr.right_sibling_ptr;
          left_sibling->hdr.mtx->unlock();
          left_sibling = t;
          left_sibling->hdr.mtx->lock();
        }
        else
          left_sibling = left_sibling->hdr.right_sibling_ptr;
      }

      register int num_entries = count();
      register int left_num_entries = left_sibling->count();

      // Merge or Redistribution
      int total_num_entries = num_entries + left_num_entries;
      if(hdr.leftmost_ptr)
        ++total_num_entries;

      entry_key_t parent_key;

      
      if (left_num_entries < (int)((cardinality-1) *0.5) && num_entries < (int)((cardinality-1) *0.5)){
        left_sibling->hdr.is_deleted = 1;
        clflush((char *)&(left_sibling->hdr.is_deleted), sizeof(uint8_t));
				clflush((char *)&(left_sibling->hdr.is_deleted), sizeof(uint8_t));
				if(left_sibling->hdr.leftmost_ptr)
					insert_key(deleted_key_from_parent, 
							(char *)hdr.leftmost_ptr, &left_num_entries);
				


				for(int i = 0; i < left_sibling->hdr.num_valid_key; ++i) {
					int idx = (left_sibling->hdr.first_index + i) & (cardinality - 1); 
					insert_key(left_sibling->hdr.records[idx].key, left_sibling->hdr.records[idx].ptr, &num_entries);
				}

				
				if (left_left_sibling != nullptr){
					left_left_sibling->hdr.right_sibling_ptr = this;
					clflush((char *)&(left_left_sibling->hdr.right_sibling_ptr), sizeof(page *));	
				}
      }
      if(with_lock) {
        left_sibling->hdr.mtx->unlock();
        hdr.mtx->unlock();
      }
      // TODO: delete
      return true;
    }

    inline void 
      insert_key(entry_key_t key, char* ptr, int *num_entries, bool flush = true,
          bool update_last_index = true) {
        bool is_left = false;
				if(*num_entries == 0) {  // this page is empty
					entry* new_entry = (entry*) &hdr.records[0];
					entry* array_end = (entry*) &hdr.records[1];
					new_entry->key = (entry_key_t) key;
					new_entry->ptr = (char*) ptr;

					array_end->ptr = (char*)nullptr;
          hdr.buffer_records[0] = cal_hash(key);
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
                hdr.buffer_records[i+1] = hdr.buffer_records[i];
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
								hdr.records[i+1].ptr = ptr;
                hdr.buffer_records[i+1] = cal_hash(key);
								if(flush)
                	clflush((char*)&hdr.records[i+1],sizeof(entry));
								inserted = 1;
								break;
							}
						}
						if(inserted==0){
							hdr.records[0].ptr =(char*) hdr.leftmost_ptr;
							hdr.records[0].key = key;
							hdr.records[0].ptr = ptr;
              hdr.buffer_records[0] = cal_hash(key);
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
									hdr.buffer_records[insert_idx] = hdr.buffer_records[idx];
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
							hdr.records[insert_idx].ptr = ptr;
              hdr.buffer_records[insert_idx] = cal_hash(key);
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
									hdr.buffer_records[insert_idx] = hdr.buffer_records[idx];
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
							hdr.records[insert_idx].ptr = ptr;
              hdr.buffer_records[insert_idx] = cal_hash(key);
							inserted = 1;
							if(flush)
								clflush((char*)&hdr.records[insert_idx],sizeof(entry));
							// TODO: update b_node, flush b_node;
						}
						if(inserted==0){
							hdr.records[0].ptr =(char*) hdr.leftmost_ptr;
							hdr.records[0].key = key;
							hdr.records[0].ptr = ptr;
              hdr.buffer_records[0] = cal_hash(key);
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
      (btree* bt, char* left, entry_key_t key, char* right,
       bool flush, bool with_lock, page *invalid_sibling = nullptr) {
        if(with_lock) {
          // hdr.mtx->lock(); // Lock the write lock
          pthread_spin_lock(&hdr.slock);
        }
        if(hdr.is_deleted) {
          if(with_lock) {
            // hdr.mtx->unlock();
            pthread_spin_unlock(&hdr.slock);

          }

          return nullptr;
        }

        // If this node has a sibling node,
        if(hdr.right_sibling_ptr && (hdr.right_sibling_ptr != invalid_sibling)) {
          // Compare this key with the first key of the sibling
          if(key > hdr.right_sibling_ptr->hdr.records[0].key) {
            if(with_lock) { 
              // hdr.mtx->unlock(); // Unlock the write lock
              // hdr.slock->unlock();
              pthread_spin_unlock(&hdr.slock);
            }
            return hdr.right_sibling_ptr->store(bt, nullptr, key, right, 
                true, with_lock, invalid_sibling);
          }
        }

        register int num_entries = count();

        // FAST
        if(num_entries < cardinality - 1) {
          insert_key(key, right, &num_entries, flush);

          if(with_lock) {
            // hdr.mtx->unlock(); // Unlock the write lock
            // hdr.slock->unlock();
            pthread_spin_unlock(&hdr.slock);
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
							sibling->insert_key(hdr.records[idx].key, hdr.records[idx].ptr, &sibling_cnt, false);
							// MARK
              hdr.records[idx].ptr = nullptr;
						//	hdr.records[idx].key = nullptr;
						}
					}
					else{ // internal node
						for(int i=1;i<=move_num;++i){ 
							int idx = get_index(m + i);
							sibling->insert_key(hdr.records[idx].key, hdr.records[idx].ptr, &sibling_cnt, false);
							hdr.records[idx].ptr = nullptr;
						//	hdr.records[idx].key = nullptr;
						}
						// TODO: have to do with the leftmost_ptr
						sibling->hdr.leftmost_ptr = (page*) hdr.records[m].ptr;
					}

          sibling->hdr.right_sibling_ptr = hdr.right_sibling_ptr;
          clflush((char *)sibling, sizeof(page));

          hdr.right_sibling_ptr = sibling;
          clflush((char*) &hdr, sizeof(hdr));

          
          hdr.records[m].ptr = nullptr;
          clflush((char*) &hdr.records[m], sizeof(entry));

          hdr.num_valid_key -= sibling_cnt;
					clflush((char *)&(hdr.num_valid_key), sizeof(uint32_t));

          num_entries = hdr.num_valid_key;

          page *ret;

          // insert the key
          if(key < split_key) {
            insert_key(key, right, &num_entries);
            ret = this;
          }
          else {
            sibling->insert_key(key, right, &sibling_cnt);
            ret = sibling;
          }

          // Set a new root or insert the split key to the parent
          if(bt->root == (char *)this) { // only one node can update the root ptr
            page* new_root = new page((page*)this, split_key, sibling, 
                hdr.level + 1);
            bt->setNewRoot((char *)new_root);

            if(with_lock) {
              // hdr.mtx->unlock(); // Unlock the write lock
              // hdr.slock->unlock();
              pthread_spin_unlock(&hdr.slock);
            }
          }
          else {
            if(with_lock) {
              // hdr.mtx->unlock(); // Unlock the write lock
              // hdr.slock->unlock();
              pthread_spin_unlock(&hdr.slock);
            }
            bt->btree_insert_internal(nullptr, split_key, (char *)sibling, 
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

    char *linear_search(entry_key_t key) {
      int i = 1;
      char *ret = nullptr;
      char *t;
      entry_key_t k;
      unsigned char hash_val = cal_hash(key);
      if(hdr.leftmost_ptr == nullptr) { // Search a leaf node
        for (i = 0; i < count(); ++i){
          int idx = get_index(hdr.first_index + i);
          if (hash_val == hdr.buffer_records[idx]) {
            if (key == hdr.records[idx].key){
              ret = (char*)hdr.records[idx].ptr;
              break;
            }
          }
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

char *btree::btree_search(entry_key_t key){
  page* p = (page*)root;

  while(p->hdr.leftmost_ptr != nullptr) {
    p = (page *)p->linear_search(key);
  }

  page *t;
  while((t = (page *)p->linear_search(key)) == p->hdr.right_sibling_ptr) {
    p = t;
    if(!p) {
      break;
    }
  }

  if(!t || (char *)t != (char *)key) {
    printf("NOT FOUND %lu, t = %x\n", key, t);
    return nullptr;
  }

  return (char *)t;
}

// insert the key in the leaf node
void btree::btree_insert(entry_key_t key, char* right){ //need to be string
  page* p = (page*)root;

  while(p->hdr.leftmost_ptr != nullptr) {
    p = (page*)p->linear_search(key);
  }

  if(!p->store(this, nullptr, key, right, true, true)) { // store 
    btree_insert(key, right);
  }
}

// store the key into the node at the given level 
void btree::btree_insert_internal
(char *left, entry_key_t key, char *right, uint32_t level) {
  if(level > ((page *)root)->hdr.level)
    return;

  page *p = (page *)this->root;

  while(p->hdr.level > level) 
    p = (page *)p->linear_search(key);

  if(!p->store(this, nullptr, key, right, true, true)) {
    btree_insert_internal(left, key, right, level);
  }
}

void btree::btree_delete(entry_key_t key) {
  page* p = (page*)root;

  while(p->hdr.leftmost_ptr != nullptr){
    p = (page*) p->linear_search(key);
  }

  page *t;
  while((t = (page *)p->linear_search(key)) == p->hdr.right_sibling_ptr) {
    p = t;
    if(!p)
      break;
  }

  if(p) {
    if(!p->remove(this, key)) {
      btree_delete(key);
    }
  }
  else {
    printf("not found the key to delete %lu\n", key);
  }
}

void btree::btree_delete_internal
(entry_key_t key, char *ptr, uint32_t level, entry_key_t *deleted_key, 
 bool *is_leftmost_node, page **left_sibling, page** left_left_sibling) {
	if(level > ((page *)this->root)->hdr.level)
		return;
	
	page *p = (page*)(this->root);

	while(p->hdr.level > level) {
		p = (page *)p->linear_search(key);
	}
	p->hdr.mtx->lock();
	if((char *)p->hdr.leftmost_ptr == ptr) {
		*is_leftmost_node = true;
    p->hdr.mtx->unlock();
		return;
	}
	
	*is_leftmost_node = false;
	
	int last_index = p->get_last_idx();
	
	for(int i=0; i < p->hdr.num_valid_key; i++) {
		int idx = p->get_index(p->hdr.first_index + i);
		if(p->hdr.records[idx].ptr == ptr) {
	    if(idx == p->hdr.first_index) {
				
				if((char *)p->hdr.leftmost_ptr != p->hdr.records[idx].ptr) {
					
					*deleted_key = p->hdr.records[idx].key;
					page* tmp = (page*)p->hdr.records[idx].ptr;
					*left_sibling = p->hdr.leftmost_ptr;
					int num_keys = (tmp)->count();
					if (((*left_sibling)->count() < (int)((cardinality-1) *0.5) && num_keys < (int)((cardinality-1) *0.5))
					){
						
						p->remove(this, *deleted_key, false, false);
						// return;
						p->set_leftmost_ptr(tmp);
						// if (num_keys == 0) delete tmp;
					}else if (num_keys == 0){
						// p->remove(this, *deleted_key, false, false);
						// delete tmp;
						// (*left_sibling)->hdr.right_sibling_ptr = nullptr;
					}
					break;
				}
			}
			else {
				int prev_idx = p->get_index(idx - 1);
				if(p->hdr.records[prev_idx].ptr != p->hdr.records[idx].ptr) {
					
					*deleted_key = p->hdr.records[idx].key;
					*left_sibling = (page *)p->hdr.records[prev_idx].ptr;
					page* tmp = (page*)p->hdr.records[idx].ptr;
					
					if (prev_idx == p->hdr.first_index){
						*left_left_sibling = p->hdr.leftmost_ptr;
					}else{
						*left_left_sibling = (page *)p->hdr.records[p->get_index(prev_idx - 1)].ptr;
					}
					int num_keys = (tmp)->count();
					if (((*left_sibling)->count() < (int)((cardinality-1) *0.5) && num_keys-1 < (int)((cardinality-1) *0.5) )
					){
						
						p->remove(this, *deleted_key, false, false);
						p->hdr.records[prev_idx].ptr = (char*)tmp;
						// if (num_keys == 0) delete tmp;
					}else if (num_keys == 0){
						// p->remove(this, *deleted_key, false, false);
						// delete tmp;
						// (*left_sibling)->hdr.right_sibling_ptr = nullptr;
					}
					
					break;
				}
			}
		}
	}
  p->hdr.mtx->unlock();
}


// Function to search keys from "min" to "max"
void btree::btree_search_range
(entry_key_t min, entry_key_t max, unsigned long *buf) {
  page *p = (page *)root;

  while(p) {
    if(p->hdr.leftmost_ptr != nullptr) {
      // The current page is internal
      p = (page *)p->linear_search(min);
    }
    else {
      // Found a leaf
      p->linear_search_range(min, max, buf);

      break;
    }
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
