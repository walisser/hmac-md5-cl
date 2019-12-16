
// common header for kernels and C source
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include "config.h"

// max hashes that can be cracked at a time
#define MAX_HASHES (255)

#define MASK_KEY_INTS  ((MASK_KEY_CHARS+3)/4)
#define MASK_COUNTER_CHARS (MASK_KEY_CHARS)
#define MASK_COUNTER_INTS (MASK_KEY_INTS)
#define KEY_SCRATCH_INTS (MASK_KEY_INTS+MASK_COUNTER_INTS)

#define HMAC_MSG_INTS  ((HMAC_MSG_CHARS+3)/4)
#define HMAC_BLOCK_CHARS (64)
#define HMAC_BLOCK_INTS  (16)

// amount of local memory used by each work element, use to check
// that GPU has enough local memory to fit the local work size.
#define HMAC_SCRATCH_INTS (HMAC_BLOCK_INTS*2 + MD5_DIGEST_INTS + HMAC_MSG_INTS)

#define MD5_DIGEST_INTS  (4)
#define MD5_DIGEST_CHARS (16)

#define CLSTRING_MAX_LEN (255)

struct CLString
{
    unsigned char len;
    char ch[CLSTRING_MAX_LEN];
};


// fake types for c++ editor to make some useful warnings in kernels
#ifndef __OPENCL_VERSION__
#include "stdint.h"
#define uchar uint8_t
#define uint uint32_t
#define ulong uint64_t
#define __constant
#define __global
#define __local
#define __kernel
#define get_local_size(x) (ulong(LOCAL_WORK_SIZE))
#define get_global_id(x)  (ulong(0))
#define get_local_id(x)   (ulong(0))
#endif
#endif
