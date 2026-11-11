# TGMR v2 model format

TGMR v2 is the data-only model container for the fixed X-Trans
K32/S9/top-8 Student-t GMR inference graph. All integers and IEEE-754 float32
values are little-endian. No native structure, graph program, path, timestamp,
or executable code is stored.

## Header (512 bytes)

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 8 | `RTTGMR2\0` |
| 8 | 2 | major version, 2 |
| 10 | 2 | minor version, 0 |
| 12 | 4 | header size, 512 |
| 16 | 4 | endian marker, `0x01020304` |
| 20 | 4 | flags, zero |
| 24 | 4 | architecture, 1 |
| 28 | 4 | reviewed model revision, nonzero |
| 32 | 4 | scalar type, 1 = float32 |
| 36 | 4 | patch edge, 7 |
| 40 | 4 | phases, 18 |
| 44 | 4 | components, 32 |
| 48 | 4 | coarse support edge, 3 |
| 52 | 4 | shortlist size, 8 |
| 56 | 4 | observed values, 49 |
| 60 | 4 | predicted center components, 2 |
| 64 | 4 | Student-t nu, 3.0 |
| 68 | 4 | posterior temperature, 4.0 |
| 72 | 4 | regularization tau, 0.0003 |
| 76 | 4 | section count, 1 |
| 80 | 4 | directory-record bytes, 96 |
| 84 | 4 | reserved, zero |
| 88 | 8 | directory offset, 512 |
| 96 | 8 | directory bytes, 96 |
| 104 | 8 | payload offset, 640 |
| 112 | 8 | payload bytes, 6,073,128 |
| 120 | 8 | complete file bytes, 6,073,768 |
| 128 | 32 | TGPC payload SHA-256 |
| 160 | 32 | canonical trainer-configuration SHA-256 |
| 192 | 32 | phase-payload SHA-256 |
| 224 | 32 | corpus attribution SHA-256 |
| 256 | 32 | trainer revision SHA-256 |
| 288 | 32 | SHA-256 of `rawtherapee-xtrans-tgmr-v2-k32-s9-q8` |
| 320 | 32 | reserved, zero |
| 352 | 32 | container authentication SHA-256 |
| 384 | 128 | reserved, zero |

The authentication digest is SHA-256 over the complete file after replacing
bytes 352 through 383 with zeros. The whole-file SHA-256 remains in the
companion manifest because a file cannot contain its own digest.

## Directory record (96 bytes)

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 4 | section type, 1 = phase data |
| 4 | 4 | encoding, 1 = K32/S9/q8 |
| 8 | 8 | flags/reserved, zero |
| 16 | 8 | absolute payload offset, 640 |
| 24 | 8 | payload bytes, 6,073,128 |
| 32 | 8 | reserved, zero |
| 40 | 32 | phase-payload SHA-256 |
| 72 | 24 | reserved, zero |

Bytes 608 through 639 are canonical zero padding.

## Phase payload

There are exactly 18 consecutive 337,396-byte phase records. Each phase stores,
in order:

1. 49 uint32 observed planar RGB indices and a three-uint32 sampled/target
   channel permutation;
2. 32 log mixture weights;
3. 32x49 observed means and 32x2 target means;
4. 32 full 49x49 lower Cholesky matrices and 32 log determinants;
5. 32x2x49 conditional regression gains;
6. the fixed nine central support positions `16,17,18,23,24,25,30,31,32`;
7. 32 coarse 9x9 lower Cholesky matrices and 32 log determinants.

All coefficient arrays are contiguous float32. Phase observation patterns must
be unique, observed spatial positions must form permutations, channel contracts
must be valid, and every Cholesky diagonal must be finite and positive. Readers
reject any trailing bytes or alternate layout.

Installed-data discovery accepts only the whole-file identity compiled into a
reviewed release. `RT_XTRANS_TGMR_MODEL` is an explicit compatible custom-model
override, but it receives the same complete structural and cryptographic
validation.
