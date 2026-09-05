# X-Trans Student-t GMR corpus and training tool

`rt-tgmr-train` is a standalone C++17 development tool for constructing,
auditing, fitting, and exporting RawTherapee's phase-conditioned X-Trans
Student-t GMR model. It is deliberately independent of `rtengine` and the GUI.
It uses only OpenMP, zlib, JPEG, PNG, TIFF, LittleCMS, and the small cJSON copy
already present in RawTherapee. Python is needed only for the optional
standard-library source downloader; it is not a training or runtime dependency.

The [frozen corpus metadata](corpus-v1/README.md) describes 5,000 selected
photographs and 1,152,000 patches. The data-only production-v1 model is bundled
locally under `rtdata/models/`; original photographs, TGPC files, and fitting
checkpoints remain external. The original 200-image BSDS research corpus was
not used to fit this model. This is an **experimental candidate**, not a model
that passed the original release gates. See the
[productization report](../../devnotes/xtrans-tgmr-productization-report.md)
for the failed quality gates and known limitations. Attribution/redistribution
review must be completed before pushing or distributing the bundled model.

## Build

Ubuntu development packages:

```sh
sudo apt install build-essential cmake ninja-build pkg-config \
    libjpeg-dev libpng-dev libtiff-dev liblcms2-dev zlib1g-dev libomp-dev
```

Standalone GCC build:

```sh
cmake -S tools/tgmr_trainer -B /tmp/rt-tgmr-trainer \
    -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rt-tgmr-trainer -j4
ctest --test-dir /tmp/rt-tgmr-trainer --output-on-failure
```

RawTherapee-tree build:

```sh
cmake -S . -B build/tgmr -DCMAKE_BUILD_TYPE=Release -DBUILD_TGMR_TRAINER=ON
cmake --build build/tgmr -j4 --target rt-tgmr-train
```

The canonical model-publishing backend is selected at run time with
`--backend canonical`. It uses one thread, a fixed reduction order, float64
fitting, and compiler contraction disabled. `--backend cpu` uses deterministic
per-thread sufficient statistics and fixed thread-order reduction. Its fitted
parameters need only satisfy the frozen validation tolerance; it is not the
official byte-identity producer. The `omp-target` name is reserved and currently
requires a separately compiled AMDGCN/NVPTX build. Builds without such a device
image fail closed. The target implementation rejects host fallback, keeps model
and sample arrays mapped while processing bounded batches, and offloads
likelihood, latent-weight, and sufficient-statistic reductions. It remains
experimental until the million-patch speed and image-quality gate passes.

Example AMDGCN configuration (the compiler and ROCm device bitcode must have
compatible LLVM versions):

```sh
cmake -S tools/tgmr_trainer -B /tmp/rt-tgmr-amdgcn -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=/opt/rocm/llvm/bin/clang \
    -DCMAKE_CXX_COMPILER=/opt/rocm/llvm/bin/clang++ \
    -DOpenMP_CXX_FLAGS=-fopenmp -DOpenMP_CXX_LIB_NAMES=omp \
    -DOpenMP_omp_LIBRARY=/opt/rocm/llvm/lib/libomp.so \
    -DTGMR_ENABLE_OMP_TARGET=ON \
    -DTGMR_OMP_TARGET_FLAGS='-fopenmp -fopenmp-targets=amdgcn-amd-amdhsa --offload-arch=gfx1101 --rocm-path=/opt/rocm'
cmake --build /tmp/rt-tgmr-amdgcn -j4
```

NVPTX uses the corresponding compiler-supported target triple and
`--offload-arch=sm_XX`. Local invocations select a device with `--device N` and
may yield between batches using `--gpu-yield-ms N`. GPU fitting remains a
diagnostic, not a required release backend.

## Source manifest and reconstruction

The reviewed source list is a JSON Lines file governed by
[`corpus-source-manifest-v2.schema.json`](corpus-source-manifest-v2.schema.json).
The original [`corpus-v1.schema.json`](corpus-v1.schema.json) remains readable
for development fixtures. Each line is one source and line order is part of the
corpus identity. V2 additionally freezes the catalog snapshot, upstream source
and author identities, rights evidence, archive-member fallbacks, content tags,
people review, selection state, dHash, DCT pHash, and image histograms.

Sparse stellar detail is retained as an explicit demosaicing safety class,
separate from ordinary low-light photography. A Commons source receives the
`astronomy-star-field` tag only when it was discovered through the reviewed
astronomy category and its title also matches the tracked stellar-title rule.
Final selection requires at least 12 training, one validation, and one test
source with this tag. If any held-out split cannot meet that minimum, its
candidate pool must be expanded rather than silently substituting generic
night photography.

The frozen production mix is:

| Catalog | Candidate pool | Train | Validation | Test |
| --- | ---: | ---: | ---: | ---: |
| CVDF Open Images V4/V5 boxable subset | 14,000 | 3,200 | 400 | 400 |
| PASS | 0 | 0 | 0 | 0 |
| Wikimedia Commons | 3,000 | 480 | 60 | 60 |
| Smithsonian Open Access | 2,000 | 320 | 40 | 40 |

