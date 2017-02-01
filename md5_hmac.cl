
#include "keygen.cl"
#include "md5.cl"

__kernel 

__attribute__((reqd_work_group_size(LOCAL_WORK_SIZE, 1, 1)))

void md5_hmac(__global uchar* restrict results, 
    ulong index,
    __constant const struct CLString* restrict mask,
    __constant const uint* restrict hashes,
    uint numHashes
#if HMAC_USE_PRIVATE_MEM

#else
    ,__local uint* restrict scratch
#endif   
    )
{
    __constant const char* msg = HMAC_MSG;

    // generate key space on the fly
    int gid = get_global_id(0);
    
    uint groupSize = get_local_size(0);
    uint groupOffset = gid*groupSize;
    
    index += groupOffset*LOOP_MULTIPLIER;
    results += groupOffset;
    
    int localOffset = 0;
#if !KEYGEN_USE_PRIVATE_MEM
    localOffset += KEY_SCRATCH_INTS;
#endif

#if !HMAC_USE_PRIVATE_MEM
    localOffset += HMAC_SCRATCH_INTS;
#endif

    scratch = scratch + localOffset*get_local_id(0);

    // key sequence counters
#if KEYGEN_USE_PRIVATE_MEM
    uint key[MASK_KEY_INTS] = {0};
    uint counters[MASK_KEY_CHARS] = {0};
#else
    __local uint* restrict key      = scratch; scratch += MASK_KEY_INTS;
    __local uint* restrict counters = scratch; scratch += MASK_KEY_CHARS;
#endif

#if HMAC_USE_PRIVATE_MEM
    uint ipad[HMAC_BLOCK_INTS + HMAC_MSG_INTS] = {0};
    uint opad[HMAC_BLOCK_INTS + MD5_DIGEST_INTS] = {0};
#else
    __local uint* restrict ipad = scratch; scratch += HMAC_BLOCK_INTS + HMAC_MSG_INTS;
    __local uint* restrict opad = scratch; scratch += HMAC_BLOCK_INTS + MD5_DIGEST_INTS;
#endif
    
    // init constant data
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
    
    uint hash[4] = {0,0,0,0};
    
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
        
        for (int i = 0; i < numHashes; i++)
            if (hashes[i] == hash[0])
                results[j >> LOOP_MULTIPLIER_BITS] = 1;
        
        nextMaskedKey(key, counters, mask);        
    }
}

