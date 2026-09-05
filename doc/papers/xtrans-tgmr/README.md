# TGMR paper — 5 September 2026 revision

This directory contains the source for *Phase-Conditioned Student-t Mixture
Regression for X-Trans Demosaicing*. The paper is licensed under CC BY 4.0.
Dataset, model, and third-party software rights are separate.

This revision describes the current production-v1 candidate and published
5,000-source corpus, while retaining explicitly retrospective BSDS development
results. Earlier manuscript revisions remain in Git; no duplicate historical
PDF is created here. The current model has important accepted experimental
limitations, including failed pooled-gain and worst-source gates. This paper
does not turn them into passed release qualifications.

The rewrite corrects the RGB PSNR denominator (three channels, including the
exact native sample), spells out the Student-t fitting updates and tempered
shortlist approximation, and describes the actual deterministic coordinate
selection code. The all-phase validation RMS, balanced-phase comparison RMS,
and historical scalar-error p99 are explicitly different metrics.

The build uses a pinned `pandoc/extra` Ubuntu image for TeX and Eisvogel, then
replaces its older Pandoc executable with the authenticated upstream Pandoc
3.10.2 Linux amd64 binary. Docker BuildKit verifies the download digest before
the binary is installed.

From this directory:

```sh
make all
```

Outputs:

```text
doc/papers/xtrans-tgmr/output/pdf/xtrans-tgmr-paper.pdf
doc/papers/xtrans-tgmr/output/html/xtrans-tgmr-paper.html
```

The build first runs `scripts/verify_claims.py` and its offline unit tests. The
verifier authenticates nine model/evidence inputs, validates metric counts
and arithmetic, checks the abstract's production numbers and specified method
qualifications, and compares seven manuscript blocks against tables generated
from the actual JSON. It does not independently validate every scientific or
legal claim in free prose. Tests deliberately change manuscript numbers,
denominators, evidence bytes, source IDs, block markers, and model identity.
Pandoc then processes `citations.yml` with citeproc and the tracked IEEE CSL.

To refresh generated blocks after an intentional, reviewed evidence change:

```sh
python3 scripts/verify_claims.py --update
make verify
```

The evidence is in `devnotes/images/xtrans-tgmr*/results.json`,
`tools/tgmr_trainer/corpus-v1/{diagnostic-test-comparison,statistics,release-metadata}.json`,
and `rtdata/models/xtrans-tgmr-v2.tgmr{,.json}`. Their pinned identities are in
the verifier. A compact `evidence/gaussian-baseline.json` excerpt retains the
Gaussian comparator from the historical research branch, with its source Git
revision, URL, JSON pointer, and complete-source hash. The latter and its model
identity must match the frozen Student-t study's recorded parent. The build
does not need that research branch checked out or access to the network.
`manifest.sha256` separately covers the paper's source, build
inputs, tests, figures, and two rendered outputs. Check it from the repository
root with `sha256sum -c doc/papers/xtrans-tgmr/manifest.sha256`.

The CSL is a whitespace-compacted copy of the official IEEE style at
`citation-style-language/styles` revision
`0819c0e0b7d4a0301ff063521f91818cb697ca5b`. Its tracked SHA-256 is
`74301c40ad44d0bb51b5db8ac58cefbd31d8024a6c6b1dd83e999598fa6199f5`, and it
retains the upstream CC BY-SA 3.0 notice.

The current container target is Linux amd64 because the exact Pandoc 3.10.2
binary archive is architecture-specific. The paper itself remains ordinary
Markdown and can be rendered by any conforming Pandoc 3.10.2 installation.

The render retains XeLaTeX, the title page, and TOC. `SOURCE_DATE_EPOCH` pins
the revision date, not all PDF bytes. Earlier pinned-toolchain checks found
font-subset/trailer variation in XeLaTeX PDFs and in a LuaLaTeX control; changing
the engine alone did not establish binary reproducibility. The distributed PDF
has a recorded artifact hash, not a claim of byte-identical independent
renders. Compare repeated HTML bytes, extracted PDF text, page geometry, and
rasterized pages separately.

Only diagrams and a research quality–cost plot are embedded. No DSCF0771
full-frame photograph or new portrait is included. Future PDF/HTML revisions
should retain this crop-only publication boundary during visual review; the
completed one-off cleanup has no ongoing automated privacy gate or document
allowlist. This paper-only workflow does not modify the model,
corpus, trainer, runtime, or existing earring pixels.

## Verification of this revision

- Pinned Pandoc 3.10.2/citeproc/XeLaTeX build completed without warnings; all
  15 evidence-binding tests passed. The build requires no additional TeX
  package for the adjusted running header and TOC.
- Two final builds produced byte-identical self-contained HTML. The PDFs have
  identical extracted text and all 20 rasterized pages match at a 1,200-pixel
  long edge; PDF file bytes differ. All pages were visually inspected,
  including equations, tables, the two chart/diagram assets and references.
- HTML structure contains 63 MathML expressions, 14 resolved bibliography
  entries, two embedded images, no scripts and no broken internal links.
  Browser-level local-file preview was blocked by the app policy; HTML visual
  browser QA is not claimed.
- The current render hashes are recorded in `manifest.sha256`; image content
  was reviewed before publication. The one-off privacy cleanup tooling and CI
  workflow have been retired without changing the sanitized history.
- No new fitting, native benchmark, corpus modification, model change, or
  photographic derivative was performed for this revision.
