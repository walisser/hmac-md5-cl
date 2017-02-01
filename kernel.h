
// common header for kernels and C source
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include "config.h"

// max hashes that can be cracked at a time
#define MAX_HASHES (255)

#define MASK_KEY_INTS  ((MASK_KEY_CHARS+3)/4)

#define KEY_SCRATCH_INTS (MASK_KEY_INTS+MASK_KEY_CHARS)

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

#endif
