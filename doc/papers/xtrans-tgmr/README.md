# Building the TGMR paper

This directory contains the source for *Phase-Conditioned Student-t Mixture
Regression for X-Trans Demosaicing*. The paper is licensed under CC BY 4.0.
Dataset, model, and third-party software rights are separate.

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

The build first runs `scripts/verify_claims.py`, which binds the headline
numbers in the manuscript to the tracked canonical experiment JSON. It then
uses Pandoc citeproc with `citations.yml` and the tracked IEEE CSL style.

The CSL is a whitespace-compacted copy of the official IEEE style at
`citation-style-language/styles` revision
`0819c0e0b7d4a0301ff063521f91818cb697ca5b`. Its tracked SHA-256 is
`74301c40ad44d0bb51b5db8ac58cefbd31d8024a6c6b1dd83e999598fa6199f5`, and it
retains the upstream CC BY-SA 3.0 notice.

The current container target is Linux amd64 because the exact Pandoc 3.10.2
binary archive is architecture-specific. The paper itself remains ordinary
Markdown and can be rendered by any conforming Pandoc 3.10.2 installation.

With the pinned inputs and `SOURCE_DATE_EPOCH`, repeated builds have identical
page geometry, extracted text, and rendered content. The self-contained HTML
is byte-identical. The XeLaTeX PDF is not byte-identical because `xdvipdfmx`
randomizes embedded-font subset names and its trailer identifier; a LuaLaTeX
control exhibited the same subset-name variation and additional DejaVu italic
fallback warnings. The distributed PDF therefore has a recorded artifact hash,
but that hash is not presented as reproducible across independent renders.