Catalog locations and immutable-snapshot procedure are recorded in
[`catalog-acquisition-v1.json`](catalog-acquisition-v1.json); selection policy
and exact quotas are frozen in
[`corpus-selection-v1.json`](corpus-selection-v1.json). Catalog metadata is
the frozen rights evidence. Complete authenticated Open Images, Commons, and
Smithsonian rows using a whitelisted license are approved automatically only
when author, author/profile, landing-page, license, catalog revision, and
catalog digest evidence are complete. Every selected record must have an
approved rights status and may use only CC0, Public Domain Mark, or CC BY
2.0/3.0/4.0.
NC, ND, SA, unknown, ambiguous, and Smithsonian records lacking explicit CC0
are rejected. People records additionally require an explicit non-sensitive,
no-obvious-minors review.

The CVDF Open Images tar mirror contains the 1,743,042-image V4/V5 boxable
training subset, not the complete roughly nine-million-image catalog. Its
candidate metadata comes from the matching
`train-images-boxable-with-rotation.csv` snapshot (638,407,721 bytes, SHA-256
`05f3d68dbbb03728d1a37e51479f4f35c062b871e1a6cae8c4cefbe0e0c80ed0`).
Open Images uses
`OriginalURL` and its advertised checksum as provenance; its changing
thumbnail URL is never canonical. PASS uses the official individual URL and
may name an authenticated Zenodo archive/member as a byte-identical fallback.
PASS is retained as a supported research input, but it has no corpus-v1 quota:
the distributed files have a maximum long side of roughly 500 pixels and fail
the frozen resolution gate. PASS's metadata `hash` is a variable-length hexadecimal source/filename
identity, not an image-content MD5; downloaded originals receive a local
SHA-256 before they become corpus inputs.
Commons freezes a deterministic, bounded category recipe, upload revision,
original URL, API SHA-1, and local SHA-256. The production candidate snapshot
combines subject-tagged Quality Images with explicit CC BY/CC0 license
categories because most Quality Images use the excluded ShareAlike license.
Smithsonian freezes exact anonymous Open Data on AWS index and metadata-shard
bytes, then selects a named high-resolution JPEG rendition. Both the record's
metadata usage and the selected image's media usage must be exactly CC0. No
Smithsonian REST API key or AWS account is required. Records without a stable
landing page are rejected before normalization.
The normalized record also carries the catalog-declared JPEG type so delivery-
service URLs without a filename extension receive a portable `.jpg` cache
name; authenticated files created by the earlier `.img` rule are migrated in
place.

Reconstruct or audit the exact listed originals without third-party Python
modules:

```sh
python3 tools/tgmr_trainer/reconstruct_corpus.py \
    corpus-v1.jsonl /data/tgmr/source-cache --retry 2 --report /tmp/download.json
python3 tools/tgmr_trainer/reconstruct_corpus.py \
    corpus-v1.jsonl /data/tgmr/source-cache --offline-verify
python3 tools/tgmr_trainer/reconstruct_corpus.py \
    corpus-v1.jsonl --emit-fetch-list corpus-v1-fetch.tsv
```

The downloader reads in manifest order, writes `.part` files, retries transient
failures, publishes only bytes having the frozen SHA-256, and never silently
substitutes a changed fallback. V2 also authenticates complete PASS archives
and the selected archive member before publishing it under the source's frozen
identity. Original reconstruction is best effort because third-party URLs can
disappear. The authenticated TGPC patch corpus is the durable training input.

## Corpus workflow

The catalog preparation program uses only the Python standard library. Network
APIs are allowed only while creating a frozen snapshot; canonical rebuilds
consume the snapshot bytes and their pinned SHA-256, never a live API result.

