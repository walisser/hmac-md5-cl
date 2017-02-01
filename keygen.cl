
#include "kernel.h"

#if KEYGEN_USE_PRIVATE_MEM
    #define KEYGEN_LOCAL
#else
    #define KEYGEN_LOCAL __local
#endif

void maskedKey(
    KEYGEN_LOCAL uint* restrict key,
    KEYGEN_LOCAL uint* restrict counters,
    ulong index,
    __constant const struct CLString* mask)
{
    // build up string into int in reverse order
    KEYGEN_LOCAL char* ch=(KEYGEN_LOCAL char*)key;
    
    int i;

    //for (i = 0; i< MASK_KEY_CHARS; i++)
    //    ch[i] =mask[i].ch[0];
    
    for (i = MASK_KEY_CHARS-1; i>=0; i--)
    {
        uchar base = mask[i].len;
        uchar digit = index % base;
        counters[i] = digit;
        index /= base;
        
        ch[i] = mask[i].ch[digit];        

        //if (sequence==0)
        //    break;
    }
}

void nextMaskedKey(
    KEYGEN_LOCAL uint* restrict key,
    KEYGEN_LOCAL uint* restrict counters,
    __constant const struct CLString* restrict mask)
{
    KEYGEN_LOCAL char* ch = (KEYGEN_LOCAL char*)key;
    
    for (int i = MASK_KEY_CHARS-1; i >=0; i--)
    {
        uchar digit = counters[i]+1;
        if (digit < mask[i].len)
        {
            ch[i] = mask[i].ch[digit];
            counters[i] = digit;
            break;
        }
        else
        {
            counters[i] = 0;
            ch[i] = mask[i].ch[0];
        }
    }
}

__kernel void keygen(
    __global uint* keys, 
    ulong index,
    __constant const struct CLString* mask,
    __local uint* scratch
    )
{
    uint gid = get_global_id(0);

#if KEYGEN_USE_PRIVATE_MEM
    uint key[MASK_KEY_INTS] = {0};
    uint counters[MASK_KEY_CHARS] = {0};
#else
    __local uint* key = (__local uint*)(scratch + (MASK_KEY_INTS+MASK_KEY_CHARS)*get_local_id(0));
    __local uint* counters = (__local uint*)(key + MASK_KEY_INTS);
#endif

    uint groupSize = get_local_size(0);
    uint groupOffset = gid*groupSize;

    index += groupOffset;     
    keys += groupOffset*MASK_KEY_INTS;      
    
    maskedKey(key, counters, index, mask);

    for (uint j = 0; j < groupSize; j++)
    {
        for (uint i = 0; i < MASK_KEY_INTS; i++)
            keys[i] = key[i];
        
        nextMaskedKey(key, counters, mask);
        
        keys += MASK_KEY_INTS;
    }
}
