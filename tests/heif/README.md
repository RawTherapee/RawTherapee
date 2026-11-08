# HEIF dependency and codec feasibility

This standalone C++17 project uses RawTherapee's `cmake/Heif.cmake` discovery
module. It exercises the library and codecs without adding export or import
to RawTherapee. RawTherapee and the harness require libheif **1.23.6 or newer**
when `WITH_HEIF=ON`; `AUTO` disables HEIF if that version is absent, and `OFF`
does not query libheif. Runtime codecs remain separate requirements.

## Ubuntu development packages

For an existing RawTherapee development checkout:

```sh
sudo apt install libheif-dev libheif-plugin-libde265 libheif-plugin-x265 \
  libheif-examples libde265-dev libx265-dev
```

The plugin packages provide decoding and encoding for a system libheif build.
The codec development packages are needed when building libheif separately;
RawTherapee itself links libheif through its imported pkg-config target.
`libheif-examples` provides optional inspection/encoding tools.

The Ubuntu 26.04.1 reference host already has those packages and all existing
RawTherapee development dependencies installed. Its available libheif package
is 1.21.2, so `apt upgrade` does not supply the chosen 1.23.6 baseline. Build
that version in an isolated prefix. Do not replace `/usr` libraries.

Optional independent validation tools:

```sh
sudo apt install libimage-exiftool-perl imagemagick ffmpeg python3
```

The harness also uses `liblcms2-dev` and `libglib2.0-dev`, already present for
RawTherapee, plus CMake, Ninja, pkg-config, and a C++17 compiler. Building
libheif itself needs a C++20 compiler. `dependencies.json` records the source
archive hash and the exact codec packages used for the Linux reference run.

## Build and run

From the repository root, with development packages available:

```sh
python3 tests/heif/prepare-libheif.py build/heif-g1/reproduce
export PKG_CONFIG_PATH="$PWD/build/heif-g1/reproduce/builtin-install/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake -S tests/heif -B build/heif-g1/reproduce/harness -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DWITH_HEIF=ON
nice -n 10 cmake --build build/heif-g1/reproduce/harness -j4
nice -n 10 ctest --test-dir build/heif-g1/reproduce/harness --output-on-failure -j4
build/heif-g1/reproduce/harness/heif-feasibility --roundtrip \
  build/heif-g1/reproduce/harness/artifacts \
  > build/heif-g1/reproduce/harness/roundtrip.tsv
python3 tests/heif/verify-readers.py build/heif-g1/reproduce/harness/artifacts
```

`prepare-libheif.py --archive <path>` accepts a previously downloaded archive
and checks the same pinned SHA-256. It never changes system libraries. Its
version record captures the installed codec versions: reproducing the exact
Linux reference requires those versions, not merely any recent codec package.

Keep `PKG_CONFIG_PATH` set for subsequent builds: Ninja may rerun CMake.
An already installed libheif >= 1.23.6 can be used without the preparation
step; record its version, codec names/versions, and packaging mode.

CTest checks unavailable-library `OFF`, `AUTO`, required `ON`, and invalid
selection; codec capability reporting; 19 actual encode/decode cases; and
failure paths for truncated input, context size limits, unwritable output,
and real stdio write failure on a read-only file handle.
The size limit is enforced during decoding if parsing succeeds. The writer
failure check uses a newly encoded context: libheif deliberately rejects
writing a context that was loaded from a file.
The cases cover 8/10/12-bit x 4:2:0/4:2:2/4:4:4 with ICC alone and ICC plus
NCLX, plus a custom wide-gamut ICC without an equivalent NCLX description.
Assertions check coded precision/chroma, odd dimensions, identical ICC bytes,
NCLX fields/absence, absent EXIF/XMP, RGB error, and high-precision gradients.

TSV `case` columns: name, width, height, bit depth, chroma, ICC size, dual
profile flag, mean RGB error, maximum RGB error, neutral ramp distinct levels,
maximum interior RGB error, and worst-error x/y/channel. RGB errors use a
0..1 range. Large sharp-boundary errors in subsampled output are reported;
they are not hidden by the separate 3.5% interior and mean limits. This is
codec/conversion feasibility evidence, not a photographic quality benchmark.

The independent reader script checks container fields and exact ICC bytes
with ExifTool, cropped dimensions with ImageMagick, and precision/chroma,
bitstream color tags, and actual RGB samples with FFmpeg's native HEVC decoder.
FFmpeg has its own decoder, whereas ImageMagick on the Linux reference host
uses the distro libheif. FFprobe and ExifTool report the padded coded extent
(1026 x 258); the clean aperture and decoded dimensions are 1025 x 257. The
script applies that aperture exactly rather than rounding it for subsampling.