```sh
TOOL=/tmp/rt-tgmr-trainer/rt-tgmr-train
CACHE=/data/tgmr/source-cache
PREP=tools/tgmr_trainer/prepare_corpus.py

# Download and authenticate the production catalog snapshots. The reviewed release
# records their actual SHA-256 values; examples omit them because this tree does
# not contain the external snapshots.
python3 "$PREP" snapshot \
    https://storage.googleapis.com/openimages/2018_04/train/train-images-boxable-with-rotation.csv \
    oi.csv
python3 "$PREP" snapshot \
    https://storage.googleapis.com/openimages/v7/oidv7-class-descriptions-boxable.csv \
    oi-class-descriptions.csv
python3 "$PREP" snapshot \
    https://storage.googleapis.com/openimages/v5/train-annotations-human-imagelabels-boxable.csv \
    oi-human-labels.csv
python3 "$PREP" snapshot \
    https://storage.googleapis.com/openimages/v6/oidv6-train-annotations-bbox.csv \
    oi-boxes.csv

# Commons discovery uses a tracked, bounded category recipe. Its output is the
# complete live-API snapshot consumed by every later offline step.
python3 "$PREP" collect-commons-categories \
    tools/tgmr_trainer/commons-category-recipe-v1.json commons-api.jsonl \
    --report commons-api-report.json

# The collector already records actual file-page categories. For an older
# frozen snapshot created before that field existed, enrich it once and bind
# every later step to the enriched snapshot instead.
python3 "$PREP" enrich-commons-categories commons-api.jsonl \
    commons-api-with-file-categories.jsonl \
    --report commons-category-enrichment-report.json

# Smithsonian is collected from explicitly selected units and hexadecimal
# metadata shards in its public AWS bucket. Both commands freeze their live
# inputs; normalization is offline.
python3 "$PREP" collect-smithsonian smithsonian-aws.jsonl \
    --unit chndm --unit fsg --unit nmah --unit nmnhbirds --unit nmnhbotany \
    --unit nmnhento --unit nmnhminsci --unit nmnhpaleo --unit npm --unit saam \
    --prefix 00 --prefix 01 --prefix 02 --prefix 03 --per-unit-limit 200 \
    --snapshot-dir smithsonian-aws-metadata \
    --report smithsonian-aws-report.json

python3 "$PREP" normalize openimages oi.csv oi-candidates.jsonl \
    --revision cvdf-open-images-v5-boxable-05f3d68dbbb0 \
    --snapshot-sha256 \
    05f3d68dbbb03728d1a37e51479f4f35c062b871e1a6cae8c4cefbe0e0c80ed0 \
    --eligible-only --limit 14000
python3 "$PREP" normalize commons commons-api-with-file-categories.jsonl \
    commons-candidates.jsonl \
    --revision COMMONS_SNAPSHOT_REVISION --snapshot-sha256 SHA256 \
    --tag-rules tools/tgmr_trainer/catalog-content-tags-v1.json --limit 3000
python3 "$PREP" normalize smithsonian smithsonian-aws.jsonl \
    smithsonian-candidates.jsonl --revision SMITHSONIAN_SNAPSHOT_REVISION \
    --snapshot-sha256 SHA256 \
    --tag-rules tools/tgmr_trainer/catalog-content-tags-v1.json --limit 2000

# Download, classify, and assemble each catalog separately. The separate files
# keep the Open Images annotation join isolated from the generic Commons and
# Smithsonian review path. All catalogs may share the authenticated cache.
python3 "$PREP" fetch oi-candidates.jsonl "$CACHE" oi-fetched.jsonl \
    --retry 2 --jobs 4 --report oi-fetch-report.json
"$TOOL" corpus classify oi-fetched.jsonl "$CACHE" oi-classifications.jsonl \
    --candidates --jobs 4 --checkpoint-images 100 --checkpoint-seconds 120
python3 "$PREP" assemble oi-fetched.jsonl oi-classifications.jsonl "$CACHE" \
    oi-assembled-pending-review.jsonl --jobs 4

# Open Images rights are approved from its authenticated catalog row only when
# author, author profile, landing page, accepted CC BY license, catalog revision,
# and catalog digest are complete. Positive human labels and bounding boxes
# create advisory content tags. Machine-generated labels are not used for
# automatic rejection because Open Images documents substantial false positives.
# The generated HTML deliberately leaves people/minor/sensitive-content decisions
# to a human and exports canonical decision JSONL.
python3 "$PREP" prepare-openimages-review oi-assembled-pending-review.jsonl \
    oi-class-descriptions.csv oi-human-labels.csv oi-boxes.csv "$CACHE" \
    oi-auto-reviews.jsonl oi-people-queue.jsonl oi-review-report.json \
    --class-descriptions-sha256 SHA256 --human-labels-sha256 SHA256 \
    --boxes-sha256 SHA256 --rules-sha256 SHA256 \
    --candidate-pool-size 14000 --html oi-people-review.html
python3 "$PREP" apply-openimages-people-decisions oi-auto-reviews.jsonl \
    oi-people-queue.jsonl oi-people-decisions.jsonl oi-reviews.jsonl \
    --require-complete
python3 "$PREP" assemble oi-fetched.jsonl oi-classifications.jsonl "$CACHE" \
    oi-reviewed-candidates.jsonl --reviews oi-reviews.jsonl --jobs 4

python3 "$PREP" fetch commons-candidates.jsonl "$CACHE" commons-fetched.jsonl \
    --retry 2 --jobs 1 --request-delay 1 --report commons-fetch-report.json
python3 "$PREP" fetch smithsonian-candidates.jsonl "$CACHE" smithsonian-fetched.jsonl \
    --retry 2 --jobs 4 --report smithsonian-fetch-report.json
"$TOOL" corpus classify commons-fetched.jsonl "$CACHE" commons-classifications.jsonl \
    --candidates --jobs 4 --checkpoint-images 100 --checkpoint-seconds 120
"$TOOL" corpus classify smithsonian-fetched.jsonl "$CACHE" \
    smithsonian-classifications.jsonl --candidates --jobs 4 \
    --checkpoint-images 100 --checkpoint-seconds 120
python3 "$PREP" assemble commons-fetched.jsonl commons-classifications.jsonl \
    "$CACHE" commons-assembled-pending-review.jsonl --jobs 4
python3 "$PREP" assemble smithsonian-fetched.jsonl smithsonian-classifications.jsonl \
    "$CACHE" smithsonian-assembled-pending-review.jsonl --jobs 4
python3 "$PREP" merge supplemental-assembled-pending-review.jsonl \
    commons-assembled-pending-review.jsonl smithsonian-assembled-pending-review.jsonl
python3 "$PREP" prepare-catalog-people-review \
    supplemental-assembled-pending-review.jsonl "$CACHE" \
    supplemental-auto-reviews.jsonl supplemental-people-queue.jsonl \
    --html supplemental-people-review.html --report supplemental-review-report.json
python3 "$PREP" apply-catalog-people-decisions supplemental-auto-reviews.jsonl \
    supplemental-people-queue.jsonl supplemental-people-decisions.jsonl \
    supplemental-reviews.jsonl --require-complete
python3 "$PREP" assemble commons-fetched.jsonl commons-classifications.jsonl \
    "$CACHE" commons-reviewed-candidates.jsonl --reviews supplemental-reviews.jsonl \
    --jobs 4
python3 "$PREP" assemble smithsonian-fetched.jsonl smithsonian-classifications.jsonl \
    "$CACHE" smithsonian-reviewed-candidates.jsonl \
    --reviews supplemental-reviews.jsonl --jobs 4

python3 "$PREP" merge reviewed-preduplicate.jsonl oi-reviewed-candidates.jsonl \
    commons-reviewed-candidates.jsonl smithsonian-reviewed-candidates.jsonl \
    --cache-prefix openimages-cvdf-v5-boxable=open-images-cvdf-v5 \
    --cache-prefix wikimedia-commons=tgmr-corpus-v1/commons \
    --cache-prefix smithsonian-open-access=tgmr-corpus-v1/smithsonian
python3 "$PREP" prepare-duplicate-review reviewed-preduplicate.jsonl "$CACHE" \
    reviewed-pending-duplicates.jsonl duplicate-review-queue.jsonl \
    --html duplicate-review.html --report duplicate-review-report.json
python3 "$PREP" apply-duplicate-decisions reviewed-pending-duplicates.jsonl \
    duplicate-review-queue.jsonl duplicate-decisions.jsonl \
    reviewed-candidates.jsonl --require-complete
"$TOOL" corpus report reviewed-candidates.jsonl --sources \
    --json candidate-statistics.json --csv candidate-statistics.csv \
    --html candidate-statistics.html

# Selection assigns whole normalized-author groups to one split, enforces exact
# per-catalog quotas and the author cap, rejects exact/perceptual duplicates,
# and balances the 27 training-derived brightness/chroma/texture cells.
"$TOOL" corpus select reviewed-candidates.jsonl \
    tools/tgmr_trainer/corpus-selection-v1.json selected-sources.jsonl \
    > selection-report.json

# Freeze nested, catalog-proportional 250/500/1000/2000/4000 training
# prefixes before patch coordinates are generated. The order manifest binds
# the reviewed selected-source input and reordered output identities.
"$TOOL" corpus order-training selected-sources.jsonl \
    selected-sources-ordered.jsonl training-order.json

# Finalization creates 256 train or 128 validation/test fixed coordinates per
# source, with 75% spatial and 25% coverage samples and frozen augmentations.
"$TOOL" corpus finalize selected-sources-ordered.jsonl "$CACHE" corpus-v1.jsonl \
    --work-dir /var/tmp/tgmr-finalize-work --progress-seconds 15
"$TOOL" corpus validate-manifest corpus-v1.jsonl
"$TOOL" corpus verify-sources corpus-v1.jsonl "$CACHE"
"$TOOL" corpus pack corpus-v1.jsonl "$CACHE" tgmr-corpus-v1.tgpc \
    --work-dir /var/tmp/tgmr-pack-work --checkpoint-records 8192 \
    --progress-seconds 15
"$TOOL" corpus pack corpus-v1.jsonl "$CACHE" \
    tgmr-corpus-v1-identity-training.tgpc \
    --training-augmentation identity-only
"$TOOL" corpus pack corpus-v1.jsonl "$CACHE" \
    tgmr-corpus-v1-sensor-noise.tgpc --noise sensor-v1
"$TOOL" corpus gzip tgmr-corpus-v1.tgpc tgmr-corpus-v1.tgpc.gz
"$TOOL" corpus gzip-file corpus-v1.jsonl corpus-v1.jsonl.gz
"$TOOL" corpus inspect tgmr-corpus-v1.tgpc.gz
"$TOOL" corpus balance tgmr-corpus-v1.tgpc.gz
"$TOOL" corpus report tgmr-corpus-v1.tgpc.gz \
    --json corpus-statistics.json --csv corpus-statistics.csv \
    --html corpus-statistics.html
"$TOOL" corpus release-files corpus-v1.jsonl CORPUS-NOTICE.txt \
    rights-report.json corpus-v1-reconstruction.tsv

python3 "$PREP" release-manifest corpus-v1.jsonl tgmr-corpus-v1.tgpc \
    tgmr-corpus-v1.tgpc.gz CORPUS-NOTICE.txt corpus-statistics.json \
    rights-report.json tgmr-corpus-v1.release.json \
    --source-manifest-gzip corpus-v1.jsonl.gz \
    --statistics-csv corpus-statistics.csv \
    --statistics-html corpus-statistics.html \
    --reconstruction corpus-v1-reconstruction.tsv \
    --checksums-output SHA256SUMS \
    --zenodo-doi DOI --github-release-url URL
```

