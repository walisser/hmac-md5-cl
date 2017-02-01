

#include "clsimple/clsimple.h"
#include "kernel.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <inttypes.h>
#include <unistd.h> // usleep
#include <signal.h> // signal()

#include <QtCore/QtCore>

#define swap32(num) \
        ((((num)>>24)&0xff) | \
        (((num)<<8)&0xff0000) | \
        (((num)>>8)&0xff00) |  \
        (((num)<<24)&0xff000000))

 typedef struct {
        uchar byte[16];
    } Hash;

static const char* kMaskCharSets[MASK_KEY_CHARS] = MASK_KEY_MASK;

static int hex2int(char ch)
{
    if (ch >='0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return 0;
}

static inline uint64_t nanoTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return ((uint64_t)tv.tv_sec)*1000000000ULL
            + (tv.tv_usec * 1000);
}

// stateless key generation 1-dimensional-charset
static void simpleKey(char* key, const int keyLen, uint64_t sequence, const char* chars, const uint64_t base)
{
    for (int i = 0; i < keyLen; i++)
    {
        uint64_t digit = sequence % base;
        sequence /= base;
        key[keyLen-i-1] = chars[digit];
    }
}

// convert array of charsets to array of CLString which also holds the string length
// mask and charSets array length == keyLen
// return number of permutations (size of key space, last sequence number+1)
static uint64_t initMask(CLString* mask, const char** charSets, int keyLen)
{
    memset(mask, 0, sizeof(*mask) * keyLen);

    uint64_t keySpace=1;

    for (int i = 0; i < keyLen; i++)
    {
        int len = strlen(charSets[i]);
        assert(len < CLSTRING_MAX_LEN);

        mask[i].len = len;
        strncpy(mask[i].ch, charSets[i], len);

        printf("mask[%d] = %s (len=%d)\n", i, mask[i].ch, mask[i].len);

        keySpace *= mask[i].len;
    }

    printf("keyspace = %.4g, uint64 limit= %.4g\n",
        (float)keySpace, (float)UINT64_MAX);

    return keySpace;
}

// construct key from mask-based N-dimensional charset (each character position has its own charset)
// the sequence number decides what key is output
// the length of key, counters, and mask array must be == keyLen
static void maskedKey(char* key, uchar* counters, int keyLen, uint64_t sequence, const struct CLString* mask)
{
    for (int i = keyLen-1; i>= 0; i--)
    {
        const struct CLString* set = mask + i;
        uchar base = set->len;
        
        uchar digit = sequence % base;
        counters[i] = digit;
        key[i] = set->ch[digit];

        sequence /= base;        
    }
}

// construct the next key given the previous key
// this is considerably faster than maskedKey() since we stop after incrementing, leaving the
// rest of the key unchanged. also elimnates modulus and divide operation.
static void nextMaskedKey(char* key, uchar* counters, int keyLen, const struct CLString* mask)
{
    for (int i = keyLen-1; i>=0; i--)
    {
        if (counters[i] < mask[i].len-1)
        {
            uchar digit = counters[i] + 1;
            counters[i] = digit;
            key[i] = mask[i].ch[digit];
            break;
        }
        else
        {
            counters[i] = 0;
            key[i] = mask[i].ch[0];
        }
    }
}

QByteArray hmacMd5(QByteArray key, const QByteArray& message)
{
    const int blockSize = 64;
    if (key.length() > blockSize)
        QCryptographicHash::hash(key, QCryptographicHash::Md5);

    while (key.length() < blockSize)
        key.append('\0');

    QByteArray o_key_pad('\x5c', blockSize);
    o_key_pad.fill('\x5c', blockSize);

    QByteArray i_key_pad;
    i_key_pad.fill('\x36', blockSize);

    for (int i=0; i < blockSize; i++)
    {
        o_key_pad[i] = o_key_pad[i] ^ key[i];
        i_key_pad[i] = i_key_pad[i] ^ key[i];
    }

    return QCryptographicHash::hash(o_key_pad
        + QCryptographicHash::hash(i_key_pad + message, QCryptographicHash::Md5), QCryptographicHash::Md5);
}

