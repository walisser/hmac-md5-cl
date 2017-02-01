
#include "md5.cl"
#include "keygen.cl"

__kernel void md5_keys(__global uint *hashes, 
    ulong index, 
    __constant const struct CLString* mask,
    __local uint* scratch
    )
{
    // generate key space on the fly
    uint gid = get_global_id(0);
    
    uint groupSize = get_local_size(0);
    uint groupOffset = gid*groupSize;
    
    index += groupOffset;
    hashes += groupOffset*4;

#if KEYGEN_USE_PRIVATE_MEM
    uint key[MASK_KEY_INTS] = {0};
    uchar counters[MASK_KEY_CHARS] = {0};
#else
    __local uint* key = (__local uint*)(scratch + (MASK_KEY_INTS+MASK_KEY_CHARS)*get_local_id(0));
    __local uchar* counters = (__local uchar*)(key + MASK_KEY_INTS);
#endif

    maskedKey(key, counters, index, mask);
    
    for (int j = 0; j < groupSize; j++)
    {
        uint hash[4];
        md5_multiBlock(hash, key, MASK_KEY_CHARS);

        for (int i = 0; i < 4; i++)
            hashes[i] = hash[i];
        
        nextMaskedKey(key, counters, mask);
        hashes += 4;
    }
}
