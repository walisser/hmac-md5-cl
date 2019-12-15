
#ifndef __CONFIG_H__
#define __CONFIG_H__ 1

//
// constants used by OpenCL kernels and C functions
//
// these must be changed at least to specify the hash plaintext, target
// hashes, and character set
//
#define HASH_FILENAME "hashes.txt"

#define NUM_GPUS (1)

// tuning: number of keys tested per kernel launch
// This is effectively the buffer size (in bytes) tested by the CPU for
// any matches. It may be optimal for this size to fit in CPU cache.
// The kernel global work size is a division of this since each
// work item does multiple iterations.
#define GLOBAL_WORK_SIZE (512*1024)

// tuning: size of local work group
//         also number of loops per per work group
// nvidia likes 32, amd likes 64
#define LOCAL_WORK_SIZE  (32)

// tuning: number of buffers to use for reading back results,
// >= 2 decreases readback penalty
#define NUM_WRITE_BUFFERS (2)

// tuning: multiply the number of keys tested per work item w/o raising buffer size
// Higher values increase kernel execution time and efficiency / lower system responsiveness
// Each work item generates and tests multiple keys
// The number of keys tested per work item is LOCAL_WORK_SIZE * LOOP_MULTIPLIER
// The number of bits is shifted off the work item index, so multiple
// items map to the same index.
#define LOOP_MULTIPLIER_BITS  (1) // min 0, realistic max 10-12, max 30
#define LOOP_MULTIPLIER  (1 << LOOP_MULTIPLIER_BITS)

//#define HMAC_MSG "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore"
#define HMAC_MSG "the quick brown fox jumps over the lazy dog"
#define HMAC_MSG_CHARS (43)

// set to 1 to use private memory for temporaries, otherwise use local memory
// huge boost on AMD
// doesn't work on Nvidia, might be allocating buffers in global memory
#define HMAC_USE_PRIVATE_MEM   1
#define MD5_USE_PRIVATE_MEM    1
#define KEYGEN_USE_PRIVATE_MEM 1

// character sets for constructing a mask
#define CHARS_ALPHA       "abcdefghijklmnopqrstuvwxyz"
#define CHARS_ALPHA_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define CHARS_NUMBER      "0123456789"
#define CHARS_SYMBOL      "`~!@#$%^&*()-_=+[]{}\\|;':\",./<>?"

#define CHARS_ALPHANUMERIC (CHARS_ALPHA CHARS_ALPHA_UPPER CHARS_NUMBER)
#define CHARS_ALL (CHARS_ALPHA CHARS_ALPHA_UPPER CHARS_NUMBER CHARS_SYMBOL)

// size of the generated password
#define MASK_KEY_CHARS (8)

// configuration of the mask, must contain MASK_KEY_CHARS elements
#define MASK_KEY_MASK  { \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_ALPHANUMERIC, \
        CHARS_ALPHANUMERIC, \
        CHARS_ALPHANUMERIC, \
    }

#endif
