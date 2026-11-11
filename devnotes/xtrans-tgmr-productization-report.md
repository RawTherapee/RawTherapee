# TGMR experimental productization and clean-branch report

> Privacy update: DSCF0771 full-frame imagery is private and is not
> distributed. Only the reviewed earring crops may be shared. Historical
> full-frame asset references below describe private benchmark evidence,
> not current publication instructions.

## Decision and scope

The unchanged production-v1 model is the local **experimental release
candidate**. No further retraining is required for this branch. The hard-case
retraining study is closed as **NO-GO**: replacing up to 5% of training patches
with synthetic examples did not sufficiently repair the known failures.

Sparse digitally sharp borders, one-pixel structures, and some isolated
chromatic highlights can produce false colour or ringing. This is not evidence
that such structures cannot occur in photographs. Runtime fallback handles
model/loading/CFA/allocation/non-finite execution errors; it cannot detect
these image-quality limitations. Markesteijn three-pass remains the default.

This branch does **not** claim the original quality release gates passed.
The owner's explicit decision accepts these limitations for an experimental
feature. The sanitized branch has since been published at the owner's request;
that is not a completed model-redistribution review. Intended model terms remain
CC BY 4.0. Corpus publication is now complete on GitHub as described below;
model-license finalization and the broader platform/camera matrix remain open
for release qualification and merge-readiness.

## Branch and selective port

`codex/xtrans-tgmr-productization` was created in a separate worktree from a
fresh fetch of `upstream/dev` at
`498f623784e33fd9a7077fcd8937fe0734033366`. No broad experiment commit was
cherry-picked. The original research branch and all external baseline files
remain retained, and its unrelated untracked `Testing/` directory is untouched.

The [pre-port inventory](tgmr-clean-port-inventory.json) freezes source and
artifact identities. The trainer source/header/build inputs are taken from
`0043c7d9c15588022614066402b26fc2d8f20c3e`, before the hard-case additions.
The port excludes the sensor-physical renderer, synthetic replacement
generator, screening commands, and their associated tests. Existing ordinary
exposure/white-balance/camera-matrix augmentation and optional sensor noise are
the original corpus-v1 functionality and remain available.

Upstream already uses C++17 and had moved RAW parameter definitions into
`rtengine/params/raw.h` and `raw.cc`; integration follows that layout instead
of copying the old monolithic parameter files. Existing GUI method-list,
history, profile, edited-state, and partial-paste machinery is reused.
Only the English TGMR label and tooltip are added; translation fallback is
unchanged, with no invented translations. No other experimental demosaicer
or neural runtime is included.

## Frozen model and corpus

The installed candidate is `rtdata/models/xtrans-tgmr-v2.tgmr`:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| TGMR v2 model | 6,073,768 | `5707fbd67d1998ed3bac646ecce967297a2022776821d62944a24dbbb8615285` |
| Canonical companion manifest | 77,764 | `d753847f9389d5dd07a04d10013f81ffe85c4cfda1dd5120f90eb3e6e46d79af` |
| Complete corpus notice | 1,266,232 | `67c386e1046e250894c3e7c2867bdb1760211a43bc16c219c8a6bc6e19dbd1ad` |
| Ordered selected-source JSONL | 12,857,532 | `cb703713bec1d373bd41b01c5b98f311b93513dd4a813966414aa03cc78bd909` |
| Nested training-order JSON | 201,718 | `981316001ad70d53abee3d41f6341f0efa0bb96997cee4f81a16f998f0854d81` |

The model was fitted from 4,000 training photographs: 3,200 Open Images,
480 Commons, and 320 Smithsonian. Validation and test each have 500 separate
sources in the same proportions. There are 1,024,000 training and 64,000
records in each evaluation split, with author-isolated splits and at most five
photographs per normalized author. The complete 5,000-source attribution and
rights evidence are retained, not an abbreviated training-only notice.

The selected augmentation is `production-v1`, without sensor noise. The
compressed patch corpus is external: 251,806,462 bytes (approximately 240 MiB),
expanding to 442,368,256 bytes. The large patch-expanded JSONL, TGPC files,
checkpoints, original photographs, and full-resolution TIFFs are not in Git.
See the [corpus reproduction instructions](../tools/tgmr_trainer/corpus-v1/README.md)
and [release identities](../tools/tgmr_trainer/corpus-v1/release-metadata.json).

