
#include "clsimple/clsimple.h"
#include "kernel.h"

#include <cstring>
#include <cstdio>
#include <cassert>
#include <cinttypes>

#include <csignal> // signal()
#include <sys/time.h> // gettimeofday()

#include <QtCore/QtCore>
#include <QtConcurrent/QtConcurrent>

#define swap32(num) \
        ((((num)>>24)&0xff) | \
        (((num)<<8)&0xff0000) | \
        (((num)>>8)&0xff00) |  \
        (((num)<<24)&0xff000000))

#define roundInt(x, y) (((x)/(y))*(y))

typedef union {
    uchar byte[16];
    char  ch[16];
} Hash;


static const char* kMaskCharSets[] = MASK_KEY_MASK;

static_assert(sizeof(kMaskCharSets) == MASK_KEY_CHARS*sizeof(char*), "invalid mask array or length");

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
    gettimeofday(&tv, nullptr);

    return  uint64_t(tv.tv_sec) * 1000000000ULL +
            uint64_t(tv.tv_usec * 1000);
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
    memset(mask, 0, sizeof(*mask) * size_t(keyLen));

    uint64_t keySpace=1;

    for (int i = 0; i < keyLen; i++)
    {
        int len = int(strlen(charSets[i]));
        assert(len < CLSTRING_MAX_LEN);

        mask[i].len = len & 0xff;
        strncpy(mask[i].ch, charSets[i], size_t(len));

        printf("mask[%d] = %s (len=%d)\n", i, mask[i].ch, mask[i].len);

        keySpace *= mask[i].len;
    }

    printf("keyspace = %.4g, uint64 limit= %.4g\n",
        double(keySpace), double(UINT64_MAX));

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
        
        uchar maskIndex = sequence % base;
        counters[i] = maskIndex;
        key[i] = set->ch[maskIndex];

        sequence /= base;        
    }
}

// construct the next key given the previous key
// this is considerably faster than maskedKey() since we stop after incrementing, leaving the
// rest of the key unchanged. also elimnates modulus and divide operation.
static void nextMaskedKey(char* key, uchar* counters, int keyLen, const struct CLString* mask)
{
    for (int i = keyLen-1; i >= 0; --i)
    {
        uchar maskIndex = counters[i]+1;
        if (maskIndex < mask[i].len)
        {
            counters[i] = maskIndex;
            key[i] = mask[i].ch[maskIndex];
            break;
        }
        else
        {
            counters[i] = 0;
            key[i] = mask[i].ch[0];
        }
    }
}

static QByteArray hmacMd5(QByteArray key, const QByteArray& message)
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

static void testCpuHmac()
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

    sigaction(SIGINT, &action, nullptr);
}

// test the key generation outside of OpenCL. Needed to verify results of OpenCL
static void testCpuKeyGen()
{
    const int keyLen = MASK_KEY_CHARS;
                        
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
    
    printf("%.2f Mkey/s last=\"%s\"\n", i*1000.0 / (end-start), key);
}

