#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include "storage.h"
#include "def.h"

typedef struct CacheConfig_ {
    int size;
    int associativity;
    int set_num; // Number of cache sets
    int write_through; // 0|1 for back|through
    int write_allocate; // 0|1 for no-alc|alc
    REPLACE_POLICY replace_policy;
    int use_bypass;
    int mct_size;
    int use_prefetch;
    int prefetch_stride;
    int prefetch_num;
} CacheConfig;

class Cache: public Storage {
public:
    Cache() {}
    ~Cache() {}

    // Sets & Gets
    void SetConfig(CacheConfig cc);
    void GetConfig(CacheConfig &cc) { cc = config_; }
    void SetLower(Storage *ll) { lower_ = ll; }
    // Main access process
    void HandleRequest(unsigned long long addr, int arg_bytes,
                       int read, char *content,
                       int &hit, int &time);
    void print();

private:
    int tag_bits, idx_bits, offset_bits;
    int set_num, line_num, line_size;
    bool **valid, **dirty;
    unsigned long long **tag;
    int **counter, **bt;
    char ***bytes;
    unsigned long long **mct;

    unsigned long long get_tag(unsigned long long addr);
    unsigned long long get_idx(unsigned long long addr);
    unsigned long long get_offset(unsigned long long addr);

    int find_line(unsigned long long arg_tag, int set, int &line);
    void read_hit(unsigned long long addr, int arg_set, int arg_line, int arg_offset, int arg_bytes, char *content, int &time);
    void read_miss(unsigned long long addr, int arg_set, int arg_offset, int arg_bytes, char *content, int &time);
    void write_hit(unsigned long long addr, int arg_set, int arg_line, int arg_offset, int arg_bytes, char *content, int &time);
    void write_miss(unsigned long long addr, int arg_set, int arg_offset, int arg_bytes, char *content, int &time);

    void LRU_update(int arg_set, int arg_line);
    void PLRU_update(int arg_set, int arg_line);
    int PLRU_sel(int arg_set);

    bool is_set_full(int arg_set);
    void MCT_update(int arg_set, int arg_tag);

    void prefetch(unsigned long long curr_addr);
    void PF_update(int hit, unsigned long long addr);

    CacheConfig config_;
    Storage *lower_;
    DISALLOW_COPY_AND_ASSIGN(Cache);
};

#endif //CACHE_CACHE_H_ 