Two original canonical fits produced byte-identical checkpoints, models, and
final manifests. This clean branch preserves the trainer revision digest
`df0fc05b7b62f35c87a8ad42caf07e219721bddc81307cbf2756c9ba14d71ee6`.
A clean standalone GCC 13 build re-exported the retained canonical checkpoints
into model and companion-manifest files that match the bundled artifacts
byte-for-byte. This was an export reproduction, not another training run.

## Quality evidence and accepted failed gates

The original balanced-phase comparison covered all 64,000 coordinates from
500 previously untouched test photographs. Its
[canonical report](../tools/tgmr_trainer/corpus-v1/diagnostic-test-comparison.json)
has SHA-256 `9e2b799fa829d8cac33576a113594891caf0396a1f30037f6164474aff8a6dba`.

| Original test metric | TGMR | Markesteijn |
| --- | ---: | ---: |
| Pooled PSNR | 34.063575 dB | 32.316274 dB |
| p99 patch RMS | 0.095072 | 0.115206 |
| Worst patch RMS | 0.408201 | 0.483703 |
| Median source PSNR | 36.312600 dB | 34.701333 dB |

TGMR won 408/500 sources, with a median paired gain of 1.625443 dB. However,
92 sources lost, 65 lost more than 0.5 dB, and the worst source lost
6.169889 dB. The pooled gain of 1.747301 dB missed the original 2 dB gate,
and the worst-source gate also failed. These are accepted experimental
limitations, not passing safety results. The test set became diagnostic after
those failures influenced the hard-case experiment.

The clean GCC 13 evaluator repeated all 64,000 coordinates from the same
500-source population in 275.81 seconds, with 3,641,460 KiB peak RSS.
Every field of its TGMR statistics matches the original canonical report
exactly. Clean-branch Markesteijn pooled PSNR is 32.317383 dB, a 0.001108 dB
change; the resulting TGMR advantage is 1.746192 dB. Its p99 and worst patch
RMS remain unchanged, and full-image/cropped-center Markesteijn parity is
exact for the evaluator's proof case. This is a port regression check, not
new test data. The [clean diagnostic report](tgmr-clean-population-comparison.json)
is retained separately instead of replacing the original evidence. Neither
the original nor clean result passes the original pooled/worst-source gates.

At the 1,000-source hard-case screen, the best direct-extraction synthetic
candidate reduced held-out synthetic mean MSE by only 15.83%, short of the
50% requirement. Sensor-physical candidates lost 0.177–0.312 dB on ordinary
validation. No full factorial winner or new release model was produced.
The original results and generators remain on the research branch; only this
conclusion and the unchanged candidate are carried here.

## Runtime and installation

`Student-t GMR (experimental)` is visible for X-Trans. Its PP3 identifier is
`tgmr`. Markesteijn remains the default (`3-pass (best)` in PP3).
`cmake/TgmrModel.cmake` pins the model, manifest, and attribution digests and
checks the model's embedded attribution binding before installing them under
the ordinary RawTherapee data directory. Exact-byte Git attributes protect
authenticated metadata from platform newline conversion. The original notice
separator and TSV empty final column are intentionally preserved.

The engine discovers only the pinned installed model by default.
`RT_XTRANS_TGMR_MODEL` remains an explicit structurally compatible custom-model
override, with official/custom origin diagnostics. Failed loads are not cached.
Runtime model bytes contain data only; neither JSON nor source-key strings are
used for inference. Scalar, targeted AVX2/FMA, and ARM64/NEON paths share the
fixed K32/S9/q8 contract, native-sample restoration, and bounded tiled workspace.

`BUILD_TGMR_TRAINER=OFF` is the default. A separate standalone CMake project
builds the trainer without `rtengine` or the GUI. No Torch, ONNX, TVM, MIGraphX,
Python runtime, curl, archive library, or new compression dependency is linked
into RawTherapee. `BUILD_TESTING=OFF` adds neither trainer tests nor native
development test/evaluator targets. GPU fitting remains optional and unqualified.