## Missing codec checks

Build a second copy with loadable codecs, then hide each independently:

```sh
python3 tests/heif/prepare-libheif.py build/heif-g1/reproduce --plugins
export PKG_CONFIG_PATH="$PWD/build/heif-g1/reproduce/plugins-install/lib/pkgconfig"
cmake -S tests/heif -B build/heif-g1/reproduce/plugins-harness -G Ninja
nice -n 10 cmake --build build/heif-g1/reproduce/plugins-harness -j4
python3 tests/heif/check-codecs.py \
  build/heif-g1/reproduce/plugins-harness/heif-feasibility \
  build/heif-g1/reproduce/plugins-install/lib/libheif \
  build/heif-g1/reproduce/codec-checks
```

With no encoder, `--roundtrip` exits 2; with an encoder and no decoder, it
exits 3. Both messages identify the missing codec. `--expect-codecs` checks
all four availability combinations. This requires the plugin build: builtin
codecs cannot be hidden through `LIBHEIF_PLUGIN_PATH`.

## Color, threads, and input limits

ICC remains authoritative. Known sRGB uses primaries 1, transfer 13, matrix 1
(BT.709), full range. A custom ICC without a correct CICP mapping omits the
optional container NCLX and signals unspecified HEVC primaries/transfer (2),
with explicit BT.709 matrix/full range. Set `x265:colorprim`, `x265:transfer`,
`x265:colormatrix`, and `x265:range` explicitly: libheif's conversion defaults
can otherwise replace unspecified primaries/transfer with sRGB. The independent
script guards against falsely tagging the custom ICC.

ICC-only fixtures use libheif's macOS compatibility option. ICC plus NCLX is
an experiment with Apple reader validation deferred by the user until after
development and commits. Use ICC-only as the current compatibility choice;
successful libheif/FFmpeg reads alone do not establish Apple support.

Each operation disables libheif tile workers, sets decoder codec threads to 1,
selects libde265, and configures x265 with `pools=none`, `frame-threads=1`, and
`wpp=0`. The caller plus a codec worker can still occupy more than one thread;
do not describe these settings as a wholly single-threaded process. Retain the
default libheif security limits and strict decoding. The Ubuntu libde265 1.0.16
plugin lacks the newer >= 1.1 decoder-level security-limit forwarding; libheif's
container/plane limits still apply. Assess a newer libde265 baseline for import
before treating this Linux feasibility build as packaging validation.

## Native Windows and macOS runs

The harness uses C++17 and GLib file APIs; native compilation and execution are
still pending. Use the project's MSYS2 UCRT64 or CLANGARM64 shell on Windows and
Homebrew's development environment on macOS. Make sure pkg-config sees the
matching architecture's libheif, x265, libde265, GLib, and LittleCMS packages.
Use the commands above (Windows binaries have `.exe`; omit `nice` if absent).
`prepare-libheif.py` runs with MSYS2 Python or macOS Python. Keep dependency
DLLs on `PATH` on Windows; retain the prefix's dynamic-library paths on macOS.

Run the complete matrix using both ASCII and Unicode artifact directory names,
and record compiler, architecture, library/codec versions, logs, fixture hashes,
and missing-plugin results. An x265 package without its multilib 10/12-bit APIs
must fail the affected cases rather than silently change their precision.
Windows ARM64 validation remains separate from Windows x86-64.

On macOS, run `swift tests/heif/apple-reader.swift <artifact-directory>` to
exercise ImageIO with every ICC-only and dual-profile fixture and record its
decoded dimensions, component precision, and color space. Retain failures too:
Apple may not support every HEVC range-extension profile. Compare the same
known sRGB and custom ICC fixtures before revisiting NCLX policy. The user
explicitly deferred native Apple and Windows execution until after development
and commits on 2026-10-10. These checks remain pending for G6; G1 is verified
on Linux. G6 also covers full application packaging and workflows.

The Apple script has not been compiled or executed on this Linux host. Its
reader APIs follow Apple's [ImageIO creation options](https://developer.apple.com/documentation/imageio/kcgimagesourceshouldcacheimmediately)
and [ICC extraction API](https://developer.apple.com/documentation/coregraphics/cgcolorspace/copyiccdata()).
