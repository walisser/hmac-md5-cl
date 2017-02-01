# hmac-md5-cl
Experimental HMAC-MD5 brute-forcer in OpenCL. The salt value is fixed and the key value is brute forced (as an ascii text string). The HMAC algorithm follows the RFC version.

It has the following features:
- Multi-block MD5 hash in OpenCL (greater than 64 bytes salt/key supported)
- Key generator in OpenCL with N-way character set support

The purpose of this project is to study OpenCL. It probably isn't useful in practice, though some parts of it may be. For example, I could not find any examples of multi-block md5 when I started this project.

It has been tested on Radeon HD6870 using GalliumCompute (Clover) driver, and also on a pair of Radeon R295x2 (4 GPUs).

## Requirements
- OpenCL 1.1
- Qt5

## Compile
```bash
  qmake
  make
```

## Run Tests
```bash
# test key generator
./hmac-md5 -testClKeygen

# test md5
./hmac-md5 -testMd5

# test md5 of key sequence
./hmac-md5 -testMd5KeyGen

# run the brute forcer
# you should immediately see the key aaaaa000 was found,
# and shortly later abaaa000
./hmac-md5
```

## Configure
1. Edit config.h
  - set the salt value (HMAC_MSG)
  - set the key generator mask (MASK_KEY_CHARS, MASK_KEY_MASK)

2. Edit hashes.txt
  - add your hashes (hint: include a few known keys to validate)
  - up to 255 hashes are supported (see kernel.h MAX_HASHES)

3. Recompile

4. Run the brute forcer
```bash
./hmac-md5
```

## State file
hash.state contains the current position in the key sequence. To resume from this position, change START_INDEX in config.h and recompile.


## Status output
```bash
0x151400000 aijth4Dc...aijtv01r: 26.84 Mkey/s [Wed Feb 1 20:13:31 2017] 19.47ms
```
1. Current sequence index
2. Range of hashes being tested
3. Hash rate
4. Estimated completion date
5. Time to execute kernel (lower values allow GPU to do other useful things)