`corpus inspect` reports SHA-256 values for the encoded train, validation, and
test record streams independently. The validation and test digests must match
across augmentation candidates; augmentation and sensor-noise choices are
training-only experiments.

`prepare-openimages-review` authenticates its V7 boxable class-description,
V5 human-image-label, V6 bounding-box, and tracked content-rule inputs before
joining them to the V5 boxable image population. The class vocabulary is
stable across those annotation snapshots, and the exact cross-version binding
is part of the report. Only positive human labels are accepted; negative human
labels and machine predictions cannot silently reject a candidate. The report
requires a 25% people-review surplus and a 20% quality/author-cap surplus for
each projected split. If a split misses the latter gate, append exactly 2,000
more catalog candidates using `normalize --start N --limit 2000`, classify only
that tranche, merge it, and rerun review preparation.
The Open Images people page is reject-only: every unchecked image is exported
as approved, while a checked image is exported as rejected. This preserves the
manual safety scan without requiring a separate approval click on every image.
Export attempts both a normal browser download and a clipboard copy, and also
shows and selects the complete JSONL in the page as a browser-independent
fallback.

`collect-commons-categories` follows the MediaWiki `categorymembers`
continuation protocol, but every root has explicit category, member, depth, and
file caps. It prefilters dimensions, MIME type, and the accepted license set
before freezing exactly 3,000 metadata records. The tracked recipe currently
discovers 23,850 unique files; its frozen snapshot retains 3,000 after rejecting
3,304 disallowed licenses, 1,567 undersized images, 677 unsupported raster
types, and 14,051 otherwise eligible records above the ten-candidate
acquisition cap per author. The resulting 1,063 authors provide five-image-cap
capacities of 1,814 train, 247 validation, and 180 test candidates. The snapshot
also deliberately samples astronomy and night-photography categories. The
tracked content-tag rules distinguish star-field candidates from the broader
`low-light` population using both category provenance and title evidence.
SHA-256 is
`c03fb6265aae1462b0e37c06627caca4ddc6e25079fa3dd7ff4363d1a71af0b6`;
the normalized candidate JSONL SHA-256 is
`886e3d196e30258a65bfd1b59bd3649b6c15c95e4037c6011e274eb6c8482221`.
Later normalization and source reconstruction never query the live API.