static void testHmacMd5()
{
    const char* message = "the quick brown fox jumps over the lazy dog";
    const char* key = "asdf";
    const char* hash = "1ce02afc5439752bb89c098e29394be2";

    QByteArray hmac = hmacMd5(key, message);

    assert(hmac == QByteArray::fromHex(hash));
}

// control-C handling foo
static bool sigIntReceived = false;

extern "C" {

    static void sigIntHandler(int signal)
    {
        (void)signal;
        sigIntReceived=true;
    }
}

static void trapControlC()
{
    struct sigaction action;

    action.sa_handler = sigIntHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, NULL);
}

// test the key generation outside of OpenCL. Needed to verify results of OpenCL
static void testKeyGen()
{
    int keyLen = MASK_KEY_CHARS;
                        
    CLString mask[keyLen];
    char key[keyLen+1] = {0};
    uchar counters[keyLen] = {0};
    
    uint64_t keySpace = initMask(mask, kMaskCharSets, keyLen);

    uint64_t start = nanoTime();

    maskedKey(key, counters, keyLen, 0, mask);
    
    // the loop could take forever so allow control-C to exit
    trapControlC();

    uint64_t i;
    for (i = 0; i < keySpace; i++)
    {
        // check that nextMaskedKey() gives the same result as maskedKey()
        uchar counters2[keyLen] = {0};
        char  key2[keyLen];

        maskedKey(key2, counters2, keyLen, i, mask);

        if (memcmp(key, key2, keyLen) != 0)
            printf("ERROR: %s %s\n", key, key2);

        nextMaskedKey(key, counters, keyLen, mask);

        if (i % 1000000 == 0)
        {
            printf("%" PRIu64 ": %s\r", i, key);
            fflush(stdout);
        }

        if (sigIntReceived)
        {
            printf("\n");
            break;
        }
    }
    
    uint64_t end = nanoTime();
    
    printf("%.2f Mkey/s last=\"%s\"\n", i*1000.0f / (end-start), key);
}

