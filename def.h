#ifndef CACHE_DEF_H_
#define CACHE_DEF_H_

#define TRUE 1
#define FALSE 0

typedef enum _REPLACE_POLICY_
{
    REPLACE_LRU,
    REPLACE_RANDOM,
    REPLACE_PLRU
} REPLACE_POLICY;

typedef enum _BYPASS_POLICY_
{
    BYPASS_MCT,
    BYPASS_
} BYPASS_POLICY;

//#define DEBUG

#endif //CACHE_DEF_H_
