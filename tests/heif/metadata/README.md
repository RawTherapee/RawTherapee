# HEIF export metadata checks (G4)

JPEG and HEIF share `Exiv2Metadata::prepareExportMetadata()`: EXIF selection,
user edits, source/sidecar merging, thumbnail and source-layout filtering,
RawTherapee software identification and export timestamp. HEIF serializes
EXIF with Exiv2 and embeds EXIF/XMP through libheif before publishing output.
It never invokes Exiv2's HEIF file writer or requires an unreleased writer.

The rendered primary image has normal container orientation. If selected
EXIF or source XMP retains orientation/dimension properties, HEIF updates
those properties to normal orientation and the rendered width/height. Other
metadata preparation follows JPEG. This format-specific normalization does
not change JPEG's existing metadata behavior.

Metadata mode Strip omits EXIF/XMP while retaining the output ICC. Valid input
images without EXIF can still produce HEIF with the software/export timestamp.
Missing/unreadable metadata sources, malformed enabled sidecars, failed EXIF
or XMP serialization, and libheif attachment errors fail the export before
publication. An existing destination survives and the progress listener gets
a diagnostic. JPEG retains its previous error/retry behavior.

HEIF has no standard raw IPTC IIM item. Exiv2 maps supported IIM fields into
XMP (including keywords, caption, creator, copyright and location). Explicit
source/sidecar XMP takes priority over the initial IIM conversion, matching
RawTherapee's established XMP merge policy. Edited IPTC fields then override
the corresponding XMP properties; editor text is explicitly UTF-8. Arrays
retain repeated keywords and multiple creators. Sidecar fields converted into
EXIF/IIM do not necessarily replace an already explicit embedded XMP value:
for example a sidecar caption can become EXIF ImageDescription while an
explicit embedded XMP description remains. This matches current JPEG exports.

IIM datasets without an Exiv2 mapping are not embedded: envelope records,
custom datasets, and unsupported fields such as DateCreated/TimeCreated and
DigitizationDate/DigitizationTime. The `0x00fe` fixture demonstrates that no
private IPTC item is emitted. JPEG's raw IIM remains available in JPEG output.
This is not a promise of arbitrary IIM preservation.

`ExifParser::encode()` is intrusive and lossy, using the same JPEG-oriented
serializer as a newly rendered JPEG. Its EXIF payload ceiling is 65,527 bytes.
It can remove unsupported IFDs, previews and oversized tags before returning,
including an oversized UserComment in these checks. HEIF intentionally keeps
that filtering behavior; it does not preserve arbitrary opaque MakerNotes.
Recognized Canon MakerNote fields round-trip and match JPEG here. A buffer
still exceeding the ceiling fails visibly. XMP has no JPEG APP1 ceiling in
HEIF: the 100,000-character fixture is retained; libheif's signed size argument
is checked. Real camera samples and native reader checks remain in G6.

Build and run the opt-in production-engine checks on Linux (both caches have
`WITH_HEIF_EXPORT_TESTS=ON`):

```sh
export PKG_CONFIG_PATH="$PWD/build/heif-g1/deps/install/lib/pkgconfig"
nice -n 10 cmake --build build/heif-g1/rt-on \
    --target rth rth-cli heif-export-checks heif-metadata-checks -j4
nice -n 10 ctest --test-dir build/heif-g1/rt-on --output-on-failure -j4
nice -n 10 cmake --build build/heif-g1/rt-off \
    --target rth rth-cli heif-export-checks heif-metadata-checks -j4
nice -n 10 ctest --test-dir build/heif-g1/rt-off --output-on-failure -j4
nice -n 10 python3 tests/heif/metadata/check-metadata.py \
    build/heif-g1/rt-on/rtgui/rawtherapee-cli \
    build/heif-g1/rt-on/tests/heif/export/metadata-artifacts \
    build/heif-g1/g4-metadata-readers
```

Optionally pass `--jpeg-baseline <pre-G4 rawtherapee-cli>` to compare all JPEG
metadata except the generated timestamp/software version and exact decoded
JPEG pixels in five production CLI scenarios. The recorded run used a copy
of the G3 executable preserved before rebuilding. The script also checks
seven HEIF exports with independent ExifTool/FFmpeg, EXIF/JPEG parity apart
from rendered geometry, UTF-8, arrays, sidecars, metadata Strip, ICC and CLI
processing-profile sidecars. The C++ test independently extracts standard
libheif items, honors the EXIF TIFF offset, and parses the metadata buffers;
it adds read/size/XMP failures, destination preservation, metadata-free source
cache compatibility, and four concurrent exports without edit cross-contamination.

G3 workflow scripts remain usable and now expect export software metadata in
HEIF. `reference-linux.json` records the Linux run; ignored build directories
hold images, reader reports, isolated preferences and logs. Windows/macOS
native verification and cross-compilation/Flatpak Wine remain deferred to G6.
