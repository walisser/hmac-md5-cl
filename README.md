# hmac-md5-cl
OpenCL HMAC-MD5 brute-forcer with support for long salts / keys. The salt is a constant value (see config.h) and the key is brute forced.

## Requirements
- OpenCL 1.1
- Qt5

## Configuration

1. config.h
- set the salt value (HMAC_MSG)
- set the key generator mask (MASK_KEY_CHARS, MASK_KEY_MASK)

2. hashes.txt
- add your hashes

3. compile
```bash
qmake
make
```

## Running
```bash
./hmac-md5
```

