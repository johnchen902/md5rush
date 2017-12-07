# MD5Rush
Find md5 hashes prefixed with zeroes.

## Dependency
* Boost
* Fix amd64magic.h if you'd like to use AVX512
```
$ ldd ./md5sum | grep -oP '\S*(?= =>)'
libpthread.so.0
libboost_thread.so.1.65.1
libboost_system.so.1.65.1
libstdc++.so.6
libm.so.6
libgcc_s.so.1
libc.so.6
/lib64/ld-linux-x86-64.so.2
librt.so.1
```

## Usage
```
$ ./md5rush -h
Usage: ./md5rush [OPTION]...

  -z ZEROES      number of zeroes to look for (mandantory)
  -t THREADS     number of threads to use
                 (0: use std::thread::hardware_concurrency())
  -p PREFIXFILE  read prefix from PREFIXFILE
  -o OUTPUTFILE  write result to OUTPUTFILE
  -b BLOCKSIZE   tunable parameter: number of hashes per work
                 default: 10000
```
