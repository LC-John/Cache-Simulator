#ifndef LISENCE_H
#define LISENCE_H

#include <cstdio>

#define LC_CACHE_SIMULATOR
#define THIS_IS_A_CACHE_SIMULATOR
#define IF_YOU_HAVE_ANY_TROUBLE
#define PLEASE_CONTECT_ME_THROUGH_THE_MEANS_BELOW

void help_print(char* arg_name)
{
    printf("\n");
    printf("\n");
    printf("This is a cache simulator.\n");
    printf("  If you are facing any trouble, please contect me.\n");
    printf("  My name: Zhang Huangzhao\n");
    printf("  PKU ID:  1400017707\n");
    printf("  Email:   zhang_hz@pku.edu.cn\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  -trace=: specify the trace file\n");
    /*
    printf("  -size=: set the total cache size\n");
    printf("  -setnum=: set the number of sets of the cache\n");
    printf("  -associativity=: set the associativity (the number of lines in each set) of the cache\n");
    printf("  -writeup=: write back ('b') or write through ('t')\n");
    printf("  -writemiss=: write allocate ('a') or write non-allocate('n')\n");
    */
    printf("\n");
    printf("Command: %s -trace=YourTrace -size=YourCacheSize -setnum=YourCacheSetNum -associativity=YourCacheLineNum -writeup=WB/WT -writemiss=WA/WN-\n", arg_name);
    printf("  eg. %s -trace=./1.trace -size=32768 -setnum=64 -associativity=8 -writeup=b - writemiss=a\n", arg_name);
    printf("\n");
    printf("\n");
}

#endif // LISENCE_H
