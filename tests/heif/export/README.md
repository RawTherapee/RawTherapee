# Production engine HEIF export checks

G2 introduced `ImageIO::saveHEIF` and `IImage::saveAsHEIF` for Image8, Image16, and
Imagefloat. Defaults are quality 92, 10-bit, and 4:2:0; explicit bit depths
are 8/10/12 and subsampling is 1=420, 2=422, 3=444. Quality 100 is maximum
lossy quality. The rendered output ICC is copied exactly. G3 added GUI/CLI
and batch workflows; G4 embeds selected EXIF/XMP with IPTC-to-XMP conversion
(see [metadata checks](../metadata/README.md)). This pixel harness supplies no
metadata source, so its fixtures deliberately remain EXIF/XMP-free. Input
loading remains G5.

Float scanlines are scaled and clamped directly, including nonfinite values;
integer images use their 16-bit scanlines. Higher precision never passes
through an 8-bit intermediate. Conversion and HEVC VUI use a BT.709 matrix
and full range; primaries/transfer remain unspecified because an arbitrary
output ICC cannot safely be guessed as sRGB. Container color description uses ICC only.
An empty output profile leaves the image untagged rather than inventing one.

The writer uses an exclusive temporary file beside the destination and
publishes with GLib rename only after successful encoding, writing, and
closing. It cleans partial output and preserves existing output on failure.
It respects the existing save API's overwrite semantics; caller overwrite
confirmation is handled by the G3 workflows. See
[GLib rename](https://docs.gtk.org/glib/func.rename.html) for platform behavior.

## Build and test

From the repository root, first prepare the pinned libheif prefix as described
in the parent README and install the ordinary RawTherapee build dependencies:

```sh
export PKG_CONFIG_PATH="$PWD/build/heif-g1/reproduce/builtin-install/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake -S . -B build/heif-g2/rt-on -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DWITH_HEIF=ON -DWITH_HEIF_EXPORT_TESTS=ON
nice -n 10 cmake --build build/heif-g2/rt-on --target heif-export-checks rth-cli -j4
nice -n 10 ctest --test-dir build/heif-g2/rt-on -j4 --output-on-failure
nice -n 10 python3 tests/heif/export/verify-export.py \
  build/heif-g2/rt-on/tests/heif/export/artifacts
```

`WITH_HEIF_EXPORT_TESTS` defaults OFF, so ordinary builds add no test target.
The test executable links the actual production engine and all three concrete
image classes. The corresponding `WITH_HEIF=OFF` build runs the same test
entry point and verifies that each image API returns the unsupported-format
error without changing an existing destination.

Enabled checks include 27 image type/precision/chroma combinations, exact
sRGB/custom ICC, no profile, ignored EXIF, odd dimensions, a .heif Unicode
path, a 255-character output basename, 1x1 output, float clamping, four quality settings on deterministic noise,
invalid arguments, empty images, missing parent directories, failed final
rename, overwrite, and four concurrent exporters with repeated library
initialization. On Unix, a real `RLIMIT_FSIZE` write failure checks both the
return code and destination preservation. Windows lacks this Unix test;
filesystem failure coverage there remains pending in G6.

The independent script verifies 56 generated files using ExifTool, ImageMagick,
and FFmpeg's native HEVC decoder. It checks coded bit depth/chroma, dimensions,
exact ICC, absence of EXIF/XMP and stale orientation, matrix/range and
unspecified primaries/transfer. Mean and interior RGB errors must be below
0.035 on chart fixtures; high-bit Image16/Imagefloat ramps must retain more
than 256 levels. Noise fixtures test quality/encoded-size behavior, not that
chart error bound. These are synthetic integration checks, not a photographic
quality benchmark. Reports and generated images stay in ignored build paths.

## Encoder and decoder availability

On Linux, substitute the G1 plugin-enabled libheif of the same pinned version:

```sh
nice -n 10 python3 tests/heif/export/check-export-codecs.py \
  build/heif-g2/rt-on/tests/heif/export/heif-export-checks \
  build/heif-g1/deps/plugins-install build/heif-g2/export-codecs
```

This checks all four encoder/decoder plugin combinations. An absent x265
encoder must return the availability error, report a useful diagnostic, and
preserve an existing file. An encoder alone must export successfully; export
does not require a decoder. The script changes only subprocess environment
and isolated test directories, with results in `export-codecs.json`.

Native Windows/macOS and Windows cross-compilation/Flatpak Wine execution
are explicitly pending in G6; Linux passing results do not establish them.
