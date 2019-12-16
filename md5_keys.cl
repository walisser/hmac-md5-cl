
#include "md5.cl"
#include "keygen.cl"

__kernel void md5_keys(
    __global uint* hashes, // output
    ulong index,           // position in keyspace
    __constant const struct CLString mask[MASK_KEY_CHARS] // MASK_KEY_MASK => CLString[]
#if !KEYGEN_USE_PRIVATE_MEM
    , __local uint* scratch // temporary for key gen
#endif
    )
{
    const uint outOffset = get_global_id(0)*LOOP_COUNT;
    
    index += outOffset;
    hashes += outOffset*4;

#if KEYGEN_USE_PRIVATE_MEM
    uint key[MASK_KEY_INTS];
    uchar counters[MASK_KEY_CHARS];
#else
    __local uint* key       = (__local uint*)(scratch + (MASK_KEY_INTS+MASK_COUNTER_INTS)*get_local_id(0));
    __local uchar* counters = (__local uchar*)(key + MASK_KEY_INTS);
#endif

#if LOOP_COUNT==1
    singleKey(key, index, mask);
#else
    maskedKey(key, counters, index, mask);
#endif
    for (int j = 0; j < LOOP_COUNT; j++)
    {
        uint hash[4];
        md5_multiBlock(hash, key, MASK_KEY_CHARS);

        for (int i = 0; i < 4; i++)
            hashes[i] = hash[i];

#if LOOP_COUNT > 1
        nextMaskedKey(key, counters, mask);
        hashes += 4;
#endif
    }
}
