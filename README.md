# MD5Rush
Find md5 hashes prefixed with zeroes.

## Usage
* Fix `may_have_zero` in md5rush-simd.cpp if you'd like to use AVX512.

```
$ md5rush-master/md5rush-master.py --help
usage: md5rush-master.py [-h] (--rush | -z ZEROES) [-p PREFIX_FILE]
                         [-o OUTPUT_FILE]
                         slave_config

positional arguments:
  slave_config          JSON file listing all slaves

optional arguments:
  -h, --help            show this help message and exit
  --rush                original md5rush mode
  -z ZEROES, --zeroes ZEROES
                        number of zeroes to look for
  -p PREFIX_FILE, --prefix-file PREFIX_FILE
                        read prefix from PREFIX_FILE
  -o OUTPUT_FILE, --output-file OUTPUT_FILE
                        write result to OUTPUT_FILE
```

### Sample config

```json
[
    {
        "name": "simd-cpu0",
        "command": "taskset 0x01 chrt -b 0 nice md5rush-simd/md5rush-simd",
        "block-size": 16777216
    }, {
        "name": "simd-cpu1",
        "command": "taskset 0x02 chrt -b 0 nice md5rush-simd/md5rush-simd",
        "block-size": 16777216
    }, {
        "name": "opencl",
        "command": "md5rush-opencl/md5rush-opencl",
        "block-size": 16777216
    }
]
```
