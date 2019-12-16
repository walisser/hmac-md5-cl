
#ifndef __CONFIG_H__
#define __CONFIG_H__ 1

//
// constants used by OpenCL kernels and C functions
//
// these must be changed at to specify the hash plaintext and character set
//
#define HASH_FILENAME "hashes.txt"

#define NUM_GPUS (1)

// tuning: size of result buffer tested per kernel launch
// Same as number of keys per launch if not looping
// It may be optimal for this size to fit in CPU cache.
// The kernel global work size can be a division of this.
#define GLOBAL_WORK_SIZE (512*1024)

// tuning: size of local work group
//         also number of loops per per work group
// nvidia likes 32, amd likes 64
#define LOCAL_WORK_SIZE  (32)

// tuning: number of buffers to use for reading back results,
// >= 2 decreases readback penalty
#define NUM_WRITE_BUFFERS (2)

// tuning: number of loops per work item (multiple keys per work item)
// > 1 might be better if the compiler can unroll or vectorize loops
// > 1 divides global work size without altering kernel time
// also helps amortize the setup cost common to each work item
#define LOOP_COUNT (1)

// tuning: multiply the number of keys tested per work item w/o changing global work size
// > 0 increases kernel time / lowers system responsiveness
// The number of keys tested per work item is LOOP_COUNT*2^LOOP_MULTIPLIER_BITS
// high values could be bad if the cpu cannot check results fast enough
#define LOOP_MULTIPLIER_BITS (4) // min 0, realistic 10-12, max 30
#define LOOP_MULTIPLIER  (1 << LOOP_MULTIPLIER_BITS)

// set the target message
// the length is unbounded, other configuration strings are max 255 chars
//#define HMAC_MSG "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore"
//#define HMAC_MSG_CHARS (99)

#define HMAC_MSG "the quick brown fox jumps over the lazy dog"
#define HMAC_MSG_CHARS (43)

// set to 1 to use private memory for temporaries, otherwise use local memory
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
        CHARS_ALL, \
    }
#endif
