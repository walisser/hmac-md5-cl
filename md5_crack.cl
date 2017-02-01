
#include "keygen.cl"
#include "md5.cl"

__kernel void md5_crack(__global uchar* results, 
    ulong index,
    __constant const struct CLString* mask,
    __constant uint* hashes,
    uint numHashes,
    __local uint* scratch
    )
{
    // generate key space on the fly
    uint gid = get_global_id(0);
    
    uint groupSize = get_local_size(0);
    uint groupOffset = gid*groupSize;
    
    index += groupOffset;
    results += groupOffset;
    
    uint key[MASK_KEY_INTS] = {0};
    uint counters[MASK_KEY_CHARS] = {0};
    
    maskedKey(key, counters, index, mask);
    
    for (int j = 0; j < groupSize; j++)
    {
        uint hash[4] = {0};
        md5_multiBlock(hash, key, MASK_KEY_CHARS);

        // match up to 255 hashes
        uchar flags=0;
        for (int i = 0; i < numHashes; i++)
        {
            if (hashes[i] == hash[0])
                flags |= (1 << i);
        }
        
        *results = flags;
        
        nextMaskedKey(key, counters, mask);
        
        results++;
    }
}
