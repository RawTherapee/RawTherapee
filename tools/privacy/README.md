# DSCF0771 publication boundary

> Privacy update: DSCF0771 full-frame imagery is private and is not
> distributed. Only the reviewed earring crops may be shared. Historical
> full-frame asset references below describe private benchmark evidence,
> not current publication instructions.

Only the reviewed earring PNGs and crop-only montages may be distributed.
The source RAF, full-resolution TIFFs, full-frame reduced images, thumbnails,
and full-frame comparison montages are private benchmark artifacts outside Git.
Numerical measurements and source-image hashes may remain as provenance.

Run the standard-library-only check before sharing a branch or release:

```sh
python3 tools/privacy/check_dscf0771.py --history
python3 -m unittest discover -s tools/privacy -p 'test_*.py'
```

The default audits tracked working files and (with `--history`) reachable HEAD
history. Use `--revision REF` to audit another complete Git tree and its history.
Audit each published ref separately; `--history` is not a claim that other refs,
reflogs, private backups, forks, cached views, or GitHub PR refs have been cleaned.

The policy pins approved crop bytes, prohibited historical payloads, and reviewed
opaque documents. Its digest checks catch renamed exact copies and embedded
base64 copies; it is not a general face detector or a perceptual image scanner.
Re-encoded, newly named imagery needs human review before adding it to a release.
Unpack release containers into a disposable Git staging repository and audit the
staged contents before upload; archives are not implicitly trusted.

The historical PDF and self-contained HTML paper contain charts, not the
DSCF0771 portrait. Keep their authenticated bytes unchanged. Changed opaque
documents require another image-content review and an explicit policy update.

Do not merge or push the pre-cleanup histories: that can restore the removed
images. Rebase/cherry-pick narrowly reviewed changes onto sanitized history.
Private recovery bundles must never be uploaded as release assets or refs.

The research branch's already-published tip is cleaned independently from its
unpublished local commits. The clean TGMR branch and newer model-bearing work
remain unpublished until the separate redistribution review passes.