`collect-smithsonian` uses standard-library HTTPS directly against the public
bucket. It streams every chosen shard, records SHA-256 and byte size for the
root index, unit indexes, and shards in the report, and emits only compact
eligible records. `--all-shards` may replace the explicit `--prefix` list for a
complete unit scan. `--snapshot-dir` preserves every upstream byte under stable
relative names. A later run with the same arguments plus `--offline-snapshot`
must reproduce the compact JSONL and report byte-for-byte without network
access. The REST command remains available as
`collect-smithsonian-api` for diagnostics involving an explicit record-ID list;
it is not part of the canonical corpus recipe.

Catalog-derived tags are produced by
[`catalog-content-tags-v1.json`](catalog-content-tags-v1.json). Any Commons or
Smithsonian candidate tagged `people` remains ineligible until a human decision
is recorded. After assembly, `prepare-catalog-people-review` creates a local
contact sheet and canonical queue; `apply-catalog-people-decisions
--require-complete` applies the exported decisions. This is separate from the
annotation-rich Open Images queue and uses the same no-obvious-minors or
sensitive-content policy. Commons tagging uses the authenticated categories of
each file page, not merely the discovery category, plus conservative whole-word
title terms. These signals only create a review obligation; they never approve
or reject an image automatically.

The generic people-review page is reject-only. Inspect every image, mark only
the rejected cards, then affirm the page-level completion checkbox. Export then
writes an explicit approved or rejected decision for every queued source. The
page mirrors selections into HTML attributes so saving the complete page after
review also preserves the decisions as a recovery copy.

`fetch --jobs N` downloads distinct candidates concurrently but preserves input
order in its canonical output. `--request-delay SECONDS` enforces a single
process-wide minimum interval between request starts, including across workers.
Every worker writes a unique `.part` file, authenticates the catalog checksum
when available, and publishes atomically.
Completed cache files therefore act as per-image restart checkpoints even when
the final fetch JSONL has not yet been written. Cache filenames contain only
portable filename characters plus a truncated identity digest. A rerun adopts
and renames authenticated files written by the earlier colon-containing cache
scheme instead of downloading them again. HTTP 429 and transient server errors
honor `Retry-After` (up to 15 minutes) before retrying and pause all workers.
The Commons recipe uses one worker and a one-second request interval, following
Wikimedia's serial-request guidance; keep that conservative policy for the
canonical acquisition run.

`prepare-duplicate-review` independently repeats the frozen five-image author
cap and identifies the narrow band just outside automatic rejection: dHash
distance 6-7 or DCT pHash distance 9-10. It groups connected pairs into a
side-by-side cluster contact sheet and marks every involved candidate
`candidate-pending-duplicate-review`. Reviewers affirm each complete cluster
and mark any redundant sources for rejection. Normal reviews use
`apply-duplicate-decisions --require-complete`; rejected and unresolved
candidates cannot enter final selection. A deliberately conservative partial
review can instead use `--reject-unreviewed-clusters`, which rejects every
member of every cluster omitted from the decisions file while retaining the
recorded decisions for reviewed clusters. The report separates explicit
rejections from this conservative policy. Exact identities, shared Flickr IDs,
dHash distance at most 5, and DCT pHash distance at most 8 remain automatic
hard exclusions rechecked by the C++ selector.

