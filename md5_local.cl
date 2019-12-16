
#include "md5.cl"

__kernel void md5_local(__global uint *hashes,
     __global const struct CLString* msgs
#if !MD5_USE_PRIVATE_MEM
    , __local uint* scratch // temporary for key gen
#endif
)
{
    uint gid = get_global_id(0);
    msgs += gid;
    hashes += 4*gid;
    
    const int maxLenInts=(msgs[0].len + 3)/4;

    // copy string into int buffer
#if MD5_USE_PRIVATE_MEM
    uint intBuf[256/4]={0};
    char* tmp = (char*)intBuf;
#else
    __local uint* intBuf = scratch + get_local_id(0) * maxLenInts;
    __local char* tmp = (__local char*)intBuf;

    // scrub dirty memory
    for (int i = 0; i < maxLenInts; i++)
        intBuf[i] = 0;
#endif
    for (int i = 0; i < msgs[0].len; i++)
        tmp[i] = msgs[0].ch[i];
    
    uint hash[4] = {0};
    md5_multiBlock(hash, intBuf, msgs[0].len);

    for (int i = 0; i < 4; i++)
        hashes[i] = hash[i];    
}
