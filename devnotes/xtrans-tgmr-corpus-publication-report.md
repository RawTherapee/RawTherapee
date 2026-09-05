# TGMR corpus v1 GitHub publication

## Published artifact

- Repository: <https://github.com/wilx/RT-TGMR-corpus>
- Release: <https://github.com/wilx/RT-TGMR-corpus/releases/tag/tgmr-corpus-v1>
- Frozen tag commit: `19b2a7ea11cf9ce215d996036523897a4213e437`.
- Publication date: 2026-09-05.
- Release-manifest SHA-256:
  `bf9af6a414dc4d26a0a7c4892a1756aa5f54212bcafac78e9f4ebe0a50fcb89a`.
- Complete attachment set: 26 files, 289,085,337 bytes.

This is a maintainer publication, not an official RawTherapee project release.
The standalone repository contains provenance and standard-library-only Python
tools. The large corpus and expanded coordinate manifest are release attachments,
not Git objects. No Git LFS, original photograph, private comparison portrait,
RAF/TIFF benchmark, checkpoint or model weight is distributed by this package.

The main `tgmr-corpus-v1.tgpc.gz` is 251,806,462 bytes, SHA-256
`e073c59d362df9ffb57e96d48acdb0d0347dc607da872bd9b9376d544532a59a`.
It expands to 442,368,256 bytes, SHA-256
`acf8483c21e4d8b6f01679e92663d4345fd13244beb853e94a2b799827fd3c35`.
These are unchanged production-v1 bytes. Original release files, the bundled
model, source selection, training order and attribution identities are preserved.

## Provenance and redistribution audit

The release audit followed the agreed policy of trusting accepted catalog
CC BY/CC0 records with complete attribution. It did not repeat visual review
or independently adjudicate all source landing pages. Existing people and
duplicate decisions remain intact. The selected set contains 4,190 CC BY 2.0,
84 CC BY 3.0, 119 CC BY 4.0 and 607 CC0 sources.

Every source-derived patch retains its individual source terms. The complete
unchanged attribution notice accompanies the corpus, and a transformation
description records rescaling, color conversion, extraction, augmentation,
clipping and quantization. Collection/documentation terms do not replace the
underlying source licenses. Software is GPL-3.0-or-later. No blanket assertion
about model releases, personality rights or separate trained-model licensing
is made.

The audit checked all 5,000 source identities and credit blocks, catalog/split
quotas, author isolation, five-image cap, accepted rights, review status,
file/pixel uniqueness and the 12/1/1 astronomy guardrail. It independently
streamed all 1,152,000 TGPC records against the expanded manifest, verifying
source ordinal/hash, split, unique in-bounds coordinates, exact 75% spatial /
25% coverage sampling and augmentation fields. Recorded stratum counts meet
the frozen 10,000/1,000/1,000 minima.

Importantly, unchanged evaluation payloads do **not** mean identity-only
evaluation: validation/test contain their own frozen transformations, including
held-out camera matrices. The audit checks those actual fields; no data was
changed to simplify that distinction.

Independent native inspection authenticated the payload and all split digests:

| Split | Records | Payload SHA-256 |
| --- | ---: | --- |
| Training | 1,024,000 | `6ab85983c167786609fd38a99c4e55e8885aec85d080a22df5824852c5215518` |
| Validation | 64,000 | `76e1ef73ca5f7ac590f97c0846b13300bb070630a2cce81e4e79b11bef5babe9` |
| Diagnostic test | 64,000 | `658576612d7bf5e07e550a57f2dd153adadf1568e79374d4ac3eab0f8346cc12` |

Detailed scope and machine-readable evidence are included in the release's
`publication-audit.md` and `publication-audit.json`. Complete upstream catalog
dumps are not attachments; their identities and selected-source evidence are.

## Verification

- Two independent empty staging directories produced byte-identical sets of
  all 26 attachments. This reauthenticated and restaged the frozen corpus;
  it was not another training or original-image extraction run.
- GitHub recorded the matching SHA-256 for every uploaded attachment before
  the draft release was published.
- After publication, all 26 attachments were downloaded from public HTTPS
  URLs and compared byte-for-byte with staging. The released Python downloader
  authenticated all 23 payload assets without GitHub credentials; the manifest
  and two checksum/URL control files were also fetched independently. Offline
  verification, `sha256sum -c`, and native streaming inspection of the downloaded
  TGPC passed.
- A fresh public clone at the release tag passed all 12 offline metadata and
  downloader tests. The downloader covers temporary publication, changed files,
  truncation/extension, transient retries, unsafe manifests and offline use.
- [Hosted verification](https://github.com/wilx/RT-TGMR-corpus/actions/runs/33990579109)
  passed on Linux, Windows and macOS. These are corpus-tool tests, not native
  RawTherapee package qualifications.
- All 28 RawTherapee corpus/release tests passed. The privacy history audit
  checked 1,671 files and 2,664 historical blobs with no errors. All 10 privacy
  tests and `git diff --check` passed. No native code or fitting inputs changed,
  so this publication did not require another native build or training run.

## Remaining scope

The GitHub distribution requirement is complete. Zenodo DOI/second-mirror
archival remains pending, as do model-license finalization and the native
platform, GUI and real-camera qualification work. No retraining was performed.
The old test is diagnostic, and publication does not turn accepted experimental
hard-case failures into passed quality gates. Version 1 attachment bytes must
not be replaced; any changed corpus requires a new version.
