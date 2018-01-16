#include "stdio.h"
#include "cache.h"
#include "memory.h"
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "lisence.h"
#include "def.h"
using namespace std;

int main(int argc, char* argv[])
{
    char tracename[100] = "./2.trace";
    int iteration = 1;
    unsigned int randseed = (unsigned)time(0);
    bool optimize = true;

    Memory m;
    Cache l1, l2;

    l1.SetLower(&l2);
    l2.SetLower(&m);

    StorageStats s;
    s.access_time = 0;
    s.miss_num = 0;
    s.access_counter = 0;
    s.miss_time = 0;
    s.lower_access_counter = 0;
    s.fetch_num = 0;
    s.replace_num = 0;
    s.prefetch_num =0;
    s.bypass_num = 0;
    m.SetStats(s);
    l1.SetStats(s);
    l2.SetStats(s);

    StorageLatency ml;
    ml.bus_latency = 0;
    ml.hit_latency = 100;
    m.SetLatency(ml);

    if (argc <= 1)
    {
        help_print(argv[0]);
        return -1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "-trace=", 7) == 0)
            strcpy(tracename, argv[i]+7);
        else if (strncmp(argv[i], "-iter=", 6) == 0)
            sscanf(argv[i]+6, "%d", &iteration);
        else if (strncmp(argv[i], "-randseed=", 10) == 0)
            sscanf(argv[i]+10, "%d", &randseed);
        else if (strncmp(argv[i], "-defaultconfig", 14) == 0)
            optimize = false;
        else
        {
            help_print(argv[0]);
            return -1;
        }
    }

    srand(randseed);

    StorageLatency ll;
    ll.bus_latency = 0;
    ll.hit_latency = 3;
    l1.SetLatency(ll);
    CacheConfig cc;
    cc.set_num = 64;
    cc.associativity = 8;
    cc.size = 32 * 1024;
    cc.write_through = 0;
    cc.write_allocate = 1;
    cc.replace_policy = REPLACE_PLRU;
    cc.use_bypass = 0;
    cc.use_prefetch = 1;
    cc.prefetch_num = 4;
    cc.prefetch_stride = 4;
    if (!optimize)
    {
        cc.replace_policy = REPLACE_LRU;
        cc.use_bypass = 0;
        cc.use_prefetch = 0;
    }
    l1.SetConfig(cc);
    printf("L1-Cache config:\n");
    printf("  Size = %d byte\n", cc.size);
    printf("  Set number = %d\n", cc.set_num);
    printf("  Associativity = %d\n", cc.associativity);
    printf("  %s\n", cc.write_through==0?"Write back":"Write through");
    printf("  %s\n", cc.write_allocate==1?"Write alloc":"Write non-alloc");
    printf("  Hit latency = %d cycles\n", ll.hit_latency);
    printf("  Bus latency = %d cycles\n", ll.bus_latency);
    switch (cc.replace_policy)
    {
    case REPLACE_LRU: printf("  Replace policy: LRU\n"); break;
    case REPLACE_RANDOM: printf("  Replace policy: Random\n"); break;
    case REPLACE_PLRU: printf("  Replace policy: PLRU\n"); break;
    default: printf("  Unknown replace policy, abort!\n"); return -1;
    }
    printf("  %s\n", (cc.use_bypass==1 ? "Bypass applied" : "No bypass applied"));
    if (cc.use_bypass == 1)
        printf("  MCT size = %d\n", cc.mct_size);
    printf("  %s\n", (cc.use_prefetch==1 ? "Prefetch applied" : "No prefetch applied"));
    if (cc.use_prefetch == 1)
    {
        printf("  Prefetch stride = %d (lines)\n", cc.prefetch_stride);
        printf("  Prefetch num = %d (lines)\n", cc.prefetch_num);
    }

    ll.bus_latency = 6;
    ll.hit_latency = 4;
    l2.SetLatency(ll);
    cc.size = 256 * 1024;
    cc.set_num = 512;
    cc.replace_policy = REPLACE_PLRU;
    cc.use_bypass = 1;
    cc.mct_size = 1;
    cc.use_prefetch = 0;
    cc.prefetch_num = 4;
    cc.prefetch_stride = 4;
    if (!optimize)
    {
        cc.replace_policy = REPLACE_LRU;
        cc.use_bypass = 0;
        cc.use_prefetch = 0;
    }
    l2.SetConfig(cc);
    printf("L2-Cache config:\n");
    printf("  Size = %d byte\n", cc.size);
    printf("  Set number = %d\n", cc.set_num);
    printf("  Associativity = %d\n", cc.associativity);
    printf("  %s\n", cc.write_through==0?"Write back":"Write through");
    printf("  %s\n", cc.write_allocate==1?"Write alloc":"Write non-alloc");
    printf("  Hit latency = %d cycles\n", ll.hit_latency);
    printf("  Bus latency = %d cycles\n", ll.bus_latency);
    switch (cc.replace_policy)
    {
    case REPLACE_LRU: printf("  Replace policy: LRU\n"); break;
    case REPLACE_RANDOM: printf("  Replace policy: Random\n"); break;
    case REPLACE_PLRU: printf("  Replace policy: PLRU\n"); break;
    default: printf("  Unknown replace policy, abort!\n"); return -1;
    }
    printf("  %s\n", (cc.use_bypass==1 ? "Bypass applied" : "No bypass applied"));
    if (cc.use_bypass == 1)
        printf("  MCT size = %d\n", cc.mct_size);
    printf("  %s\n", (cc.use_prefetch==1 ? "Prefetch applied" : "No prefetch applied"));
    if (cc.use_prefetch == 1)
    {
        printf("  Prefetch stride = %d (lines)\n", cc.prefetch_stride);
        printf("  Prefetch num = %d (lines)\n", cc.prefetch_num);
    }

    printf("\n");

    for (int iter = 0; iter < iteration; iter++)
    {
        ifstream fin(tracename);
        char wr, tmp[1000];
        unsigned long long addr;
        int hit, time;
        char content[64];
        if (!fin.is_open())
        {
            printf("No such file named '%s'\n", tracename);
            printf("Abort!\n");
            return -1;
        }
        while(fin.getline(tmp, 1000))
        {
            int i;
            for(i = 0; tmp[i] == ' ' || tmp[i] == '\t'; i++);
            sscanf(tmp+i, "%c%llx", &wr, &addr);
            l1.HandleRequest(addr, 0, wr=='w'?0:1, content, hit, time);
            //printf("%s %c %llx\n", tmp, wr, addr);
        }
        fin.close();
        if (iteration > 10 && iter % (iteration / 10) == 0)
        {
            printf("■■");
            fflush(stdout);
        }
    }

    l1.GetLatency(ll);
    l1.GetStats(s);
    float hit_l1 = ll.bus_latency + ll.hit_latency;
    float mr_l1 = (float)s.miss_num / (float)s.access_counter;
    l2.GetLatency(ll);
    l2.GetStats(s);
    float hit_l2 = ll.bus_latency + ll.hit_latency;
    float mr_l2 = (float)s.miss_num / (float)s.access_counter;
    float penalty_l2 = ml.bus_latency + ml.hit_latency;
    printf("\n\nAMAT = %f (cycles)\n", hit_l1+mr_l1*(hit_l2+mr_l2*penalty_l2));

    l1.GetStats(s);
    printf("\nTotal L1 access count: %d\n", s.access_counter);
    printf("Total L1 access time: %d cycles\n", s.access_time);
    printf("  L1 AAT = %f cycles\n", (float)s.access_time/(float)s.access_counter);
    printf("Total L1 miss count: %d\n", s.miss_num);
    printf("  Miss rate = %f%%\n", (float)s.miss_num/(float)s.access_counter*100.0);
    //printf("Total L1 lower access when miss count: %d\n", s.lower_access_counter);
    //printf("  Lower access per miss = %f\n", (float)s.lower_access_counter/(float)s.miss_num);
    //printf("Total L1 miss time:%d cycles\n", s.miss_time);
    //printf("  Miss penalty = %f cycles\n", (float)s.miss_time/(float)s.miss_num);
    printf("Total L1 fetch count: %d\n", s.fetch_num);
    printf("Total L1 replace count: %d\n", s.replace_num);
    printf("Total L1 prefetch count: %d\n", s.prefetch_num);
    printf("Total L1 bypass count: %d\n", s.bypass_num);

    l2.GetStats(s);
    printf("\nTotal L2 access count: %d\n", s.access_counter);
    printf("Total L2 access time: %d cycles\n", s.access_time);
    printf("  L2 AAT = %f cycles\n", (float)s.access_time/(float)s.access_counter);
    printf("Total L2 miss count: %d\n", s.miss_num);
    printf("  Miss rate = %f%%\n", (float)s.miss_num/(float)s.access_counter*100.0);
    //printf("Total L1 lower access when miss count: %d\n", s.lower_access_counter);
    //printf("  Lower access per miss = %f\n", (float)s.lower_access_counter/(float)s.miss_num);
    //printf("Total L2 miss time:%d cycles\n", s.miss_time);
    //printf("  Miss penalty = %f cycles\n", (float)s.miss_time/(float)s.miss_num);
    printf("Total L2 fetch count: %d\n", s.fetch_num);
    printf("Total L2 replace count: %d\n", s.replace_num);
    printf("Total L2 prefetch count: %d\n", s.prefetch_num);
    printf("Total L2 bypass count: %d\n", s.bypass_num);
    m.GetStats(s);
    printf("\nTotal Memory access count: %d\n", s.access_counter);
    printf("Total Memory access time: %d cycles\n", s.access_time);
    printf("  Mem AAT = %f cycles\n", (float)s.access_time/(float)s.access_counter);
    return 0;
}