## Clean-branch verification

The [machine-readable verification record](tgmr-clean-port-verification.json)
records final file identities and local evidence.

- GCC 13.3.0 portable Release: CLI, trainer, native tests, and population
  evaluator built; all six focused CTests passed. The standalone trainer also
  built independently and reproduced the canonical export identity.
- GCC 14.2.0 Release: GUI and CLI built and installed; all four runtime CTests
  passed with the trainer disabled.
- Clang 18.1.3 Debug with ASan/UBSan: CLI/trainer/native targets built; all six
  focused CTests passed with both halt-on-error and leak detection enabled.
- GCC 13/GCC 14/Clang strict source checks passed for the TGMR runtime, shared
  CFA utility, trainer sources, tests, and evaluator. Whole-tree Clang `-Werror`
  remains blocked by upstream variable-length arrays and `class`/`struct`
  forward-declaration mismatches. Only the unrelated header tag warning was
  made non-fatal in the focused Clang check; it was not globally suppressed
  in source or represented as a clean whole-tree strict build.
- All 27 offline Python corpus/release tests passed, including actual model,
  notice, source quotas, author isolation, 12/1/1 astronomy coverage, and order
  authentication. A missing Python astronomy-tag entry was corrected to match
  the existing native manifest/schema without changing trainer source identity.
- Native tests cover malformed/incompatible models, digest/size/NaN/Cholesky
  rejection, cache identity and retry, PP3 round trips, default preservation,
  edited-state/partial-paste/batch semantics, all 18 CFA matrices, tiny and odd
  images, seams/crop origins, finite output, and exact measured samples.
- On a deterministic 141×133 mosaic, scalar outputs are byte-identical across
  the three compilers; AVX2 outputs are also byte-identical across them.
  Scalar versus AVX2 is **numeric**, not bitwise, parity: maximum normalized
  difference `3.5762787e-7`, RMS `4.9053107e-8`, below the frozen `1e-5` maximum
  and `1e-6` RMS bounds. Native tests also exercise seven analytical scenes.
- GCC 13 AArch64 cross-compilation of the NEON implementation passed with
  `-Wall -Wextra -Werror`, using the host GLib header interfaces. This proves
  compilation, not an ARM64 linked package or native execution.
- Installed CLI dependency inspection found ordinary RawTherapee libraries
  only. Ubuntu TIFF/JXL dependencies can load Zstandard transitively; TGMR
  adds no direct Zstandard requirement.
- Authenticated paper claims and all archived paper/figure/output hashes
  passed. The manuscript describes the earlier research model, not these
  production weights; its benchmark claims are not relabeled as production
  results. PDF rendering itself was not repeated for this port.

Sanitizers exposed two pre-existing operations now corrected because the
focused feature tests exercise them: full-profile framing saves dereferenced a
null edited-state filter, and Markesteijn shifted a negative signed offset.
The former now supplies an empty filter for a full save; the latter negates
an already-shifted nonnegative offset. The developer Markesteijn evaluator
poisons every destination with NaN before interpolation, proving all output
pixels are overwritten when its finite-output check succeeds.

## Real RAF reproduction and fallback

The installed GCC 14 CLI selected the bundled model without an override and
processed DSCF0771, DSCF5043, DSCF5595, and DSCF3790 successfully. Every run
reported the exact model digest and `origin=official-v2`, with no fallback.
These are four X-T50 photographs, **not** generation I–V camera coverage.

DSCF0771 produced a complete 7752×5178 16-bit RGB TIFF. Its pixels and ICC
profile match the preserved production-v1 TIFF exactly. The full-frame TIFF
and reduced portrait remain private benchmark evidence outside Git. Only the
earring derivative is distributable; its original PNG bytes are preserved:

- [Earring, 500%](images/xtrans-neural/DSCF0771/DSCF0771-tgmr-production-v1-earring-500.png):
  `(3510,1930,140,160)` enlarged to 700×800 by nearest neighbour.
- [Crop-only derivative identity manifest](images/xtrans-neural/DSCF0771/tgmr-production-v1-manifest.json).

The one-off DSCF0771 privacy cleanup removed the complete portrait from the branch's
history, rather than relying on a deletion commit. Private source hashes and
numerical measurements are retained; neither the model nor its corpus changes.

