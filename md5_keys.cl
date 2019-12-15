
#include "md5.cl"
#include "keygen.cl"

__kernel void md5_keys(
    __global uint* hashes, // output
    ulong index,           // position in keyspace
    __constant const struct CLString mask[MASK_KEY_CHARS], // MASK_KEY_MASK => CLString[]
    __local uint* scratch  // temporary for key gen
    )
{
    const uint iterations = get_local_size(0);
    const uint outOffset = get_global_id(0)*iterations;
    
    index += outOffset;
    hashes += outOffset*4;

#if KEYGEN_USE_PRIVATE_MEM
    uint key[MASK_KEY_INTS];
    uint counters[MASK_COUNTER_INTS];
#else
    __local uint* key = (__local uint*)(scratch + (MASK_KEY_INTS+MASK_COUNTER_INTS)*get_local_id(0));
    __local uint* counters = (__local uint*)(key + MASK_KEY_INTS);
#endif

    maskedKey(key, counters, index, mask);

    for (int j = 0; j < iterations; j++)
    {
        uint hash[4];
        md5_multiBlock(hash, key, MASK_KEY_CHARS);

        for (int i = 0; i < 4; i++)
            hashes[i] = hash[i];
        
        nextMaskedKey(key, counters, mask);
        hashes += 4;
    }
}
