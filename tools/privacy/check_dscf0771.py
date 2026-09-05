#!/usr/bin/env python3
"""Audit distributable DSCF0771 assets and reachable Git history (stdlib only)."""

import argparse
import base64
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess


class PrivacyError(ValueError):
    pass


def read_policy(path):
    def pairs(items):
        out = {}
        for key, value in items:
            if key in out:
                raise PrivacyError(f"duplicate policy field: {key}")
            out[key] = value
        return out
    policy = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs)
    if policy.get("format") != "rawtherapee-dscf0771-privacy-v1":
        raise PrivacyError("unknown privacy policy")
    return policy


def check_bytes(path, data, policy):
    """Identity guard, not a general face detector; changed documents need review."""
    name = path.lower()
    digest = hashlib.sha256(data).hexdigest()
    if digest in policy["prohibited_sha256"]:
        raise PrivacyError(f"prohibited full-frame payload: {path}")
    suffix = PurePosixPath(name).suffix
    media = suffix in {".raf", ".raw", ".dng", ".tif", ".tiff", ".png",
                       ".jpg", ".jpeg", ".webp", ".gif", ".bmp", ".avif"}
    if "dscf0771" in name and media:
        expected = policy["allowed_crops"].get(path)
        if not expected or expected["sha256"] != digest:
            raise PrivacyError(f"unreviewed DSCF0771 image: {path}")
    # Archive/bundle content must be expanded before auditing for publication.
    if "dscf0771" in name and suffix in {".zip", ".gz", ".bz2", ".xz", ".tar", ".7z"}:
        raise PrivacyError(f"expand and audit this DSCF0771 container: {path}")
    embedded = list(re.finditer(rb"data:image/[^;\s]+;base64,([A-Za-z0-9+/=\s]+)", data))
    for match in embedded:
        try:
            payload = base64.b64decode(re.sub(rb"\s", b"", match[1]), validate=True)
        except ValueError as exc:
            raise PrivacyError(f"invalid embedded image: {path}") from exc
        if hashlib.sha256(payload).hexdigest() in policy["prohibited_sha256"]:
            raise PrivacyError(f"embedded full-frame payload: {path}")
    opaque_document = (
        suffix == ".pdf"
        or (suffix in {".html", ".svg"} and
            (embedded or b"dscf0771" in data.lower()))
    )
    if opaque_document and digest not in policy["reviewed_document_sha256"]:
        raise PrivacyError(f"document needs explicit image review: {path}")


def check_asset_manifest(path, data, policy):
    if "devnotes/images/xtrans-neural/DSCF0771/" not in path or not path.endswith(".json"):
        return
    record = json.loads(data)
    def visit(value):
        if isinstance(value, dict):
            filename = value.get("filename")
            if filename and re.search(r"\.(png|jpe?g|tiff?|webp|raf)$", filename, re.I):
                crop_path = str(PurePosixPath(path).parent / filename)
                expected = policy["allowed_crops"].get(crop_path)
                if not expected or value.get("sha256") != expected["sha256"]:
                    raise PrivacyError(f"manifest advertises an unreviewed image: {filename}")
            for child in value.values():
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)
    # Source RAW/TIFF identities are private provenance, not distributed assets.
    for key in ("assets", "png_assets"):
        if key in record:
            visit(record[key])


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def audit(repo, policy, revision=None, history=False):
    errors = []
    if revision:
        records = git(repo, "ls-tree", "-rz", "--full-tree", revision).split(b"\0")
        files = [(r.split(b"\t", 1)[1].decode(), r.split()[2].decode())
                 for r in records if r and r.split()[1] == b"blob"]
    else:
        files = [(p.decode(), None) for p in git(repo, "ls-files", "-z").split(b"\0") if p]
    for path, blob in files:
        try:
            data = git(repo, "cat-file", "blob", blob) if blob else (repo / path).read_bytes()
            check_bytes(path, data, policy)
            check_asset_manifest(path, data, policy)
        except (OSError, PrivacyError, ValueError) as exc:
            errors.append(str(exc))
    historical_blobs = 0
    if history:
        ref = revision or "HEAD"
        lines = git(repo, "rev-list", "--objects", ref).decode().splitlines()
        forbidden = set(policy["prohibited_git_blobs"])
        candidates = []
        for line in lines:
            sha, _, path = line.partition(" ")
            if sha in forbidden:
                errors.append(f"prohibited historical blob reachable from {ref}: {sha}")
            # Documents can embed renamed/base64 imagery even after deletion.
            if path and ("dscf0771" in path.lower() or
                         PurePosixPath(path).suffix.lower() in
                         {".md", ".html", ".svg", ".pdf", ".json", ".txt"}):
                candidates.append((sha, path))
        with subprocess.Popen(["git", "-C", str(repo), "cat-file", "--batch"],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE) as reader:
            try:
                for sha, path in candidates:
                    reader.stdin.write((sha + "\n").encode())
                    reader.stdin.flush()
                    fields = reader.stdout.readline().split()
                    size = int(fields[2])
                    data = reader.stdout.read(size)
                    if reader.stdout.read(1) != b"\n" or len(data) != size:
                        raise PrivacyError("short Git object read")
                    if fields[1] != b"blob":
                        continue
                    historical_blobs += 1
                    try:
                        check_bytes(path, data, policy)
                    except PrivacyError as exc:
                        errors.append(str(exc))
            finally:
                reader.stdin.close()
    return {"format": "rawtherapee-dscf0771-privacy-audit-v1",
            "files_checked": len(files), "historical_blobs_checked": historical_blobs,
            "history_checked": history, "errors": sorted(set(errors)), "pass": not errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--policy", type=Path, default=Path(__file__).with_name("dscf0771-policy.json"))
    parser.add_argument("--revision", help="audit a Git tree instead of tracked working files")
    parser.add_argument("--history", action="store_true", help="also reject reachable historical payloads")
    args = parser.parse_args()
    result = audit(args.repo, read_policy(args.policy), args.revision, args.history)
    print(json.dumps(result, sort_keys=True, indent=2))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