Single descriptive exports during other verification work took 10.19, 10.62,
10.35, and 10.42 seconds respectively, at approximately 1.90 GiB maximum RSS.
These are not controlled warm-up/three-run performance gates.

Missing and corrupt model overrides emitted `IO` and `DIGEST` fallback
diagnostics and completed full Markesteijn exports. All interior pixels match
explicit Markesteijn exactly. Whole-TIFF byte equality is not claimed:
Markesteijn-only repeated runs themselves showed small bottom-edge variation
with `Border=0` (1,142 uint16 samples, maximum 529 codes, restricted to rows
5131–5169 in the repeated control). Fallback runs have the same localized
behavior, while the NaN-poisoned native test independently verifies complete
RGB overwrite. This remaining Markesteijn repeatability issue is not a TGMR
quality detector and is not silently counted as exact whole-frame parity.

## Corpus publication — 2026-09-05

The frozen corpus and provenance are published in the independent
[RT-TGMR-corpus repository](https://github.com/wilx/RT-TGMR-corpus) and its
[tgmr-corpus-v1 release](https://github.com/wilx/RT-TGMR-corpus/releases/tag/tgmr-corpus-v1).
Large files are release attachments, not Git objects; no Git LFS is required.
The 26 attachments total 289,085,337 bytes and include the unchanged TGPC gzip,
expanded coordinate manifest, compact source/order metadata, full attribution,
rights evidence, transformations, reports and reconstruction tools. Neither
original photographs nor model weights are part of this corpus package.

The [publication report](xtrans-tgmr-corpus-publication-report.md) records the
audit and download verification. The existing human people/duplicate decisions
were retained, not reopened. Catalog rights are assessed under the previously
agreed metadata policy; this is not independent legal certification or an
approval of the separate model license. The test split stays diagnostic and
the known model-quality failures remain documented.

## Paper rewrite — 2026-09-05

The [TGMR manuscript](../doc/papers/xtrans-tgmr/paper.md) now describes the
production-v1 candidate and published corpus, with the BSDS work labeled as
retrospective development evidence. It corrects the three-channel center-RGB
PSNR definition, documents the actual coordinate-selection and augmentation
recipe, gives the fixed-degree-of-freedom fitting equations, and distinguishes
tempered shortlist inference from exact conditional MMSE. The original failed
quality gates and unsuccessful hard-case retraining remain explicit.

Fifteen evidence-binding tests now check the manuscript's generated tables,
abstract, metric contracts and pinned model/evidence identities. The 20-page
PDF and self-contained HTML were rebuilt with Pandoc 3.10.2; independent final
renders have identical HTML bytes and PDF text/rasterized pages, but not PDF
bytes. Only chart/diagram imagery is embedded. The new renders were reviewed
for image content and their hashes are recorded in the paper manifest. The
one-off cleanup tooling and dedicated CI workflow were subsequently retired;
there is no ongoing document-hash allowlist. Prior paper revisions remain in
Git. No training inputs, model bytes or earring crops changed.

## Remaining public-release work

1. Finalize the separate attribution/model-redistribution review and intended
   CC BY 4.0 model notice. The corpus audit below does not settle that question.
2. Archive the identical published corpus/provenance assets on Zenodo if the
   planned second mirror and DOI are retained. GitHub distribution is complete;
   only the second-location download/reauthentication remains open.
3. Complete Windows/macOS package installation and data discovery, native
   ARM64/NEON execution, and interactive GUI/preview/export QA. Current
   edited-state checks test parameter semantics, not mouse-driven GUI behavior.
4. Complete public Fujifilm X-Trans generation I–V coverage and the broader
   external chromatic/star/control qualification. Existing analytical/native
   checks and the four X-T50 exports are not substitutes for that matrix.
5. Retain the original hard-case failures as accepted experimental limitations.
   Any later claim of stronger generalization needs a newly frozen untouched
   test population; do not repurpose the old diagnostic test as unseen data.

There is no additional retraining, corpus publication, push, or GPU requirement
as a prerequisite to this **local clean branch**. The remaining work happens
here, separate from the preserved broad experimental history.
