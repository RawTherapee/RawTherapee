# G5 HEIF input checks

The `heif-import-checks` target uses the production standard-image loader,
frame metadata, thumbnail and inspector paths. Enable it with the existing
`WITH_HEIF_EXPORT_TESTS` developer option. It generates small synthetic HEVC
fixtures; no external photographs are needed. Generated files remain in the
ignored build directory.

```sh
PKG_CONFIG_PATH="$PWD/build/heif-g1/deps/install/lib/pkgconfig" \
  nice -n 10 cmake --build build/heif-g1/rt-on \
  --target rth rth-cli heif-import-checks heif-export-checks heif-metadata-checks -j4
OMP_NUM_THREADS=4 nice -n 10 ctest --test-dir build/heif-g1/rt-on \
  --output-on-failure -j1
nice -n 10 python3 tests/heif/import/check-import.py \
  build/heif-g1/rt-on/rtgui/rawtherapee-cli \
  build/heif-g1/rt-on/tests/heif/export/import-artifacts \
  build/heif-g1/g5-cli
nice -n 10 python3 tests/heif/workflows/check-gui.py \
  build/heif-g1/rt-on build/heif-g1/g5-gui-final-on
```

Repeat the build and CTest commands for `rt-off` to check disabled support.
The GTK workflow check needs a local display and uses isolated settings/cache.
It checks browser cache entries, inspector and editor input, then exports an
exposure-adjusted HEIF input through the production batch queue.
The Python check needs FFmpeg with AV1 encoding, FFprobe, and ExifTool. It
compares RGB16 loader output with an independent FFmpeg decode, then exports
HEIF input through the real CLI to JPEG, PNG, TIFF and HEIF. It also checks
AVIF renamed as HEIC, HDR, and malformed input rejection. The C++ harness
checks exact pixel placement for all eight orientations, crop and a 2x2 grid;
it does not rely on FFmpeg's interpretation of HEIF transformations.

Primary EXIF/XMP metadata is selected by libheif and parsed by Exiv2 from
buffers in every build, including when Exiv2 has BMFF support. Native Exiv2
0.28.8 misses grid metadata and its BMFF object rejects metadata setters used
by sidecar merging. An in-memory metadata image avoids both differences.
The harness checks grid and multi-image primary metadata, invalid TIFF offsets,
and sidecar merges. On Linux with Exiv2 >=0.28, a test interposer makes native
file opening fail while retaining the real Exiv2 buffer parsers; the production
frame/input path must still work. An actual Exiv2 build without BMFF remains
a packaging-matrix check.

For missing-decoder checks, run the import binary with the generated fixture
directory followed by `--decoder-unavailable`, using the plugin-enabled
libheif and an empty or encoder-only `LIBHEIF_PLUGIN_PATH` (see the G2 export
codec script). Decoder-only input must also work without an encoder.

Supported input is the HEVC primary still image or HEVC grid, with an RGB ICC
profile or supported SDR NCLX description. Supported NCLX primaries are BT.709,
BT.2020 and Display P3; transfer functions are BT.709/BT.601/BT.2020, sRGB,
gamma 2.2/2.8 and linear. Libheif performs matrix/range conversion while source
primaries and transfer are preserved for LittleCMS. PQ/HLG, unknown color
spaces, unsupported matrices, invalid ICC and premultiplied transparency fail
with diagnostics. Straight alpha is discarded like PNG input; auxiliary depth
and gain maps are not edited. Container transforms take precedence over EXIF
orientation. `.hif` is accepted for input; export retains `.heic`/`.heif`.

The Linux development baseline currently uses libde265 1.0.16. The pinned
libheif only forwards image-size security limits into the decoder with
libde265 >=1.1.0. For G6 packages use libde265 >=1.1.3, which also contains the
subsequent high-bit-depth and Range Extensions fixes, then rerun these checks.
See [upstream release notes](https://github.com/strukturag/libde265/releases).
Container limits remain enabled, tile workers are disabled, and each decoder
uses one codec worker in the G5 implementation.

The pinned libheif can lose the returned VUI-only NCLX description during planar
RGB conversion. When a container NCLX is absent, the loader first decodes the
native planes to retain the stream's color description and check HDR/matrix
support, releases those planes, then requests RGB. This costs a second decode
for those inputs, including ICC-only files. It avoids assuming sRGB or losing
HDR signaling; container-NCLX inputs use one pixel decode.

The recorded Linux run covers 66 accepted fixtures and 63 independent decoder
and CLI checks, plus rejection and codec-absence cases. Results and source
hashes are in `reference-linux.json`. Linux CTest passes 4/4 enabled and 3/3
disabled checks; both GTK configurations pass. Windows/macOS remain unverified.

Native Windows/macOS and MinGW/Wine verification remain deferred to G6.
