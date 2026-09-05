# TGPC v1 corpus format

TGPC is the canonical data-only patch stream consumed by `rt-tgmr-train`.
Integers are little-endian. No native C/C++ structure is written. Version 1.0
contains fixed planar 7x7 RGB uint16 patches and has no executable content.

## Header (256 bytes)

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 8 | `RTTGPC1\0` |
| 8 | 2 | major version, 1 |
| 10 | 2 | minor version, 0 |
| 12 | 4 | header size, 256 |
| 16 | 4 | record size, 384 |
| 20 | 4 | patch edge, 7 |
| 24 | 4 | channel count, 3 |
| 28 | 4 | scalar enum, 1 = little-endian uint16 |
| 32 | 8 | reserved, zero |
| 40 | 8 | record count, at most 16,000,000 |
| 48 | 8 | training record count |
| 56 | 8 | validation record count |
| 64 | 8 | test record count |
| 72 | 8 | record-region offset, 256 |
| 80 | 8 | record-region bytes, count times 384 |
| 88 | 8 | complete uncompressed file size |
| 96 | 32 | exact source-manifest SHA-256 |
| 128 | 32 | concatenated record-region SHA-256 |
| 160 | 32 | corpus-packing configuration SHA-256 |
| 192 | 64 | reserved, zero |

## Record (384 bytes)

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 32 | SHA-256 of UTF-8 source ID |
| 32 | 4 | selected-source ordinal |
| 36 | 4 | source x coordinate |
| 40 | 4 | source y coordinate |
| 44 | 1 | split: 1 train, 2 validation, 3 test |
| 45 | 1 | augmentation kind |
| 46 | 1 | original orientation, 1 through 8 |
| 47 | 1 | reserved, zero |
| 48 | 2 | signed Q8 exposure stops |
| 50 | 6 | three unsigned Q12 white-balance gains |
| 56 | 2 | frozen camera-matrix ID |
| 58 | 2 | augmentation sequence |
| 60 | 8 | patch-sampling seed |
| 68 | 294 | 147 uint16 values: R 7x7, G 7x7, B 7x7 |
| 362 | 22 | reserved, zero |

Records occur in manifest source order and patch sequence order. The split
counts must agree with decoded records, the uncompressed stream must end exactly
after the last record, and the payload digest covers every 384-byte record
including reserved zero bytes.

The distributed `.tgpc.gz` is a single ordinary gzip stream over the complete
canonical TGPC bytes. It uses zlib, fixed compression level, timestamp zero, no
original filename/comment, and OS byte 255. Readers authenticate the
uncompressed TGPC contract and payload after streaming decompression.