When catalog caches live under different directories, `merge --cache-prefix`
rebases their safe relative `cache_filename` fields beneath one common cache
root. It changes no source identity or digest and refuses unmatched catalog
names, absolute paths, and parent traversal.

The stdout from each `snapshot`, `collect-*`, `normalize`, `fetch`, `select`,
`balance`, and `release-manifest` command is canonical JSON and should be saved
with the release audit. A PASS archive-index JSONL line contains `hash`, `url`,
`sha256`, `member`, and `member_sha256`; extraction rejects absolute paths,
parent traversal, changed archives, and changed members.

Human review input is canonical JSONL. A typical line is:

```json
{"content_tags":["people","skin-hair-clothing"],"format":"rawtherapee-tgmr-source-review-v1","normalized_author_id":"flickr-user:stable-identity","people_review_status":"approved-no-minors-or-sensitive-content","rights_evidence_revision":"reviewed-upload-revision","rights_evidence_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","rights_evidence_url":"https://example.invalid/source-rights-page","rights_review_status":"approved","source_id":"openimages-cvdf-v5-boxable:source-id"}
```

`normalized_author_id` is optional, but it is required when catalog metadata
uses different identifiers for the same author. The preparation tool already
normalizes recognizable Flickr profile identities shared by Open Images and
PASS; manual review resolves remaining cross-catalog aliases.

The packer decodes and color-manages into linear sRGB, then applies frozen
exposure, white-balance, and train/evaluation-separated Fujifilm camera-matrix
augmentations. It emits 7x7 planar RGB patches as little-endian uint16. Packing
streams one source and one record at a time. The `.tgpc.gz` member is ordinary
gzip made through RawTherapee's existing required zlib dependency, with zero
timestamp, no filename/comment, fixed compression level, and a canonical OS
byte. Compressed input is streamed directly by the trainer.

`corpus pack` defaults to production-v1 training augmentation and the frozen
`none` noise recipe: transformed values are bounded and quantized, but no
synthetic noise is added. `--training-augmentation identity-only` removes
exposure, white-balance, and camera-matrix augmentation from training records
only. `--noise sensor-v1` adds deterministic bounded integer-domain read and
signal-dependent noise to augmented training records only. Its nominal
standard deviations are approximately 8 uint16 codes for read noise and 64
codes at saturation for the signal term. The TGPC configuration digest and
per-record augmentation kind distinguish the recipes. Validation and test
records are byte-identical across all three candidates, so model comparisons
use exactly the same held-out inputs. Identity-only plus sensor noise is
rejected as a contradictory request. These are validation candidates, not a
claim about a particular camera's calibrated noise model.

`corpus finalize` and `corpus pack` are restartable. Finalization writes one
authenticated immutable record file per completed source under its work
directory. Packing keeps an authenticated partial TGPC beside the destination
and a small durable checkpoint in its work directory; a restart re-hashes and
decodes every checkpointed record before appending. Completed source-sized
ranges are skipped without reopening their images. Work state is bound to the
exact input manifest, configuration, and expected record count, so it cannot
be reused for a different corpus or augmentation recipe. Both commands update
canonical `progress.json` files and print an ETA at the selected interval.
`--force` starts either operation from scratch; without it, rerunning the same command
resumes safely and still produces the same bytes as an uninterrupted run.

`corpus release-files` derives three deterministic reviewed-source artifacts
from the finalized production manifest: the human-readable attribution notice,
the canonical JSON rights/evidence report, and a TSV containing every original,
fallback, and archive-member reconstruction route with its required digest.
The command validates the complete 5,000-source production contract first and
refuses to replace any output unless `--force` is explicit.

`corpus gzip-file` applies the same zero-timestamp, filename-free canonical
gzip encoding to non-TGPC release inputs such as the expanded source JSONL. It
reports both compressed and uncompressed sizes and SHA-256 identities; unlike
`corpus gzip`, it does not try to parse the result as a patch corpus.

`corpus classify-file` remains a useful intake/debugging form for one image.
`corpus classify --candidates` is the canonical batch path: it authenticates
the Python fetch output, decodes every available candidate, and produces the
classification input for assembly. `--all` classifies selected and unselected
records from an already assembled V2 manifest. Batch classification uses a
bounded C++ worker pool (`--jobs 4` by default). Each JPEG is read once: the
exact compressed bytes are SHA-256 authenticated and then decoded from memory.
Nested cache names are supported for sharded stores, but absolute paths,
`.`/`..` components, and symlink escapes outside the supplied cache are
rejected.

Long runs are restartable without changing their final bytes. By default the
tool writes immutable, authenticated result segments under
`OUTPUT.jsonl.work`, checkpoints every 128 completed images or 120 seconds,
and reports progress every 15 seconds. Put this work directory on a local
filesystem when the source cache is a network share:

```sh
rt-tgmr-train corpus classify fetched.jsonl /data/open-images \
    classifications.jsonl --candidates --jobs 4 \
    --work-dir /var/tmp/tgmr-classify-work \
    --checkpoint-images 128 --checkpoint-seconds 120 \
    --progress-seconds 15 --retries 2
```

