# Frozen TGMR corpus-v1 provenance

This directory contains the compact, already ordered 5,000-source selection,
its training-order identity, catalog identities, rights evidence, reconstruction
list, and patch statistics. It does not contain photographs or TGPC patches.
The metadata preserves the recorded human decisions without repeating selection.

| Catalog | Train | Validation | Test |
| --- | ---: | ---: | ---: |
| Open Images CVDF V4/V5 boxable mirror | 3,200 | 400 | 400 |
| Wikimedia Commons | 480 | 60 | 60 |
| Smithsonian CC0 | 320 | 40 | 40 |

Each training photograph supplies 256 patches; each validation/test photograph
supplies 128. There are 1,024,000 / 64,000 / 64,000 records respectively.
The selected recipe is `production-v1` augmentation with `none` noise.
The first 250/500/1,000/2,000/4,000 training sources are nested, proportionally
catalog-balanced prefixes. Do not reorder this manifest or run selection again.
Fourteen astronomy sources preserve the 12/1/1 training/validation/test guardrail.

`release-metadata.json` pins every retained artifact and the external expanded
manifest and TGPC identities. `rights-report.json` is the original canonical
source-rights evidence report. Per-source catalog license approval is not a
completed legal determination about model redistribution. The complete
attribution notice accompanies the model in `rtdata/models/`.

## Reconstruct the sources and patches

Run from the repository root, using an empty or identity-bound work directory:

```sh
CORPUS=tools/tgmr_trainer/corpus-v1
TOOL=/tmp/rt-tgmr-trainer/rt-tgmr-train
CACHE=/data/tgmr/source-cache
WORK=/data/tgmr/rebuild-v1
mkdir -p "$WORK"
python3 tools/tgmr_trainer/reconstruct_corpus.py \
    "$CORPUS/selected-sources.jsonl" "$CACHE" --retry 2 \
    --report "$WORK/reconstruction.json"
python3 tools/tgmr_trainer/reconstruct_corpus.py \
    "$CORPUS/selected-sources.jsonl" "$CACHE" --offline-verify
"$TOOL" corpus finalize "$CORPUS/selected-sources.jsonl" "$CACHE" \
    "$WORK/corpus-v1.jsonl" --work-dir "$WORK/finalize"
"$TOOL" corpus pack "$WORK/corpus-v1.jsonl" "$CACHE" \
    "$WORK/tgmr-corpus-v1.tgpc" --training-augmentation production-v1 \
    --noise none --work-dir "$WORK/pack"
"$TOOL" corpus gzip "$WORK/tgmr-corpus-v1.tgpc" \
    "$WORK/tgmr-corpus-v1.tgpc.gz" --level 9
"$TOOL" corpus inspect "$WORK/tgmr-corpus-v1.tgpc.gz"
"$TOOL" corpus balance "$WORK/tgmr-corpus-v1.tgpc.gz"
```

Compare every generated digest with `release-metadata.json`. Reconstruction
authenticates the exact bytes; unavailable or changed third-party URLs must not
be silently replaced. The selected source list includes both original provenance
and matching mirror routes. Its CVDF URLs refer to the authenticated rescaled
mirror bytes, not the independently advertised Flickr-original checksums.
Use the standard-library downloader for complete fallback/archive handling;
`reconstruction.tsv` is also available for manual download tools.

Decode/color-management and compression library versions are relevant to clean
regeneration. The original run used Ubuntu 24.04, JPEG 2.1.5, PNG 1.6.43,
TIFF 4.5.1, LittleCMS 2.14, and zlib 1.3. Authenticate results rather than
assuming arbitrary library versions reproduce identical pixels or gzip bytes.

The durable reproduction input will be the 251,806,462-byte gzip TGPC,
SHA-256 `e073c59d362df9ffb57e96d48acdb0d0347dc607da872bd9b9376d544532a59a`.
Its uncompressed size is 442,368,256 bytes and SHA-256 is
`acf8483c21e4d8b6f01679e92663d4345fd13244beb853e94a2b799827fd3c35`.
Permanent Zenodo and release-mirror URLs are **pending**. No placeholder is a
download location; originals remain best-effort until that corpus is published.

## Reproduce the model

Build the standalone trainer using the parent README. For canonical identity,
use the recorded Linux x86-64 GCC 13.3.0 environment, fixed locale, and no native
CPU specialization. The C++ source/header/build-input digest remains
`df0fc05b7b62f35c87a8ad42caf07e219721bddc81307cbf2756c9ba14d71ee6`.
Each phase is fitted with one canonical thread and contraction disabled;
independent phase processes can be scheduled separately without changing bytes.

```sh
export LC_ALL=C LANG=C OMP_NUM_THREADS=1 OMP_DYNAMIC=FALSE
"$TOOL" train "$WORK/tgmr-corpus-v1.tgpc.gz" "$WORK/checkpoints" \
    --backend canonical --components 32 --gaussian-iterations 10 \
    --student-iterations 30 --covariance-floor 1e-6 --degrees-of-freedom 3 \
    --batch-size 4096 --maximum-memory-mib 8192
"$TOOL" export --checkpoints "$WORK/checkpoints" "$WORK/model.tgmr" \
    --corpus-sha256 573a0bf7f073282f715fbafc68ace1e8366c8ca19e99e7f2f797a75c11444c74 \
    --attribution-sha256 67c386e1046e250894c3e7c2867bdb1760211a43bc16c219c8a6bc6e19dbd1ad
"$TOOL" validate "$WORK/model.tgmr" "$WORK/tgmr-corpus-v1.tgpc.gz" \
    --split validation > "$WORK/model-validation.json"
"$TOOL" export --checkpoints "$WORK/checkpoints" "$WORK/model-with-validation.tgmr" \
    --corpus-sha256 573a0bf7f073282f715fbafc68ace1e8366c8ca19e99e7f2f797a75c11444c74 \
    --attribution-sha256 67c386e1046e250894c3e7c2867bdb1760211a43bc16c219c8a6bc6e19dbd1ad \
    --validation-report "$WORK/model-validation.json"
```

The `--corpus-sha256` argument is the authenticated **TGPC payload** identity,
not the whole-file or model-payload digest. Both exported model files should
match the bundled model; only the final manifest includes validation evidence.
The clean-branch GCC 13 trainer has already re-exported the preserved canonical
checkpoints into byte-identical model and final companion-manifest files.

The old test population is now diagnostic: its failures influenced subsequent
hard-case experiments. Reproducing those numbers does not create a new untouched
test. See the productization report before interpreting quality or release status.