// test OpenCL-based keygen and verify with C functions
static void testClKeyGen()
{
    uint64_t t0, t1;
    
    // load the kernel
    CLSimple cl("keygen");

    cl_int keyLen = MASK_KEY_CHARS;

    // number of keys we can fit in a buffer
    cl_int keyLenInts = (keyLen+3)/4;
    cl_int maxKeys = cl.maxBufferSize / (keyLenInts*4);

    // rounded to the max workgroup size since we will
    // dividing it up that way
    size_t localSize = cl.maxWorkGroupSize;
    size_t globalSize = (maxKeys / localSize) * localSize;
    
    // number of keys per iteration of the kernel, limited by
    // how many strings we can store in a buffer
    cl_int numKeys = globalSize;
    
    // divide globalSize since we will use get_global_id() as the offset to
    // each local division
    globalSize /= localSize;

    size_t outSize = sizeof(cl_uint)*keyLenInts*numKeys;
    cl_uint* out = (cl_uint*)malloc(outSize);
    cl.setOutputBuffer(outSize);
    
    size_t maskSize = sizeof(CLString)*keyLen;
    CLString* mask = (CLString*)malloc(maskSize);

    cl_ulong keySpace = initMask(mask, kMaskCharSets, keyLen);
    
    cl_mem maskBuffer;
    cl.createBuffer(&maskBuffer, CL_MEM_READ_ONLY, sizeof(*mask)*keyLen);
    cl.enqueueWriteBuffer(cl.command_queue, maskBuffer, maskSize, mask);
    
    cl.kernelSetArg(cl.kernel, 2, sizeof(maskBuffer), &maskBuffer);
    cl.kernelSetArg(cl.kernel, 3, cl.maxLocalMem, NULL);
    
    t0=nanoTime();
    
    printf("globalSize = %lu localSize=%lu buffSize=%lu MB Mkeys/iteration=%d\n",
        globalSize, localSize, outSize/1024/1024, (int)numKeys/1000000);

    assert(globalSize*localSize == (size_t)numKeys);
    assert(globalSize > localSize);
    assert(globalSize % localSize == 0);

    for (cl_ulong index = 0; index < keySpace; index += numKeys)
    {
        uchar counters[keyLen] = {0};
        char tmp[keyLen+1] = {0};
        maskedKey(tmp, counters, keyLen, index, mask);

        printf("%s\r", tmp);
        fflush(stdout);

        cl.kernelSetArg(cl.kernel, 1, sizeof(index), &index);
        cl.enqueueNDRangeKernel(cl.command_queue, cl.kernel, 1, &globalSize, &localSize);

        t1=nanoTime();

        // time without the read-back, never read-back when hashing
        printf("%s    : %dms, %.2f Mkey/sec, %.2f%%\r",
            tmp,
            (int)((t1-t0)/1000000),
            numKeys*1000.0f / (t1-t0),
            (index*100.0)/keySpace
            );
        t0=t1;

        cl.readOutput(out, outSize);

        // verify the result
        for (cl_int i = 0; i < numKeys; i++)
        {
            char* key = (char*)(out + i * keyLenInts);

            if (memcmp(key, tmp, keyLen) != 0)
            {
                printf("ERROR @ %d: got: ", i);
                for (int j= 0; j < keyLen; j++)
                    printf("%c", key[j]);
                printf(" expected: %s\n", tmp);
            }

            nextMaskedKey(tmp, counters, keyLen, mask);
        }

        t0=nanoTime();
    }
}

// test the multiblock OpenCL md5 with different length strings
static void testMd5()
{
    CLSimple cl("md5_local");

    size_t maxSize = cl.maxBufferSize / sizeof(CLString);

    size_t localSize = cl.maxWorkGroupSize / 2;
    size_t globalSize = (maxSize / localSize) * localSize;
    size_t numMsgs = globalSize;

    CLString* msgs = new CLString[numMsgs];
    cl_mem msgBuffer;
    cl.createBuffer(&msgBuffer, CL_MEM_READ_ONLY, numMsgs*sizeof(CLString));

    cl.setOutputBuffer(numMsgs*sizeof(Hash));

    Hash* hashes = new Hash[numMsgs];

    for (size_t msgLen = 1; msgLen <= 255; msgLen++)
    {
        assert(msgLen <= CLSTRING_MAX_LEN);

        // work elements copy string into local memory before hashing,
        // there must be enough room in the local memory to allow for that
        assert((cl.maxLocalMem >= (sizeof(CLString) * localSize)));

        cl.kernelSetArg(cl.kernel, 1, sizeof(msgBuffer), &msgBuffer);
        cl.kernelSetArg(cl.kernel, 2, cl.maxLocalMem, NULL);

        for (uint64_t seq = 0; seq < numMsgs*4; seq += numMsgs)
        {
            for (size_t i = 0; i < numMsgs; i++)
            {
                simpleKey(msgs[i].ch, msgLen, seq+i, "0123456789abcdef", 16);
                msgs[i].len = msgLen;
            }

            cl.enqueueWriteBuffer(cl.command_queue, msgBuffer, numMsgs*sizeof(CLString), msgs);
            cl.enqueueNDRangeKernel(cl.command_queue, cl.kernel, 1, &globalSize, &localSize);
            cl.readOutput(hashes, sizeof(Hash)*numMsgs);

            for (size_t i = 0; i < numMsgs; i++)
            {
                QByteArray msgBytes(msgs[i].ch, msgs[i].len);

                if (QByteArray((char*)(hashes[i].byte), 16) != QCryptographicHash::hash(msgBytes, QCryptographicHash::Md5))
                {
                    printf("%s: ", msgs[i].ch);
                    for (int j = 0; j < 16; j++)
                        printf("%.2x", hashes[i].byte[j]);

                    printf("   ..error");
                    printf("\n");
                }
            }

            printf("len=%d %d hashes\n", (int)msgLen, (int)seq);
        }
    }
}

