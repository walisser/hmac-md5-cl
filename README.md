# hmac-md5-cl
Experimental <a href="https://en.wikipedia.org/wiki/HMAC">HMAC-MD5</a> brute-forcer in OpenCL. The salt/message value is fixed and the key value is brute forced (as an ascii text string). The HMAC algorithm follows the RFC 2104 version with 64-byte blocks (e.g. `openssl dgst -md5 -hmac <key>`, php hash_hmac()).

The purpose of this project is to study OpenCL. There are lots of tunable parameters to experiment with, and it's a bit more difficult than plain md5.


Main features:
- Multi-block MD5 (greater than 64 bytes salt/message supported)
- Multi-GPU
- Multi-forcer (tests multiple hmac-md5 hashes simultaenously, up to 255)
- Lots of tunable parameters (see config.h)

Requirements:
- OpenCL 1.1
- Qt5

Verified hardware:
- Radeon 6870, GalliumCompute (Clover)
- Radeon R295x2, AMD OpenCL
- Nvidia 1050ti, NVidia OpenCL


## Compile
```bash
  qmake
  make
```

## Testing

If any of these fail, there is no point running the hmac forcer.

```bash
# let this run for a while and stop it
./hmac-md5 -testKeygen

# let run to completion
./hmac-md5 -testMd5

# run this for a while
./hmac-md5 -testMd5KeyGen

# run the brute forcer
# you should immediately see the key aaaaa000 was found,
# and shortly later abaaa000
./hmac-md5
```

## Running
1. Edit config.h
  - set the salt/plaintext value (HMAC_MSG, HMAC_MSG_CHARS)
  - set the key generator charset (MASK_KEY_MASK, MASK_KEY_CHARS)
  - edit tunables

2. Edit hashes.txt
  - add your hashes (hint: include a few known keys to validate)
  - up to 255 hashes are supported (see kernel.h MAX_HASHES)

3. Recompile

4. Run the brute forcer
```bash
./hmac-md5
```

## Resuming
You can stop hashing (Control-C, or gpu crash ;-), tune the parameters, and resume where you left off with `-resume` switch. Note this is only valid if the key sequence configuration hasn't changed.


## Status output
```bash
0x151400000 aijth4Dc...aijtv01r: 26.84 Mkey/s [Wed Feb 1 20:13:31 2017] 19.47ms
```
1. Current sequence index (saved to hash.state file)
2. Range of keys being tested
3. Hash rate
4. Estimated completion date/time
5. Time to execute kernel (lower values allow GPU to do other useful things)

