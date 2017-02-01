
#ifndef __CONFIG_H__
#define __CONFIG_H__

//
// constants used by OpenCL kernels and C functions
//
// these must be changed at least to specify the hash plaintext, target
// hashes, and character set
//
#define HASH_FILENAME "hashes.txt"

#define NUM_GPUS (1)

// where to start/resume hashing in the key sequence
#define START_INDEX (0x0)

// tuning: number of keys tested per iteration
// this is also the buffer size (in bytes) tested by the CPU for
// any matches. It may be optimal for this size to fit in CPU caches.
#define GLOBAL_WORK_SIZE (512*1024)

// tuning: number of keys per work group, best value depends on gpu
#define LOCAL_WORK_SIZE  (64)

// tuning: number of buffers to use for reading back results,
// >= 2 decreases readback penalty
#define NUM_WRITE_BUFFERS (2)

// tuning: multiply the number of keys tested per work group,
// for altering the execution time of the kernel (system responsiveness)
// the number of keys per iteration (per group) is LOCAL_WORK_SIZE * LOOP_MULTIPLIER
#define LOOP_MULTIPLIER_BITS  (0)
#define LOOP_MULTIPLIER  (1 << LOOP_MULTIPLIER_BITS)

//#define HMAC_MSG "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore"
#define HMAC_MSG "the quick brown fox jumps over the lazy dog"
#define HMAC_MSG_CHARS (43)

// set to 1 to use private memory for temporaries, otherwise use local memory
// might crash or give bad output with older GPUs
#define HMAC_USE_PRIVATE_MEM   0
#define MD5_USE_PRIVATE_MEM    0
#define KEYGEN_USE_PRIVATE_MEM 0

// character sets for constructing a mask
#define CHARS_ALPHA       "abcdefghijklmnopqrstuvwxyz"
#define CHARS_ALPHA_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define CHARS_NUMBER      "0123456789"
#define CHARS_SYMBOL      "`~!@#$%^&*()-_=+[]{}\\|;':\",./<>?"

#define CHARS_ALPHANUMERIC (CHARS_ALPHA CHARS_ALPHA_UPPER CHARS_NUMBER)

// size of the generated password
#define MASK_KEY_CHARS (8)

// configuration of the mask, must contain MASK_KEY_CHARS elements
#define MASK_KEY_MASK  { \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_ALPHA, \
        CHARS_NUMBER, \
        CHARS_ALPHANUMERIC, \
        CHARS_ALPHANUMERIC, \
    }

#endif