Rerunning the same command validates and resumes its segments. Incomplete
`.tmp` files are ignored. A changed input list or full/proxy mode is rejected
instead of being combined with old state. Final output is merged in original
input order and is byte-identical across thread counts and interruptions.
Unresolved per-image failures are recorded in `failures.json`; successful
images remain checkpointed for the next run. Candidate-pool screening may use
the explicit `--allow-failures` option after reviewing that file: the final
classification JSONL then contains only successful records, while the failure
report remains beside the checkpoints as the rejection audit. The default
continues to fail closed, and reviewed/final corpus verification must not use
this option to hide missing selected sources.

For metadata shortlisting, `--proxy` asks libjpeg for a 1/8-resolution decode.
Its rows use the distinct
`rawtherapee-tgmr-image-proxy-classification-v1` identity and record both source
and proxy dimensions. Proxy results are approximate, JPEG-only, and must not
be passed to `assemble`; candidates selected from them require the normal full
classification pass. RGB and grayscale embedded ICC profiles are both honored;
grayscale JPEGs are expanded to neutral linear RGB through their one-component
profile rather than being incorrectly presented to LittleCMS as RGB-profile
input.

Bulk CVDF Open Images archives do not need a separate preliminary per-file
SHA-256 pass; classification still authenticates every selected local object.
The tar files contain only the V4/V5 boxable subset and therefore must be paired
with the matching boxable image-information snapshot. Content tagging separately
uses the authenticated human-label and box snapshots named above; it never
changes the underlying image identity. After metadata filtering has produced
canonical catalog-candidate JSONL, point the classifier at the extracted split root:

```sh
rt-tgmr-train corpus classify oi-shortlist.jsonl /data/open-images \
    oi-proxy.jsonl --open-images-cvdf train --proxy --jobs 4 \
    --work-dir /var/tmp/tgmr-oi-proxy
```

For an image ID `abcdef0123456789`, this mode requires
`train/a/b/c/abcdef0123456789.jpg`. It authenticates and decodes the same byte
buffer and records `source_sha256` in the classification row. Run the retained
shortlist again without `--proxy` for final metadata. `prepare_corpus.py
assemble` accepts the original catalog-candidate JSONL for this local mode,
requires the full classification's source digest, and reauthenticates the file
before creating the V2 source manifest. Because CVDF images are rescaled mirror
renditions rather than the Flickr originals, assembly records the matching
`https://open-images-dataset.s3.amazonaws.com/SPLIT/ID.jpg` object as the
reconstruction URL and does not apply the original-image MD5 to those bytes.

The intended large-catalog sequence is metadata/license/author filtering
first, proxy classification of a roughly 50,000-image shortlist second, and
full classification of only the roughly 10,000--16,000 candidates retained for
deduplication and balanced source selection. The tool can process a larger
list, but scanning every Open Images file is neither required nor the frozen
corpus recipe.

`corpus balance` reports the declared low/middle/high brightness, chroma, and
texture strata using cut points derived only from training patches. The old
fixed research thresholds remain available as `--fixed-v1-thresholds`.
`corpus report --sources` emits source/catalog/license/rights/tag and histogram
statistics before packing; ordinary `corpus report` emits canonical TGPC JSON,
CSV, and self-contained HTML. Final release files are intended for publication
outside Git as identical `.tgpc.gz` bytes on Zenodo and a RawTherapee/GitHub
release. Those locations have not been reserved or published. The Git tree
retains hashes, compact source JSONL, the URL/checksum list, statistics, and
attribution; permanent URLs will be added after publication and verification.

Selection deliberately fails if the reviewed pool cannot satisfy a catalog,
split, people, author, quality, or deduplication constraint. The remedy is to
expand the corresponding candidate source, never to relax licensing or reuse
duplicates. Catalog rights metadata is the frozen rights decision for Open
Images; individual landing pages are not manually re-reviewed. Manual inspection
remains required for borderline near-duplicate clusters, controlled people
content, and ambiguous content cases. The program cannot turn a syntactically
valid manifest into a legal conclusion.

## Fitting, checkpoints, export, and benchmark

```sh
"$TOOL" train tgmr-corpus-v1.tgpc.gz /data/tgmr/checkpoints \
    --backend canonical --components 32 --gaussian-iterations 10 \
    --student-iterations 30 --batch-size 4096 --maximum-memory-mib 8192

"$TOOL" resume tgmr-corpus-v1.tgpc.gz \
    /data/tgmr/checkpoints/phase-00-final.tgmrc \
    /data/tgmr/checkpoints/phase-00-resumed.tgmrc \
    --additional-student 1 --backend canonical

"$TOOL" benchmark tgmr-corpus-v1.tgpc.gz \
    --backend cpu --samples 102400 --phase 0 --components 32 \
    --gaussian-iterations 10 --student-iterations 30 --batch-size 4096
```

The frozen 250/500/1000/2000/4000-source convergence curve is generated from
the same TGPC by passing `--source-limit N` to `train`. Before finalization,
`corpus order-training` makes those prefixes nested and fixes their cumulative
Open Images/Commons/Smithsonian counts to 200/30/20, 400/60/40, 800/120/80,
1600/240/160, and 3200/480/320. Within each catalog it interleaves the 27
training-derived brightness/chroma/texture cells deterministically. The limit
and actual sample count are stored in every checkpoint and companion manifest;
`resume` rejects a different limit. This avoids repacking subtly different
curve corpora while retaining one corpus payload identity.

