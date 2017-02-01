
#include "md5.cl"

__kernel void md5_local(__global uint *hashes, 
    __constant const struct CLString* msgs,
    __local uint* scratch
    )
{
    uint gid = get_global_id(0);
    msgs += gid;
    hashes += 4*gid;
    
    const int maxLenInts=(msgs[0].len + 3)/4;
    
    scratch += get_local_id(0) * maxLenInts;
    
    // zero local memory
    for (int i = 0; i < maxLenInts; i++)
        scratch[i] = 0;
    
    // copy string to local memory
    __local char* tmp = (__local char*)scratch;
    for (int i = 0; i < msgs[0].len; i++)
        tmp[i] = msgs[0].ch[i];
    
    uint hash[4] = {0};
    md5_multiBlock(hash, scratch, msgs[0].len);

    for (int i = 0; i < 4; i++)
        hashes[i] = hash[i];    
}