// test OpenCL md5(key)
static void testMd5KeyGen()
{
    uint64_t t0 = nanoTime();
    uint64_t t1 = t0;    
 
    CLSimple cl("md5_keys");

    t1 = nanoTime();
    printf("init    : %dms\n", (int)((t1-t0)/1000000));
    t0 = t1;
    
    const int keyLen = MASK_KEY_CHARS;

    // largest buffer will be hashes, 16 bytes per.
    size_t maxHashes = cl.maxBufferSize / 16;
    
    size_t localSize = cl.maxWorkGroupSize;
    size_t globalSize = (maxHashes/localSize)*localSize;
    size_t numHashes = globalSize;
    
    size_t maskSize = sizeof(CLString)*keyLen;
    CLString* mask = (CLString*)malloc(maskSize);
    
    cl_ulong keySpace = initMask(mask, kMaskCharSets, keyLen);

    // key mask
    cl_mem maskBuffer;
    cl.createBuffer(&maskBuffer, CL_MEM_READ_ONLY, sizeof(*mask)*keyLen);
    cl.enqueueWriteBuffer(cl.command_queue, maskBuffer, maskSize, mask);
    cl.kernelSetArg(cl.kernel, 2, sizeof(maskBuffer), &maskBuffer);

    // scratch space
    cl.kernelSetArg(cl.kernel, 3, cl.maxLocalMem, NULL);

    // output buffer
    const size_t hashesSize = sizeof(Hash)*numHashes;
    
    Hash* hashes = new Hash[numHashes];
    memset(hashes, 0, hashesSize);
    
    printf("%d Mkeys, buffers: hashes=%dMB\n", 
        (int)numHashes/1000000,
        (int)hashesSize/1024/1024);
        
    cl.setOutputBuffer(hashesSize);

    assert(localSize*MASK_KEY_INTS <  cl.maxLocalMem);
    t1 = nanoTime();

    printf("setup   : %dms\n", (int)((t1-t0)/1000000));
    t0 = t1;
        
    globalSize /= localSize;
    assert(globalSize > localSize);
    assert(globalSize % localSize == 0);
  
    for (cl_ulong index = 0; index < keySpace; index += numHashes)
    {
        uchar counters[keyLen] = {0};
        char key[keyLen+1] = {0};

        maskedKey(key, counters, keyLen, index, mask);
        printf("\n%s ======================\n", key);

        cl.kernelSetArg(cl.kernel, 1, sizeof(index), &index);

        cl.enqueueNDRangeKernel(cl.command_queue, cl.kernel, 1, &globalSize, &localSize);

        t1 = nanoTime();
        printf("exec    : %dms, %.2f Mhash/sec\n",
            (int)((t1-t0)/1000000),
            numHashes*1000.0f / (t1-t0)
            );
        t0 = t1;

        cl.readOutput(hashes, hashesSize);

        t1 = nanoTime();
        printf("read    : %dms, %.2f Mhash/sec\n",
            (int)((t1-t0)/1000000),
            numHashes*1000.0f / (t1-t0)
            );
        t0 = t1;

        for (size_t i = 0; i < numHashes; i++)
        {
            QByteArray keyBytes(key, keyLen);

            if (QByteArray((char*)(hashes[i].byte), 16) != QCryptographicHash::hash(keyBytes, QCryptographicHash::Md5))
            {
                printf("%s: ", key);
                for (int j = 0; j < 16; j++)
                    printf("%.2x", hashes[i].byte[j]);

                printf("   ..error");
                printf("\n");
            }

            nextMaskedKey(key, counters, keyLen, mask);
        }

        t1 = nanoTime();
        printf("verify  : %dms\n", (int)((t1-t0)/1000000));
        t0 = t1;
    }

    return;
}

