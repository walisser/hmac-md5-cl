
#include "keygen.cl"
#include "md5.cl"

__kernel
void md5_hmac(
    __global uchar* results,
    ulong index,
    __constant const struct CLString* mask,
    __constant const uint* hashes,
    const uint numHashes
#if !HMAC_USE_PRIVATE_MEM
    ,__local uint* scratch
#endif   
    )
{
    uint outOffset = get_global_id(0)*LOOP_COUNT;

    index += outOffset*LOOP_MULTIPLIER;
    results += outOffset;
    
    // zero result buffer, only touch it again
    // if there is a match
    for (uint j = 0; j < LOOP_COUNT; j++)
        results[j] = 0;

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
    uint key[MASK_KEY_INTS];
    uchar counters[MASK_KEY_CHARS];
#else
    __local uint*  __restrict key      = scratch;                 scratch += MASK_KEY_INTS;
    __local uchar* __restrict counters = (__local uchar*)scratch; scratch += MASK_COUNTER_INTS;
#endif

    // hmac padding blocks
#if HMAC_USE_PRIVATE_MEM
    uint ipad[HMAC_BLOCK_INTS + HMAC_MSG_INTS];
    uint opad[HMAC_BLOCK_INTS + MD5_DIGEST_INTS];
#else
    __local uint* __restrict ipad = scratch; scratch += HMAC_BLOCK_INTS + HMAC_MSG_INTS;
    __local uint* __restrict opad = scratch; scratch += HMAC_BLOCK_INTS + MD5_DIGEST_INTS;
#endif
    
    for (int i = 0; i < HMAC_BLOCK_INTS; ++i)
    {
        ipad[i] = 0x36363636;
        opad[i] = 0x5c5c5c5c;
    }

    // append message to input pad
    {
        __constant const char* msg = HMAC_MSG;

#if HMAC_USE_PRIVATE_MEM
        char* msgBuf = (char*)(ipad + HMAC_BLOCK_INTS);
#else
        __local char* msgBuf = (__local char*)(ipad + HMAC_BLOCK_INTS);
#endif
        for (int i = 0; i < HMAC_MSG_CHARS; ++i)
            msgBuf[i] = msg[i];
    }
    
#if LOOP_COUNT*LOOP_MULTIPLIER == 1
    singleKey(key, index, mask);
#else
    maskedKey(key, counters, index, mask);
#endif

    uint hash[4];
    for (uint j = 0; j < LOOP_COUNT*LOOP_MULTIPLIER; ++j)
    {
        for (int i = 0; i < MASK_KEY_INTS; ++i)
        {
            ipad[i] = 0x36363636 ^ key[i];
            opad[i] = 0x5c5c5c5c ^ key[i];
        }

        md5_multiBlock(hash, ipad, HMAC_BLOCK_CHARS+HMAC_MSG_CHARS);
        
        for (int i = 0; i < MD5_DIGEST_INTS; ++i)
            opad[i + HMAC_BLOCK_INTS] = hash[i];

        md5_multiBlock(hash, opad, HMAC_BLOCK_CHARS+MD5_DIGEST_CHARS);
        
        //
        // - Only test the first word of the hash (for speed),
        //   the driver checks the full hash
        //
        // - When using loop multiplier, multiple results will map to the same
        //   byte, the driver checks all possible keys that could have matched
        //
        for (uint i = 0; i < numHashes; ++i)
            if (hashes[i] == hash[0])
                results[j >> LOOP_MULTIPLIER_BITS] = 1;

#if LOOP_COUNT*LOOP_MULTIPLIER > 1
        nextMaskedKey(key, counters, mask);
#endif
    }
}
