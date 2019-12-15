
#include "keygen.cl"
#include "md5.cl"

__kernel 

__attribute__((reqd_work_group_size(LOCAL_WORK_SIZE, 1, 1)))

void md5_hmac(
    __global uchar* results,
    ulong index,
    __constant const struct CLString* mask,
    __constant const uint* hashes,
    uint numHashes
#if !HMAC_USE_PRIVATE_MEM
    ,__local uint* scratch
#endif   
    )
{
    __constant const char* msg = HMAC_MSG;

    const uint groupSize = get_local_size(0);
    uint groupOffset = get_global_id(0)*groupSize;
    
    index += groupOffset*LOOP_MULTIPLIER;
    results += groupOffset;
    
    int localOffset = 0;
#if !KEYGEN_USE_PRIVATE_MEM
    localOffset += KEY_SCRATCH_INTS;
#endif

#if !HMAC_USE_PRIVATE_MEM
    localOffset += HMAC_SCRATCH_INTS;
#endif

#if !KEYGEN_USE_PRIVATE_MEM && !HMAC_USE_PRIVATE_MEM
    scratch = scratch + localOffset*get_local_id(0);
#endif

    // key sequence counters
#if KEYGEN_USE_PRIVATE_MEM
    uint key[MASK_KEY_INTS] = {0};
    uint counters[MASK_KEY_CHARS] = {0};
#else
    __local uint* restrict key      = scratch; scratch += MASK_KEY_INTS;
    __local uint* restrict counters = scratch; scratch += MASK_COUNTER_INTS;
#endif

    // hmac padding blocks
#if HMAC_USE_PRIVATE_MEM
    uint ipad[HMAC_BLOCK_INTS + HMAC_MSG_INTS] = {0};
    uint opad[HMAC_BLOCK_INTS + MD5_DIGEST_INTS] = {0};
#else
    __local uint* restrict ipad = scratch; scratch += HMAC_BLOCK_INTS + HMAC_MSG_INTS;
    __local uint* restrict opad = scratch; scratch += HMAC_BLOCK_INTS + MD5_DIGEST_INTS;
#endif
    
    // init buffers
    {
        #if HMAC_USE_PRIVATE_MEM
        char* restrict ptr = (char*)(ipad + HMAC_BLOCK_INTS);
        #else
        __local char* restrict ptr = (__local char*)(ipad + HMAC_BLOCK_INTS);
        #endif

        for (int i = 0; i < HMAC_BLOCK_INTS; i++)
        {
            ipad[i] = 0x36363636;
            opad[i] = 0x5c5c5c5c;
        }
        
        for (int i = 0; i < HMAC_MSG_CHARS; i++)
            ptr[i] = msg[i];
    }
    
    // init key
    maskedKey(key, counters, index, mask);
    
    uint hash[4] = {0};
    
    // zero result buffer
    for (int j = 0; j < groupSize; j++)
        results[j] = 0;
    
    for (int j = 0; j < groupSize*LOOP_MULTIPLIER; j++)
    {
        // copy key into pad
        for (int i = 0; i < MASK_KEY_INTS; i++)
        {
            ipad[i] = 0x36363636 ^ key[i];
            opad[i] = 0x5c5c5c5c ^ key[i];
        }
        
        md5_multiBlock(hash, ipad, HMAC_BLOCK_CHARS+HMAC_MSG_CHARS);
        
        for (int i = 0; i < MD5_DIGEST_INTS; i++)
            opad[i + HMAC_BLOCK_INTS] = hash[i];

        md5_multiBlock(hash, opad, HMAC_BLOCK_CHARS+MD5_DIGEST_CHARS);
        
        //
        // Results reporting is tuned a bit..
        // - Only test the first word of the hash (for speed),
        //   the driver checks the full hash
        //
        // - When using loop multiplier, multiple results will map to the same
        //   bucket. Since driver skips any bucket with 0 value, it only makes
        //   more work for the driver when there is a match.
        //
        for (int i = 0; i < numHashes; i++)
            if (hashes[i] == hash[0])
                results[j >> LOOP_MULTIPLIER_BITS] = 1;
        
        nextMaskedKey(key, counters, mask);        
    }
}