// test OpenCL-based keygen and verify with C functions
static void testKeyGen()
{
    uint64_t t0, t1;
    
    // load the kernel
    CLSimple cl("keygen");

    const size_t keyLen = MASK_KEY_CHARS;

    // number of keys we can fit in an output buffer
    const size_t keyLenInts = (keyLen+3)/4;
    const size_t maxKeys = cl.maxBufferSize / (keyLenInts*4);

    size_t localSize = LOCAL_WORK_SIZE; //cl.kernelWorkGroupSize; //cl.maxWorkGroupSize;
#if KEYGEN_USE_PRIVATE_MEM
    size_t localMemSize = 0;
#else
    const size_t localStride = (KEY_SCRATCH_INTS+MASK_KEY_CHARS)*4;
    size_t localMemSize = localStride*localSize;
    while (localMemSize > (cl.maxLocalMem-cl.kernelLocalMem))
    {
        localSize /= 2;
        localMemSize = localStride*localSize;
    }
    assert(localMemSize <= (cl.maxLocalMem-cl.kernelLocalMem));
    cl.kernelSetArg(cl.kernel, 3, localMemSize, nullptr);
#endif

    // globalSize is rounded to so it can be divided again for iterations and local work groups
    size_t globalSize = roundInt(maxKeys, localSize*LOOP_COUNT);

    // number of keys per iteration/enqueued kernel, limited by
    // how many strings we can store in a buffer
    const size_t numKeys = globalSize;

    // divide again since each work item will do LOOP_COUNT iterations
    globalSize /= LOOP_COUNT;

    const size_t outSize = sizeof(cl_uint)*keyLenInts*numKeys;
    cl_uint* out = new cl_uint[keyLenInts*numKeys];
    cl.setOutputBuffer(outSize);

    const size_t maskSize = sizeof(CLString)*keyLen;
    CLString* mask = new CLString[keyLen];
    cl_ulong keySpace = initMask(mask, kMaskCharSets, keyLen);
    
    cl_mem maskBuffer;
    cl.createBuffer(&maskBuffer, CL_MEM_READ_ONLY, maskSize);
    cl.enqueueWriteBuffer(cl.command_queue, maskBuffer, maskSize, mask);
    cl.kernelSetArg(cl.kernel, 2, sizeof(maskBuffer), &maskBuffer);

    t0=nanoTime();
    
    printf("globalWorkSize=%lu localWorkSize=%lu outMemSize=%lu inMemSize=%lu localMem=%lu\n",
        globalSize, localSize, outSize, size_t(0), localMemSize);

    assert(globalSize*LOOP_COUNT == numKeys);
    assert(globalSize > localSize);
    assert(globalSize % localSize == 0);

    for (cl_ulong index = 0; index < keySpace; index += numKeys)
    {
        uchar counters[keyLen] = {0};
        char tmp[keyLen+1] = {0};
        maskedKey(tmp, counters, keyLen, index, mask);

        cl.kernelSetArg(cl.kernel, 1, sizeof(index), &index);
        cl.enqueueNDRangeKernel(cl.command_queue, cl.kernel, 1, &globalSize, &localSize);

        t1=nanoTime();

        // time without the read-back, never read-back when hashing
        printf("%s    : %dms, %.2f Mkey/sec, %.2f%%\r",
            tmp,
            int((t1-t0)/1000000),
            numKeys*1000.0 / (t1-t0),
            (index*100.0)/keySpace
            );
        t0=t1;
        fflush(stdout);

        cl.readOutput(out, outSize);

        // verify the result
        for (size_t i = 0; i < numKeys; i++)
        {
            char* key = reinterpret_cast<char*>(out + i * keyLenInts);
            //printf("%d: %s\n", int(i), tmp);
            if (memcmp(key, tmp, keyLen) != 0)
            {
                printf("ERROR @ %d: got: ", int(i));
                for (size_t j= 0; j < keyLen; j++)
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

    const size_t maxSize = cl.maxBufferSize / sizeof(CLString);

    size_t localSize = LOCAL_WORK_SIZE; //cl.kernelWorkGroupSize; //cl.maxWorkGroupSize;

#if MD5_USE_PRIVATE_MEM
    size_t localMemSize = 0;
#else
    const size_t localStride = sizeof(CLString);
    size_t localMemSize = localStride*localSize;
    while (localMemSize > (cl.maxLocalMem-cl.kernelLocalMem))
    {
        localSize /= 2;
        localMemSize = localStride*localSize;
    }
    assert(localMemSize <= (cl.maxLocalMem-cl.kernelLocalMem));
    cl.kernelSetArg(cl.kernel, 2, localMemSize, nullptr);
#endif

    size_t globalSize = roundInt(maxSize, localSize);
    //globalSize = qMin(size_t(localSize*2), globalSize);
    const size_t numMsgs = globalSize;

    Hash* hashes = new Hash[numMsgs];
    const size_t hashesSize = sizeof(Hash)*numMsgs;
    assert(hashesSize <= cl.maxBufferSize);
    cl.setOutputBuffer(hashesSize);

    CLString* msgs = new CLString[numMsgs];
    cl_mem msgBuffer;
    const size_t msgSize = sizeof(CLString)*numMsgs;

    assert(msgSize <= cl.maxBufferSize);
    cl.createBuffer(&msgBuffer, CL_MEM_READ_WRITE, msgSize);
    cl.kernelSetArg(cl.kernel, 1, sizeof(msgBuffer), &msgBuffer);

    printf("globalWorkSize=%lu localWorkSize=%lu outMemSize=%lu inMemSize=%lu localMem=%lu\n",
        globalSize, localSize, hashesSize, msgSize, localMemSize);

    QVector<QFuture<int>> work;
    QMutex stdioMutex;

    for (int msgLen = 1; msgLen <= 255; msgLen++)
    {
        assert(msgLen <= CLSTRING_MAX_LEN);

        for (uint64_t seq = 0; seq < numMsgs*4; seq += numMsgs)
        {
            for (size_t i = 0; i < numMsgs; i++)
            {
                simpleKey(msgs[i].ch, msgLen, seq+i, "0123456789abcdef", 16);
                msgs[i].len = msgLen & 0xff;
            }

            cl.enqueueWriteBuffer(cl.command_queue, msgBuffer, msgSize, msgs);
            cl.enqueueNDRangeKernel(cl.command_queue, cl.kernel, 1, &globalSize, &localSize);
            cl.readOutput(hashes, hashesSize);

            for (size_t i = 0; i < numMsgs; i+=64*1024)
            {
                work.append(QtConcurrent::run([&,i]() {

                    const size_t j = qMin(i+64*1024, numMsgs);
                    for (size_t k = i; k < j; ++k)
                    {
                        QByteArray msgBytes(msgs[k].ch, msgs[k].len);

                        if (QByteArray(hashes[k].ch, 16) != QCryptographicHash::hash(msgBytes, QCryptographicHash::Md5))
                        {
                            QMutexLocker lock(&stdioMutex);
                            printf("\nerror @ len=%d %s : ", msgs[k].len, qPrintable(QString(msgBytes)));
                            for (int l = 0; l < 16; ++l)
                                printf("%.2x", hashes[k].byte[l]);

                            printf("\n");
                        }
                    }
                    return 0;
                }));
            }
            for (auto& w : work)
                w.waitForFinished();

            printf("msglen=%d %.2d%% of %dk hashes OK   \r",
                   int(msgLen),
                   int((seq+numMsgs)*100/(numMsgs*4)),
                   int(numMsgs/1024));
            fflush(stdout);
        }
    }
    printf("\n");
}

// test OpenCL md5(key)
static void testMd5KeyGen()
{
    uint64_t t0 = nanoTime();
    uint64_t t1 = t0;    
 
    CLSimple cl("md5_keys");

    t1 = nanoTime();
    printf("init    : %dms\n", int((t1-t0)/1000000));
    t0 = t1;
    
    const int keyLen = MASK_KEY_CHARS;

    size_t localSize = LOCAL_WORK_SIZE;//cl.kernelWorkGroupSize; //cl.maxWorkGroupSize;

#if KEYGEN_USE_PRIVATE_MEM
    size_t localMemSize = 0;
#else
    const size_t localStride = (MASK_KEY_INTS+MASK_COUNTER_INTS)*4;
    size_t localMemSize = localStride*localSize;
    while (localMemSize > (cl.maxLocalMem-cl.kernelLocalMem))
    {
        localSize /= 2;
        localMemSize = localStride*localSize;
    }
    assert(localMemSize <= (cl.maxLocalMem-cl.kernelLocalMem));
    cl.kernelSetArg(cl.kernel, 3, localMemSize, nullptr);
#endif

    // largest buffer will be hashes, 16 bytes per.
    const size_t maxHashes = cl.maxBufferSize / 16;
    
    size_t globalSize = roundInt(maxHashes, localSize*LOOP_COUNT);
    const size_t numHashes = globalSize;
    globalSize /= LOOP_COUNT;

    assert(globalSize > localSize);
    assert(globalSize % localSize == 0);

    // keygen mask
    const size_t maskSize = sizeof(CLString)*keyLen;
    CLString* mask = new CLString[keyLen];
    cl_ulong keySpace = initMask(mask, kMaskCharSets, keyLen);

    cl_mem maskBuffer;
    cl.createBuffer(&maskBuffer, CL_MEM_READ_ONLY, maskSize);
    cl.enqueueWriteBuffer(cl.command_queue, maskBuffer, maskSize, mask);
    cl.kernelSetArg(cl.kernel, 2, sizeof(maskBuffer), &maskBuffer);

    // output buffer
    const size_t hashesSize = sizeof(Hash)*numHashes;
    
    Hash* hashes = new Hash[numHashes];
    memset(hashes, 0, hashesSize);
    
    printf("%d Mkeys, buffers: hashes=%dMB\n", 
        int(numHashes)/1000000,
        int(hashesSize)/1024/1024);
        
    cl.setOutputBuffer(hashesSize);

    t1 = nanoTime();

    printf("setup   : %dms\n", int(((t1-t0)/1000000)));
    t0 = t1;

    printf("globalWorkSize=%lu localWorkSize=%lu outMemSize=%lu inMemSize=%lu localMem=%lu\n",
        globalSize, localSize, hashesSize, size_t(0), localMemSize);

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
            int((t1-t0)/1000000),
            numHashes*1000.0 / (t1-t0)
            );
        t0 = t1;

        cl.readOutput(hashes, hashesSize);

        t1 = nanoTime();
        printf("read    : %dms, %.2f Mhash/sec\n",
            int((t1-t0)/1000000),
            numHashes*1000.0 / (t1-t0)
            );
        t0 = t1;

        for (size_t i = 0; i < numHashes; i++)
        {
            QByteArray keyBytes(key, keyLen);

            if (QByteArray(hashes[i].ch, 16) != QCryptographicHash::hash(keyBytes, QCryptographicHash::Md5))
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
        printf("verify  : %dms\n", int((t1-t0)/1000000));
        t0 = t1;
    }

    return;
}

// find the key for a known hmac-md5 hash and known plaintext
static void hmacMd5Search(const size_t startingIndex)
{
    testCpuHmac();

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
    printf("init    : %dms\n", int((t1 - t0) / 1000000));
    t0 = t1;

    const int keyLen = MASK_KEY_CHARS;

    const size_t maxKeys   = GLOBAL_WORK_SIZE; //cl.maxBufferSize;
    const size_t localSize = LOCAL_WORK_SIZE;  //cl.maxWorkGroupSize;
    size_t globalSize = roundInt(maxKeys, localSize*LOOP_COUNT);
    size_t numKeys = globalSize;

    // needs to be multiple of 8 due to optimized results check
    assert(numKeys % 8 == 0);

    globalSize /= LOOP_COUNT;

    const size_t maskSize = sizeof(CLString) * keyLen;
    CLString* mask = new CLString[keyLen];
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

    // the hmac plaintext is a constant in config.h
    printf("target message = \"%s\" len=%d\n", HMAC_MSG, int(strlen(HMAC_MSG)));
    assert(strlen(HMAC_MSG) == HMAC_MSG_CHARS);

    // load the target hashes
    cl_uint* hashes = new cl_uint[numHashes];
    const size_t hashSize = sizeof(*hashes)*numHashes;

    for (size_t i = 0; i < numHashes; i++)
    {
        printf("target hash %d: %s\n", int(i), strHashes[i]);
        uint32_t word = 0;
        for (int j = 0; j < 8; j++)
        {
            word <<= 4;
            word |= hex2int(strHashes[i][j]) & 0xf;
        }
        hashes[i] = swap32(word);
    }

    for (int i = 0; i < NUM_GPUS; i++)
        gpus[i].cl->createBuffer(&(gpus[i].hashBuffer), CL_MEM_READ_ONLY, hashSize);

    assert(globalSize > localSize);
    assert(globalSize % localSize == 0);

    // check local memory constraints
    // localStride is offset in local memory between wgs
    size_t localStride = 0;
#if !KEYGEN_USE_PRIVATE_MEM
    localStride += KEY_SCRATCH_INTS*4;
#endif

#if !HMAC_USE_PRIVATE_MEM
    localStride += HMAC_SCRATCH_INTS*4;
#endif

    const size_t localMemSize = localStride*localSize;

    // output one byte per hash, if loop multiplier enabled, output LOOP_MULTIPLER hashes per byte
    size_t outSize = numKeys;

    printf("globalSize=%lu localSize=%lu localMem=%lu outSize=%lu keySpace=%g keys/iteration=%dk\n",
            globalSize, localSize, localMemSize, outSize, double(keySpace),
            int(numKeys*LOOP_MULTIPLIER*NUM_GPUS / 1000));
    
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
        assert(localMemSize <  gpu->cl->maxLocalMem - gpu->cl->kernelLocalMem);
        gpu->cl->kernelSetArg(gpu->cl->kernel, 5, localMemSize, nullptr);
#endif
    }
    
    t1 = nanoTime();
    printf("setup   : %dms\n", int((t1 - t0) / 1000000));
    t0 = t1;

    uchar counters[keyLen] = {0};
    char key[keyLen + 1] = {0};

    int prevIndex = 1;
    int currIndex = 0;

    uint64_t worstVerify = 0;

    uint64_t p0, p1;
    p0 = nanoTime();

    size_t loops = 0;
    
    // note: extra iteration added and ignored, so additional check outside loop isn't needed
    for (cl_ulong index = startingIndex; index < keySpace + numKeys*LOOP_MULTIPLIER*NUM_GPUS; )
    {
        t1 = nanoTime();
        p1 = t1;
        
        // print progress every once in a while
        if (p1-p0 > 1000000000)
        {
            // save progress for -resume
            // ideally this would also indicate if the saved progress
            // is valid for the current configuration
            QFile f("hash.state");
            f.open(QFile::WriteOnly);
            f.write(QString("0x%1").arg(index, 0, 16).toUtf8());
            f.close();

            maskedKey(key, counters, keyLen, index, mask);
            printf("\r0x%" PRIX64 " %s...", index, key);
            maskedKey(key, counters, keyLen, index + numKeys - 1, mask);

            // approximate remaining time
            uint64_t t;
            t = keySpace - index;
            t /= numKeys*LOOP_MULTIPLIER*NUM_GPUS;
            t *= (t1 - t0);
            t /= 1000000;
            QDateTime time = QDateTime::currentDateTime();
            time = time.addMSecs(qint64(t));

            printf("%s: %.2f Mkey/s [%s] kernel=%.2fms verify=%.2fms          ",
                    key,
                    loops*numKeys*NUM_GPUS*LOOP_MULTIPLIER * 1000.0 / (p1 - p0),
                    qPrintable(time.toString()),
                    (t1 - t0) / 1000000.0,
                    worstVerify / 1000000.0);
            fflush(stdout);
     
            t1 = nanoTime();
            p1 = t1;
            p0 = p1;
            loops = 0;
        }
        t0 = t1;

        int mappedIndex = prevIndex;
        for (int g = 0; g < NUM_GPUS; g++)
        {
            auto gpu = gpus+g;
            
            // the finish/flush is required since we are going to
            // read from the buffer that just finished
            clFinish(gpu->cl->command_queue);
        
            // queue the next group
            gpu->cl->kernelSetArg(gpu->cl->kernel, 0, sizeof(cl_mem), &(gpu->pinBuffer[currIndex]));
            gpu->cl->kernelSetArg(gpu->cl->kernel, 1, sizeof(index), &index);
            gpu->cl->enqueueNDRangeKernel(gpu->cl->command_queue, gpu->cl->kernel, 1, &globalSize, &localSize, false);

            // this doesn't seem to be necessary
            //clFlush(gpu->cl->command_queue);
            
            // get pointer to the finished kernel's output
            uint64_t* out = static_cast<uint64_t*>(clEnqueueMapBuffer(gpu->outQueue,
                gpu->pinBuffer[mappedIndex], CL_MAP_READ,
                CL_TRUE, 0, outSize, 0, nullptr, nullptr, nullptr));

            assert(out);

            // wait should not be needed since we used CL_TRUE in the map
            //clFinish(gpu->outQueue);

            // check the output, 8 bytes at a time
            
            uint64_t v0 = nanoTime();

            // skip first buffer(s), there is nothing in them since
            // we haven't returned from the first iteration yet.
            if (index >= startingIndex + numKeys*LOOP_MULTIPLIER*NUM_GPUS)
            for (size_t k = 0; k < numKeys / 8; k++)
            {
                uint64_t tmp = out[k];
                if (tmp)
                {
                    // maybe match: check each byte of the uint64
                    // kernel only compares the first 4 bytes of the hash (for speed),
                    // so there will be false positives
                    for (size_t i = 0; i < sizeof(tmp); i++)
                    {
                        uchar byte = tmp & 0xff;
                        tmp >>= 8;
                        if (byte)
                        {
                            // each byte represents LOOP_MULTIPLIER possible keys
                            for (int n = 0; n < LOOP_MULTIPLIER; n++)
                            {
                                maskedKey(key, counters, keyLen,
                                        index +
                                            k*(LOOP_MULTIPLIER*sizeof(tmp)) +     // 8*N keys per uint64
                                            i*LOOP_MULTIPLIER +                   // offset in the uint64
                                            n -                                   // which of the N keys
                                            numKeys*NUM_GPUS*LOOP_MULTIPLIER,     // index is ahead of us by this amount (same as skip amount above)
                                          mask);

                                const QByteArray a = hmacMd5(QByteArray(key, keyLen), QByteArray(HMAC_MSG, HMAC_MSG_CHARS));

                                // find the hash that matches
                                for (size_t j = 0; j < numHashes; j++)
                                {
                                    const QByteArray b = QByteArray::fromHex(strHashes[j]);
                                    if (a == b)
                                        printf("\r%s:%s                                                              \n",
                                               strHashes[j], key);
                                }
                            }  
                        }
                    }
                }
            }
            
            uint64_t v1 = nanoTime();
            worstVerify = qMax(worstVerify, v1-v0);

            index += numKeys*LOOP_MULTIPLIER;

            clEnqueueUnmapMemObject(gpu->outQueue, gpu->pinBuffer[mappedIndex], out,
                0, nullptr, nullptr);
        }
        
        prevIndex = currIndex;
        currIndex = (currIndex + 1) % NUM_WRITE_BUFFERS;
        
        loops++;
    }

    printf("\n\n");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.count() > 1)
    {
        const QString arg = args[1];
        if (arg == "-testCpuKeyGen") testCpuKeyGen();
        if (arg == "-testKeyGen")    testKeyGen();
        if (arg == "-testMd5")       testMd5();
        if (arg == "-testMd5KeyGen") testMd5KeyGen();
        if (arg == "-resume")
        {
            QFile stateFile("hash.state");
            if (!stateFile.exists() || !stateFile.open(QFile::ReadOnly))
                qFatal("missing or unreadable state file");

            const QString str = stateFile.readAll();
            bool ok = false;
            const size_t index = str.toULongLong(&ok, 16);
            if (ok)
                hmacMd5Search(index);
        }
    }
    else
        hmacMd5Search(0x0);

    return 0;
}
