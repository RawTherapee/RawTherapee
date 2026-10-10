# HEIF GUI, queue, settings and CLI checks

G3 adds HEIF 8/10/12-bit presets to Save As and the batch queue. Quality
(default 92) and chroma (default 4:2:0) are independent of JPEG settings.
Presets require the x265 encoder at runtime. An unavailable saved HEIF
selection stays visible with an error; it is never mapped to another format.
`.heic` is the default extension and `.heif` is an export alias, ignoring case.

The CLI selects HEIF with `-H[0-100]`, chroma with `-Hs<1|2|3>` and integer
bit depth with `-b<8|10|12>` (default 10). Quality 100 remains lossy. Validate
bit depth after parsing; both `-b12 -H` and `-H -b12` work. Existing JPEG,
PNG and TIFF depths remain supported. HEIF embeds ICC, EXIF and XMP; supported IPTC fields are mapped to XMP. GUI/batch processing profiles use `.out.pp3`; CLI `-O` retains the
established output `.pp3` convention.

New `Output` keys are `HeifQuality`, `HeifBps`, and `HeifSubSamp`, plus their
`Batch` counterparts. Queue rows append quality, depth and chroma after
`bigTiff`. Old rows retain all existing column meanings and get HEIF defaults
92/10/1, including when current preferences differ.

These integration scripts currently exercise Linux. The GUI test uses the
actual production GUI objects, serializer, recovery loader and queue worker.
It replaces only the application's `main` while retaining the built target's
compiler flags, libraries and RPATH. It requires a completed Ninja build and
a usable GTK display; in headless CI run through `xvfb-run -a`. Run without
concurrent application access to the test settings directories. All files
and generated artifacts go in ignored build directories, separate from user
settings. Compiler invocations use `nice -n 10`; build with four jobs:

```sh
export PKG_CONFIG_PATH="$PWD/build/heif-g1/deps/install/lib/pkgconfig"
nice -n 10 cmake --build build/heif-g1/rt-on \
    --target rth rth-cli heif-export-checks -j4
nice -n 10 cmake --build build/heif-g1/rt-off \
    --target rth rth-cli heif-export-checks -j4
python3 tests/heif/workflows/check-gui.py \
    build/heif-g1/rt-on build/heif-g1/g3-gui-on
python3 tests/heif/workflows/check-gui.py \
    build/heif-g1/rt-off build/heif-g1/g3-gui-off
python3 tests/heif/workflows/check-cli.py \
    build/heif-g1/rt-on/rtgui/rawtherapee-cli \
    build/heif-g1/legacy/input.png build/heif-g1/g3-cli-on
python3 tests/heif/workflows/check-cli.py \
    build/heif-g1/rt-off/rtgui/rawtherapee-cli \
    build/heif-g1/legacy/input.png build/heif-g1/g3-cli-off --unavailable
```

CLI input must be an existing PNG; the G2 regression fixture above is one
example. Tests explicitly select the checked-in sRGB profile, then compare
embedded ICC bytes exactly with ExifTool. FFprobe checks actual coded depth,
chroma and range; FFmpeg checks identical decoded output with either argument
order. Both scripts require ExifTool and FFmpeg. CLI tests cover quality 0/100,
filename aliases, processing-profile sidecars, invalid options, unavailable
support, and JPEG/PNG/TIFF (integer/float) exports.

GUI tests cover all seven legacy preset meanings, all HEIF presets, saved
settings round trips and defaults, old/short/malformed queue fields, actual
Save As acceptance or an availability dialog, generic extension dispatch,
recovered forced/override/automatic queue exports, sidecars and overwrite
preservation. Repeat with a plugin build containing only libde265 to verify
that missing x265 is handled separately from a disabled build:

```sh
export LD_LIBRARY_PATH="$PWD/build/heif-g1/deps/plugins-install/lib"
export LIBHEIF_PLUGIN_PATH="$PWD/build/heif-g1/g2-export-codecs/plugins-0-1"
python3 tests/heif/workflows/check-gui.py \
    build/heif-g1/rt-on build/heif-g1/g3-gui-no-encoder
python3 tests/heif/workflows/check-cli.py \
    build/heif-g1/rt-on/rtgui/rawtherapee-cli \
    build/heif-g1/legacy/input.png build/heif-g1/g3-cli-no-encoder --unavailable
```

The last directory is prepared by the G2 codec tests. An encoder-only plugin
build should show presets and export successfully without libde265; use
`plugins-1-0` for that check. HEIF input is still deferred to G5. Native
Windows/macOS and Windows cross-compilation/Wine checks remain G6.