// find the key for a known hmac-md5 hash and known plaintext
static void hmacMd5Search()
{
    testHmacMd5();

    uint64_t t0 = nanoTime();
    uint64_t t1 = t0;

    struct {
        CLSimple* cl;
        cl_mem    maskBuffer, hashBuffer, pinBuffer[NUM_WRITE_BUFFERS];
        cl_command_queue outQueue;
    } gpus[NUM_GPUS];

    for (int i = 0; i < NUM_GPUS; i++)
        gpus[i].cl = new CLSimple("md5_hmac", i);
    
    t1 = nanoTime();
    printf("init    : %dms\n", (int) ((t1 - t0) / 1000000));
    t0 = t1;

    const int keyLen = MASK_KEY_CHARS;

    size_t maxKeys   = GLOBAL_WORK_SIZE; //cl.maxBufferSize;
    size_t localSize = LOCAL_WORK_SIZE;  //cl.maxWorkGroupSize;
    size_t globalSize = (maxKeys / localSize) * localSize;
    size_t numKeys = globalSize;

    // needs to be multiple of 8 due to optimized results check
    assert(numKeys % 8 == 0);

    size_t maskSize = sizeof(CLString) * keyLen;
    CLString* mask = (CLString*) malloc(maskSize);
    cl_ulong keySpace = initMask(mask, kMaskCharSets, keyLen);
    
    for (int i = 0; i < NUM_GPUS; i++)
        gpus[i].cl->createBuffer(&(gpus[i].maskBuffer), CL_MEM_READ_ONLY, maskSize);
    
    cl_uint numHashes = 0;

    const char* strHashes[MAX_HASHES];
    {
        QFile hashFile(HASH_FILENAME);
        assert(hashFile.open(QFile::ReadOnly));

        while (!hashFile.atEnd())
        {
            QString line = QString(hashFile.readLine()).trimmed();
            if (!line.startsWith("#"))
            {
                assert(numHashes < MAX_HASHES);
                strHashes[numHashes++] = strdup(qPrintable(line));
            }
        }
    }

    // hmac-md5
    assert(strlen(HMAC_MSG) == HMAC_MSG_CHARS);

    // load the target hashes
    cl_uint hashes[numHashes];
    size_t hashSize = sizeof(hashes);

    for (size_t i = 0; i < numHashes; i++)
    {
        printf("loading hash %d: %s\n", (int)i, strHashes[i]);
        uint32_t word = 0;
        for (int j = 0; j < 8; j++)
        {
            word <<= 4;
            word |= hex2int(strHashes[i][j]);
        }
        hashes[i] = swap32(word);
    }

    for (int i = 0; i < NUM_GPUS; i++)
        gpus[i].cl->createBuffer(&(gpus[i].hashBuffer), CL_MEM_READ_ONLY, hashSize);

    // 4 words per key (128 bit hash)
    typedef struct
    {
        uchar byte[16];
    } Hash;

    globalSize /= localSize;
    assert(globalSize > localSize);
    assert(globalSize % localSize == 0);

    int localMemPerGroup = 0;
#if !KEYGEN_USE_PRIVATE_MEM
    localMemPerGroup += KEY_SCRATCH_INTS;
#endif

#if !HMAC_USE_PRIVATE_MEM
    localMemPerGroup += HMAC_SCRATCH_INTS;
#endif

#if HMAC_USE_PRIVATE_MEM

#else
    printf("local mem needed=%d\n", (int)(localMemPerGroup*4*localSize));
#endif

    printf("globalSize=%lu localSize=%lu keySpace=%g keys/iteration=%dk\n",
            globalSize, localSize, (float) keySpace,
            (int) numKeys*LOOP_MULTIPLIER*NUM_GPUS / 1000);
    
    // output one byte per hash, a mask of the hashes that matched
    size_t outSize = numKeys;
    for (int i = 0; i < NUM_GPUS; i++)
    {
        auto gpu = gpus+i;
        
        for (int j = 0; j < NUM_WRITE_BUFFERS; j++)
            gpu->cl->createBuffer(&(gpu->pinBuffer[j]), CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR, outSize);

        gpu->cl->createCommandQueue(&(gpu->outQueue));

        gpu->cl->enqueueWriteBuffer(gpu->cl->command_queue, gpu->maskBuffer, maskSize, mask);
        gpu->cl->enqueueWriteBuffer(gpu->cl->command_queue, gpu->hashBuffer, hashSize, hashes);
        
        
        gpu->cl->kernelSetArg(gpu->cl->kernel, 2, sizeof(gpu->maskBuffer), &(gpu->maskBuffer));
        gpu->cl->kernelSetArg(gpu->cl->kernel, 3, sizeof(gpu->hashBuffer), &(gpu->hashBuffer));
        gpu->cl->kernelSetArg(gpu->cl->kernel, 4, sizeof(numHashes), &numHashes);
        
#if HMAC_USE_PRIVATE_MEM
#else
        assert(localMemPerGroup*4*localSize <  gpu->cl->maxLocalMem);
        gpu->cl->kernelSetArg(gpu->cl->kernel, 5, gpu->cl->maxLocalMem, NULL);
#endif
    }
    
    t1 = nanoTime();
    printf("setup   : %dms\n", (int) ((t1 - t0) / 1000000));
    t0 = t1;
    
    uchar counters[keyLen] = { 0 };
    char key[keyLen + 1] = { 0 };

    int prevIndex = 1;
    int currIndex = 0;

    //uint64_t r0, r1;
    //uint64_t c0, c1;
    //uint64_t v0, v1;
    //uint64_t k0, k1;
    // uint64_t w0, w1;
    uint64_t p0, p1;
    
    //r1 = r0 = c0 = c1 = v0 = v1 = k0 = k1 = w0 = w1 = 0;
    p0 = nanoTime();

    int loops = 0;
    
    
    // note: extra iteration added and ignored, so additional check outside loop isn't needed
    for (cl_ulong index = START_INDEX; index < keySpace + numKeys*LOOP_MULTIPLIER*NUM_GPUS; )//index += numKeys)
    {
        t1 = nanoTime();
        p1= t1;
        
        // print progress every once in a while
        if (p1-p0 > 1000000000)
        {
            QFile f("hash.state");
            f.open(QFile::WriteOnly);
            f.write(QString("0x%1").arg(index, 0, 16).toUtf8());
            f.close();

            maskedKey(key, counters, keyLen, index, mask);
            printf("\r0x%" PRIX64 " %s...", index, key);
            maskedKey(key, counters, keyLen, index + numKeys - 1, mask);

            uint64_t t;

            t = keySpace - index;
            t /= numKeys*LOOP_MULTIPLIER*NUM_GPUS;
            t *= (t1 - t0);
            t /= 1000000;

            QDateTime time = QDateTime::currentDateTime();

            time = time.addMSecs(t);

            printf("%s: %.2f Mkey/s [%s] %.2fms",
                    key,
                    loops*numKeys*NUM_GPUS*LOOP_MULTIPLIER * 1000.0f / (p1 - p0),
                    qPrintable(time.toString()),
                    (t1 - t0) / 1000000.0f);
            fflush(stdout);
     
            t1 = nanoTime();
            p1= t1;
            p0=p1;
            loops=0;
        }
        t0 = t1;

        int mappedIndex = prevIndex;

        
        for (int g = 0; g < NUM_GPUS; g++)
        {
            auto gpu = gpus+g;
            
            // wait for kernel completion
            clFinish(gpu->cl->command_queue);
        
            // queue the next group
            gpu->cl->kernelSetArg(gpu->cl->kernel, 0, sizeof(cl_mem), &(gpu->pinBuffer[currIndex]));
            gpu->cl->kernelSetArg(gpu->cl->kernel, 1, sizeof(index), &index);
            gpu->cl->enqueueNDRangeKernel(gpu->cl->command_queue, gpu->cl->kernel, 1, &globalSize,
                    &localSize, false);
            clFlush(gpu->cl->command_queue);
            

            // get pointer to the finished kernel's output
            const uchar* out = (const uchar*) clEnqueueMapBuffer(gpu->outQueue,
                gpu->pinBuffer[mappedIndex], CL_MAP_READ,
                CL_TRUE, 0, outSize, 0, NULL, NULL, NULL);

            //clFlush(gpu->outQueue);
            //assert(out);
            
            // wait for read completion, not needed since we used CL_TRUE in the map
            // note: should be faster to wait? but performance dropoff was huge
            //clFinish(gpu->outQueue);

            // check the output, 8 bytes at a time
            uint64_t* ptr = (uint64_t*) out;
            
            // skip first buffers, there is nothing in them since
            // we haven't returned from the first loop yet.
            if (index >= START_INDEX + numKeys*LOOP_MULTIPLIER*NUM_GPUS)             
            for (size_t k = 0; k < numKeys / 8; k++)
            {
                uint64_t tmp = ptr[k];
                if (tmp)
                {
                    //printf("\nmaybe match\n");  
                    // check each byte flag, one per group
                    for (size_t i = 0; i < 8; i++)
                    {
                        uchar byte = tmp & 0xff;
                        tmp >>= 8;
                        if (byte)
                        {
                            // each byte represents 8 possible keys, so we have
                            // 8*n checks to find which hashes matched
                            for (int n = 0; n < LOOP_MULTIPLIER; n++)
                            {
                                // kernel only compares the first 4 bytes of the hash (for speed),
                                // so there will be false positives
                                maskedKey(key, counters, keyLen,
                                        index + k*(LOOP_MULTIPLIER*8) + i*(LOOP_MULTIPLIER) + n - numKeys*NUM_GPUS*LOOP_MULTIPLIER, mask);

                                QByteArray a = hmacMd5(QByteArray(key, keyLen), QByteArray(HMAC_MSG, HMAC_MSG_CHARS));

                                //QByteArray a = QCryptographicHash::hash(
                                //        QByteArray(key, keyLen),
                                //        QCryptographicHash::Md5);

                                // find the hash that matches
                                for (size_t j = 0; j < numHashes; j++)
                                    //if (flag & (1 << j))
                                    {
                                        QByteArray b = QByteArray::fromHex(strHashes[j]);

                                        if (a == b)
                                            printf("\r%s:%s                                                              \n",
                                                   strHashes[j], key);
                                    }
                                
                            }  
                        }
                    }
                }
            }

            
            index += numKeys*LOOP_MULTIPLIER;

            clEnqueueUnmapMemObject(gpu->outQueue, gpu->pinBuffer[mappedIndex], (void*) out,
                0, NULL, NULL);
        }
        
        
        prevIndex = currIndex;
        currIndex = (currIndex + 1) % NUM_WRITE_BUFFERS;
        
        loops++;
    }

    printf("\n\n");
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    QCoreApplication app(argc, argv);

    QStringList args = app.arguments();
    if (args.count() == 2)
    {
        if (args[1] == "-testKeyGen")   testKeyGen();
        if (args[1] == "-testClKeyGen") testClKeyGen();
        if (args[1] == "-testMd5")      testMd5();
        if (args[1] == "-testMd5KeyGen")testMd5KeyGen();
    }
    else
    {
        hmacMd5Search();
    }

    return 0;
}