For a target build, replace `--backend cpu` with `--backend omp-target` and add
`--device N`. A physical-device smoke test is not the release performance gate:
the backend is retained only if the production run is at least 1.5x faster than
optimized CPU and its exported model remains within 0.02 dB on validation.

Training uses 18 phase-conditioned 51-dimensional observations, deterministic
k-means++, full-covariance Gaussian EM initialization, covariance floor 1e-6,
and fixed-nu Student-t ECM with nu=3. Checkpoints are authenticated and atomic.
The final export specializes each phase/component into the fixed S9/top-8,
tau=0.0003, temperature-4 inference contract and emits the TGMR v2 model plus a
canonical companion manifest:

```sh
"$TOOL" training-identity --backend cpu --components 32 \
    --gaussian-iterations 10 --student-iterations 30 \
    --covariance-floor 1e-6 --degrees-of-freedom 3 \
    --batch-size 4096 --maximum-memory-mib 8192
"$TOOL" export --checkpoints /data/tgmr/checkpoints model.tgmr \
    --corpus-sha256 HEX --attribution-sha256 HEX
"$TOOL" verify model.tgmr
"$TOOL" validate model.tgmr tgmr-corpus-v1.tgpc.gz \
    --split validation > model.validation.json
# Re-exporting the deterministic model bytes attaches the authenticated
# validation result to the final companion manifest.
"$TOOL" export --checkpoints /data/tgmr/checkpoints model.tgmr \
    --corpus-sha256 HEX --attribution-sha256 HEX \
    --validation-report model.validation.json --force
```

Checkpoint export derives the canonical configuration digest from the
authenticated phase checkpoints and the trainer-revision digest from a sorted
manifest of the trainer sources, headers, CMake input, and embedded cJSON
implementation. Optional explicit digest arguments are accepted only as
assertions and are rejected when they differ from those derived identities.

`validate` applies the exact scalar K32/S9/q8 inference equations to every
selected patch through all 18 X-Trans phases, restores the measured center
sample, and reports pooled/phase/per-source PSNR, phase spread, fixed corpus-v1
brightness/chroma/texture error strata, plus p99 and worst patch RMS. Its
canonical JSON deliberately excludes elapsed time so two
clean canonical runs can be compared byte-for-byte. This is the native
measurement used for source-count curves, augmentation selection,
canonical/accelerated parity, and the frozen validation/test record.

When built with both `BUILD_TESTING` and `BUILD_TGMR_TRAINER`, the non-installed
`rawtherapee-xtrans-tgmr-evaluate` target compares a TGMR v2 model with the real
three-pass Markesteijn implementation at frozen validation or test coordinates.
It rebuilds sufficiently padded camera-linear crops from the reviewed source
images, verifies one deterministic crop center against a full-image
Markesteijn run, assigns all 18 phases evenly, and reports pooled, tail,
per-source, and fixed brightness/chroma/texture results for both methods:

```sh
rawtherapee-xtrans-tgmr-evaluate model.tgmr tgmr-corpus-v1.tgpc.gz \
    corpus-v1.jsonl "$CACHE" --split test > model.test-comparison.json
```

The release model must be produced twice in clean pinned canonical environments
with byte-identical corpus, checkpoint/export inputs, model, and manifest. An
optimized CPU or future OpenMP-target fit is acceptable only after the specified
PSNR/image-quality parity tests; it can never publish the official artifact.
Release CMake configuration rejects a companion manifest whose validation
object is absent or whose model/corpus identities do not match.
The canonical validation report contains only deterministic quality fields so
two clean canonical fits can require byte-identical results. Wall-clock time is
measured separately by the benchmark and release orchestration rather than
being included in an authenticated quality report.

## RawTherapee integration

RawTherapee exposes `Student-t GMR (experimental)` for X-Trans sensors, while
Markesteijn three-pass remains the default. A reviewed build installs
`models/xtrans-tgmr-v2.tgmr` and compiles its exact SHA-256 into the loader.
The in-tree production-v1 artifact is installed automatically. Developers can
select an authenticated compatible custom model explicitly:

```sh
RT_XTRANS_TGMR_MODEL=/absolute/path/model.tgmr rawtherapee-cli ...
```

The explicit environment variable is an override, not a search path. Loading,
CFA, allocation, or inference failure emits a diagnostic and causes complete
Markesteijn overwrite. No partial TGMR output is exposed.

`cmake/TgmrModel.cmake` authenticates the exact in-tree model, canonical
companion manifest, and complete attribution notice before installation.
Their trusted digests are reviewed source constants, not user-supplied cache
values. Configure with `-DTGMR_OFFICIAL_MODEL=` to build without installing
weights; selecting TGMR without a valid override then falls back loudly.
`BUILD_TGMR_TRAINER=OFF` is the default and excludes trainer-only targets.

Only the specifically reviewed in-tree model and its metadata are tracked.
Other models, the patch corpus, checkpoints, and originals remain external.
Local commits containing this candidate are authorized; pushing or distributing
them remains blocked on the separate attribution/redistribution review.

Very sharp sparse borders, one-pixel structures, and some isolated chromatic
highlights can produce false colour or ringing. Runtime fallback cannot detect
these quality failures. Markesteijn remains the default, and no hard-case
renderer, synthetic replacement generator, or screening command is included.
