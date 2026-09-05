#!/usr/bin/env python3
"""Reconstruct and authenticate the TGMR production source corpus.

This downloader deliberately uses only the Python standard library.  It never
changes the reviewed manifest and never substitutes bytes whose SHA-256 does
not match the selected record.  The durable training input is the separately
published TGPC bundle; this tool makes the original-source audit reproducible
as long as third-party URLs remain available.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePath
import shutil
import sys
import tarfile
import time
from typing import BinaryIO, Iterable
import urllib.error
import urllib.request


FORMAT_V1 = "rawtherapee-tgmr-corpus-source-manifest-v1"
FORMAT_V2 = "rawtherapee-tgmr-corpus-source-manifest-v2"
# Retained for the v1 fixture API and older callers.
FORMAT = FORMAT_V1
REPORT_FORMAT = "rawtherapee-tgmr-corpus-reconstruction-report-v1"
SPLITS = frozenset(("train", "validation", "test"))
LICENSES = frozenset(("CC0-1.0", "PDM-1.0", "CC-BY-2.0", "CC-BY-3.0", "CC-BY-4.0"))
CHUNK = 1024 * 1024


class ManifestError(ValueError):
    """The tracked manifest violates its strict contract."""


def canonical_json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode()


def _sha256(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while block := stream.read(CHUNK):
            digest.update(block)
            size += len(block)
    return digest.hexdigest(), size


def _require_string(record: dict[str, object], name: str) -> str:
    value = record.get(name)
    if not isinstance(value, str) or not value:
        raise ManifestError(f"{name} must be a non-empty string")
    return value


def _safe_filename(value: str) -> str:
    path = PurePath(value)
    if (
        path.is_absolute()
        or not path.parts
        or any(component in ("", ".", "..") for component in path.parts)
    ):
        raise ManifestError("cache_filename must be a safe relative path")
    return value


def _canonical_sha256(record: dict[str, object], name: str) -> str:
    value = _require_string(record, name).lower()
    if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
        raise ManifestError(f"{name} is not canonical SHA-256")
    if value != record[name]:
        raise ManifestError(f"{name} must use lowercase hexadecimal")
    return value


def _exact_integer(record: dict[str, object], name: str, minimum: int, maximum: int) -> int:
    value = record.get(name)
    if not isinstance(value, int) or isinstance(value, bool) or not minimum <= value <= maximum:
        raise ManifestError(f"{name} must be an integer in {minimum}..{maximum}")
    return value


def validate_record(value: object, line_number: int) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ManifestError(f"line {line_number}: record must be an object")
    record = value
    version = record.get("format")
    if version not in (FORMAT_V1, FORMAT_V2):
        raise ManifestError(f"line {line_number}: wrong record format")
    allowed = {
        "format", "source_id", "split", "selected", "selection_status",
        "original_url", "fallback_urls", "landing_page", "author", "title",
        "license", "license_url", "advertised_checksum", "sha256",
        "decoded_pixel_sha256", "cache_filename", "file_type", "width",
        "height", "orientation", "icc_identity", "classification",
        "patch_sampling_seed", "patch_coordinates",
    }
    if version == FORMAT_V2:
        allowed |= {
            "archive_fallbacks", "author_id", "author_url", "catalog",
            "content_tags", "people_review_status", "rights",
            "upstream_flickr_id", "upstream_source_id",
        }
    unknown = sorted(set(record) - allowed)
    if unknown:
        raise ManifestError(f"line {line_number}: unknown fields: {', '.join(unknown)}")
    source_id = _require_string(record, "source_id")
    if any(not (character.isascii() and (character.isalnum() or character in "._:-"))
           for character in source_id):
        raise ManifestError(f"line {line_number}: source_id contains nonportable characters")
    split = _require_string(record, "split")
    if split not in SPLITS and not (version == FORMAT_V2 and split == "unassigned"):
        raise ManifestError(f"line {line_number}: invalid split {split!r}")
    if not isinstance(record.get("selected"), bool):
        raise ManifestError(f"line {line_number}: selected must be Boolean")
    if record["selected"] and split == "unassigned":
        raise ManifestError(f"line {line_number}: selected source cannot be unassigned")
    _require_string(record, "selection_status")
    landing_page = _require_string(record, "landing_page")
    _require_string(record, "author")
    _require_string(record, "title")
    license_id = _require_string(record, "license")
    if record["selected"] and license_id not in LICENSES:
        raise ManifestError(f"line {line_number}: selected source has prohibited license {license_id!r}")
    license_url = _require_string(record, "license_url")
    original_url = _require_string(record, "original_url")
    if not original_url.startswith(("https://", "http://", "file://")):
        raise ManifestError(f"line {line_number}: unsupported original URL scheme")
    if not landing_page.startswith(("https://", "http://", "file://")):
        raise ManifestError(f"line {line_number}: unsupported landing-page URL scheme")
    if not license_url.startswith(("https://", "http://", "file://")):
        raise ManifestError(f"line {line_number}: unsupported license URL scheme")
    fallback = record.get("fallback_urls")
    if not isinstance(fallback, list) or any(
        not isinstance(url, str) or not url.startswith(("https://", "http://", "file://"))
        for url in fallback
    ):
        raise ManifestError(f"line {line_number}: fallback_urls must be URL strings")
    _canonical_sha256(record, "sha256")
    _canonical_sha256(record, "decoded_pixel_sha256")
    _safe_filename(_require_string(record, "cache_filename"))
    _require_string(record, "file_type")
    _exact_integer(record, "width", 7, 2**32 - 1)
    _exact_integer(record, "height", 7, 2**32 - 1)
    _exact_integer(record, "orientation", 1, 8)
    _require_string(record, "icc_identity")
    classification = record.get("classification")
    if not isinstance(classification, dict):
        raise ManifestError(f"line {line_number}: classification must be an object")
    classification_fields = {
        "channel_means", "chroma_ratio_mean", "clipped_black_fraction",
        "clipped_white_fraction", "gradient_rms", "hue_degrees",
        "jpeg_blockiness", "laplacian_rms", "local_contrast", "luminance_mean",
        "luminance_p01", "luminance_p99", "luminance_stddev", "perceptual_hash",
        "saturation_mean",
    }
    if version == FORMAT_V2:
        classification_fields |= {
            "dhash", "phash", "luminance_histogram", "hue_histogram",
            "saturation_histogram",
        }
    if set(classification) != classification_fields:
        raise ManifestError(f"line {line_number}: classification fields do not match the frozen contract")
    channel_means = classification.get("channel_means")
    if (not isinstance(channel_means, list) or len(channel_means) != 3
            or any(not isinstance(item, (int, float)) or isinstance(item, bool)
                   or not float("-inf") < item < float("inf") for item in channel_means)):
        raise ManifestError(f"line {line_number}: channel_means must contain three finite numbers")
    aggregate_fields = {
        "channel_means", "perceptual_hash", "dhash", "phash",
        "luminance_histogram", "hue_histogram", "saturation_histogram",
    }
    for name in classification_fields - aggregate_fields:
        measurement = classification.get(name)
        if (not isinstance(measurement, (int, float)) or isinstance(measurement, bool)
                or not float("-inf") < measurement < float("inf")):
            raise ManifestError(f"line {line_number}: {name} must be finite")
    perceptual_hash = classification.get("perceptual_hash")
    if perceptual_hash is not None and (
        not isinstance(perceptual_hash, str)
        or len(perceptual_hash) != 16
        or any(character not in "0123456789abcdef" for character in perceptual_hash)
    ):
        raise ManifestError(f"line {line_number}: perceptual_hash must be 16 lowercase hex digits")
    if version == FORMAT_V2:
        for name in ("dhash", "phash"):
            signature = classification.get(name)
            if (not isinstance(signature, str) or len(signature) != 16
                    or any(character not in "0123456789abcdef" for character in signature)):
                raise ManifestError(f"line {line_number}: {name} must be 16 lowercase hex digits")
        for name, count in (("luminance_histogram", 16), ("hue_histogram", 12),
                            ("saturation_histogram", 8)):
            histogram = classification.get(name)
            if (not isinstance(histogram, list) or len(histogram) != count
                    or any(not isinstance(item, int) or isinstance(item, bool) or item < 0
                           for item in histogram)):
                raise ManifestError(f"line {line_number}: malformed {name}")
    seed = record.get("patch_sampling_seed")
    if not isinstance(seed, (int, str)) or isinstance(seed, bool):
        raise ManifestError(f"line {line_number}: patch_sampling_seed must be an integer or string")
    try:
        numeric_seed = int(seed, 0) if isinstance(seed, str) else seed
    except ValueError as error:
        raise ManifestError(f"line {line_number}: malformed patch_sampling_seed") from error
    if not 0 <= numeric_seed <= 2**64 - 1:
        raise ManifestError(f"line {line_number}: patch_sampling_seed is outside uint64")
    patches = record.get("patch_coordinates")
    if not isinstance(patches, list):
        raise ManifestError(f"line {line_number}: patch_coordinates must be an array")
    for patch in patches:
        if not isinstance(patch, dict) or set(patch) - {
            "x", "y", "coverage", "coverage_class", "augmentation"
        }:
            raise ManifestError(f"line {line_number}: malformed patch coordinate")
        _exact_integer(patch, "x", 0, 2**32 - 1)
        _exact_integer(patch, "y", 0, 2**32 - 1)
        if "coverage" in patch and not isinstance(patch["coverage"], bool):
            raise ManifestError(f"line {line_number}: coverage must be Boolean")
        if "coverage_class" in patch:
            _exact_integer(patch, "coverage_class", 0, 16)
        augmentation = patch.get("augmentation")
        if augmentation is not None:
            if not isinstance(augmentation, dict) or set(augmentation) != {
                "kind", "exposure_stops", "white_balance", "matrix_id", "sequence"
            }:
                raise ManifestError(f"line {line_number}: malformed augmentation")
            _exact_integer(augmentation, "kind", 0, 255)
            exposure = augmentation.get("exposure_stops")
            if (not isinstance(exposure, (int, float)) or isinstance(exposure, bool)
                    or not -2.0 <= exposure <= 2.0):
                raise ManifestError(f"line {line_number}: invalid exposure_stops")
            white_balance = augmentation.get("white_balance")
            if (not isinstance(white_balance, list) or len(white_balance) != 3
                    or any(not isinstance(gain, (int, float)) or isinstance(gain, bool)
                           or not 0.5 <= gain <= 2.0 for gain in white_balance)):
                raise ManifestError(f"line {line_number}: invalid white_balance")
            _exact_integer(augmentation, "matrix_id", 0, 2**16 - 1)
            _exact_integer(augmentation, "sequence", 0, 2**16 - 1)
    if version == FORMAT_V2:
        _require_string(record, "author_id")
        require_urls = ("author_url",)
        for name in require_urls:
            url = _require_string(record, name)
            if not url.startswith(("https://", "http://", "file://")):
                raise ManifestError(f"line {line_number}: invalid {name}")
        catalog = record.get("catalog")
        if not isinstance(catalog, dict) or set(catalog) != {
            "name", "revision", "snapshot_sha256"
        }:
            raise ManifestError(f"line {line_number}: malformed catalog identity")
        if catalog.get("name") not in (
            "openimages-cvdf-v5-boxable", "pass-v3", "wikimedia-commons",
            "smithsonian-open-access"
        ):
            raise ManifestError(f"line {line_number}: unsupported catalog")
        if not isinstance(catalog.get("revision"), str) or not catalog["revision"]:
            raise ManifestError(f"line {line_number}: missing catalog revision")
        _canonical_sha256(catalog, "snapshot_sha256")
        rights = record.get("rights")
        if not isinstance(rights, dict) or set(rights) != {
            "evidence_revision", "evidence_sha256", "evidence_url", "review_status"
        }:
            raise ManifestError(f"line {line_number}: malformed rights evidence")
        _require_string(rights, "evidence_revision")
        _canonical_sha256(rights, "evidence_sha256")
        if not _require_string(rights, "evidence_url").startswith(("https://", "http://", "file://")):
            raise ManifestError(f"line {line_number}: invalid rights evidence URL")
        if rights.get("review_status") not in ("approved", "rejected", "pending"):
            raise ManifestError(f"line {line_number}: invalid rights review status")
        if record["selected"] and rights["review_status"] != "approved":
            raise ManifestError(f"line {line_number}: selected source lacks approved rights review")
        tags = record.get("content_tags")
        accepted_tags = {
            "people", "skin-hair-clothing", "foliage", "fur-feathers",
            "architecture-brick", "textile-print", "metal-specular-jewelry",
            "food", "water-sky", "low-light", "macro-specimen",
        }
        if (not isinstance(tags, list) or len(tags) != len(set(tags))
                or any(tag not in accepted_tags for tag in tags)):
            raise ManifestError(f"line {line_number}: invalid content_tags")
        people = record.get("people_review_status")
        if people not in (
            "not-applicable", "approved-no-minors-or-sensitive-content", "rejected", "pending"
        ):
            raise ManifestError(f"line {line_number}: invalid people review status")
        if (record["selected"] and "people" in tags
                and people != "approved-no-minors-or-sensitive-content"):
            raise ManifestError(f"line {line_number}: selected people image lacks approval")
        _require_string(record, "upstream_source_id")
        flickr = record.get("upstream_flickr_id")
        if flickr is not None and (not isinstance(flickr, str) or not flickr):
            raise ManifestError(f"line {line_number}: invalid upstream_flickr_id")
        archives = record.get("archive_fallbacks")
        if not isinstance(archives, list):
            raise ManifestError(f"line {line_number}: archive_fallbacks must be an array")
        for archive in archives:
            if not isinstance(archive, dict) or set(archive) != {
                "url", "sha256", "member", "member_sha256"
            }:
                raise ManifestError(f"line {line_number}: malformed archive fallback")
            if not _require_string(archive, "url").startswith(("https://", "http://", "file://")):
                raise ManifestError(f"line {line_number}: invalid archive URL")
            _canonical_sha256(archive, "sha256")
            _canonical_sha256(archive, "member_sha256")
            member = _require_string(archive, "member")
            if PurePath(member).is_absolute() or ".." in PurePath(member).parts:
                raise ManifestError(f"line {line_number}: unsafe archive member")
    return record


def read_manifest(path: Path) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    source_ids: set[str] = set()
    filenames: set[str] = set()
    author_splits: dict[str, str] = {}
    content_splits: dict[str, str] = {}
    perceptual_splits: dict[str, str] = {}
    with path.open("r", encoding="utf-8", newline="") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.endswith("\n"):
                raise ManifestError(f"line {line_number}: missing final newline")
            if not line.strip():
                raise ManifestError(f"line {line_number}: blank lines are forbidden")
            try:
                raw = json.loads(line)
            except json.JSONDecodeError as error:
                raise ManifestError(f"line {line_number}: {error}") from error
            record = validate_record(raw, line_number)
            source_id = str(record["source_id"])
            filename = str(record["cache_filename"])
            if source_id in source_ids:
                raise ManifestError(f"line {line_number}: duplicate source_id {source_id!r}")
            if filename in filenames:
                raise ManifestError(f"line {line_number}: duplicate cache_filename {filename!r}")
            source_ids.add(source_id)
            filenames.add(filename)
            if record["selected"]:
                split = str(record["split"])
                author = str(record.get("author_id", record["author"]))
                digest = str(record["decoded_pixel_sha256"])
                perceptual = record["classification"].get("dhash",
                    record["classification"].get("perceptual_hash"))  # type: ignore[union-attr]
                if author in author_splits and author_splits[author] != split:
                    raise ManifestError(f"line {line_number}: author occurs in multiple splits")
                if digest in content_splits:
                    raise ManifestError(f"line {line_number}: exact decoded-image duplicate")
                if perceptual and perceptual in perceptual_splits and perceptual_splits[perceptual] != split:
                    raise ManifestError(f"line {line_number}: perceptual duplicate occurs in multiple splits")
                author_splits[author] = split
                content_splits[digest] = split
                if perceptual:
                    perceptual_splits[str(perceptual)] = split
            records.append(record)
    selected = [record for record in records if record["selected"]]
    for left_index, left in enumerate(selected):
        left_hash = int(str(left["classification"]["perceptual_hash"]), 16)  # type: ignore[index]
        for right in selected[left_index + 1:]:
            if left["split"] == right["split"]:
                continue
            right_hash = int(str(right["classification"]["perceptual_hash"]), 16)  # type: ignore[index]
            if bin(left_hash ^ right_hash).count("1") <= 5:
                raise ManifestError(
                    "perceptual near-duplicate occurs in multiple corpus splits"
                )
    return records


def selected_records(
    records: list[dict[str, object]], start: int, limit: int | None, split: str | None
) -> list[dict[str, object]]:
    if start < 0 or limit is not None and limit < 0:
        raise ManifestError("--start and --limit must be non-negative")
    filtered = [
        record for record in records
        if record["selected"] and (split is None or record["split"] == split)
    ]
    return filtered[start:] if limit is None else filtered[start:start + limit]


def _copy_response(response: BinaryIO, output: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    with output.open("wb") as stream:
        while block := response.read(CHUNK):
            stream.write(block)
            digest.update(block)
            size += len(block)
        stream.flush()
        os.fsync(stream.fileno())
    return digest.hexdigest(), size


def _obtain_archive(archive: dict[str, object], cache: Path, retries: int) -> Path | None:
    archive_cache = cache / ".archives"
    archive_cache.mkdir(parents=True, exist_ok=True)
    expected = str(archive["sha256"])
    destination = archive_cache / (expected + ".tar")
    if destination.exists() and _sha256(destination)[0] == expected:
        return destination
    part = destination.with_name(destination.name + ".part")
    for attempt in range(retries + 1):
        try:
            request = urllib.request.Request(
                str(archive["url"]),
                headers={"User-Agent": "RawTherapee-TGMR-corpus-reconstructor/1"},
            )
            with urllib.request.urlopen(request, timeout=120) as response:
                actual, _ = _copy_response(response, part)
            if actual != expected:
                part.unlink(missing_ok=True)
                return None
            os.replace(part, destination)
            return destination
        except (OSError, urllib.error.URLError, urllib.error.HTTPError):
            part.unlink(missing_ok=True)
            if attempt < retries:
                time.sleep(min(2 ** attempt, 8))
    return None


def _extract_archive_member(
    archive: dict[str, object], archive_path: Path, output: Path
) -> tuple[str, int] | None:
    member_name = str(archive["member"])
    try:
        with tarfile.open(archive_path, "r:*") as container:
            member = container.getmember(member_name)
            if not member.isfile():
                return None
            source = container.extractfile(member)
            if source is None:
                return None
            actual, size = _copy_response(source, output)
    except (KeyError, OSError, tarfile.TarError):
        output.unlink(missing_ok=True)
        return None
    if actual != archive["member_sha256"]:
        output.unlink(missing_ok=True)
        return None
    return actual, size


def fetch_record(
    record: dict[str, object], cache: Path, retries: int, offline: bool
) -> dict[str, object]:
    destination = cache / str(record["cache_filename"])
    cache_root = cache.resolve()
    resolved_destination = destination.resolve(strict=False)
    try:
        resolved_destination.relative_to(cache_root)
    except ValueError as error:
        raise ManifestError("cache_filename escapes the cache through a symlink") from error
    destination.parent.mkdir(parents=True, exist_ok=True)
    expected = str(record["sha256"])
    if destination.exists():
        actual, size = _sha256(destination)
        if actual == expected:
            return {"bytes": size, "source_id": record["source_id"], "status": "authenticated-cache"}
        if offline:
            return {
                "actual_sha256": actual, "expected_sha256": expected,
                "source_id": record["source_id"], "status": "changed-cache",
            }
    elif offline:
        return {"source_id": record["source_id"], "status": "missing-cache"}

    part = destination.with_name(destination.name + ".part")
    attempts: list[dict[str, object]] = []
    urls = [str(record["original_url"]), *(str(url) for url in record["fallback_urls"])]
    for url in urls:
        for attempt in range(retries + 1):
            try:
                request = urllib.request.Request(
                    url,
                    headers={"User-Agent": "RawTherapee-TGMR-corpus-reconstructor/1"},
                )
                with urllib.request.urlopen(request, timeout=60) as response:
                    actual, size = _copy_response(response, part)
                if actual != expected:
                    attempts.append({
                        "actual_sha256": actual, "attempt": attempt + 1,
                        "status": "checksum-mismatch", "url": url,
                    })
                    part.unlink(missing_ok=True)
                    break
                os.replace(part, destination)
                return {
                    "attempts": attempts, "bytes": size, "source_id": record["source_id"],
                    "status": "downloaded", "url": url,
                }
            except (OSError, urllib.error.URLError, urllib.error.HTTPError) as error:
                part.unlink(missing_ok=True)
                attempts.append({
                    "attempt": attempt + 1, "error": str(error),
                    "status": "unavailable", "url": url,
                })
                if attempt < retries:
                    time.sleep(min(2 ** attempt, 8))
    for archive in record.get("archive_fallbacks", []):
        archive_path = _obtain_archive(archive, cache, retries)
        if archive_path is None:
            attempts.append({"status": "archive-unavailable", "url": archive["url"]})
            continue
        extracted = _extract_archive_member(archive, archive_path, part)
        if extracted is None:
            attempts.append({
                "member": archive["member"], "status": "archive-member-mismatch",
                "url": archive["url"],
            })
            continue
        actual, size = extracted
        if actual != expected:
            part.unlink(missing_ok=True)
            attempts.append({"status": "checksum-mismatch", "url": archive["url"]})
            continue
        os.replace(part, destination)
        return {
            "attempts": attempts, "bytes": size, "member": archive["member"],
            "source_id": record["source_id"], "status": "extracted-archive",
            "url": archive["url"],
        }
    return {"attempts": attempts, "source_id": record["source_id"], "status": "unavailable-or-changed"}


def reconstruct(arguments: argparse.Namespace) -> int:
    records = read_manifest(arguments.manifest)
    chosen = selected_records(records, arguments.start, arguments.limit, arguments.split)
    arguments.cache.mkdir(parents=True, exist_ok=True)
    results = [
        fetch_record(record, arguments.cache, arguments.retry, arguments.offline_verify)
        for record in chosen
    ]
    report = {
        "format": REPORT_FORMAT,
        "manifest_sha256": _sha256(arguments.manifest)[0],
        "offline": arguments.offline_verify,
        "requested_records": len(chosen),
        "results": results,
        "status_counts": {
            status: sum(result["status"] == status for result in results)
            for status in sorted({str(result["status"]) for result in results})
        },
    }
    payload = canonical_json_bytes(report)
    if arguments.report:
        temporary = arguments.report.with_name(arguments.report.name + ".tmp")
        temporary.write_bytes(payload)
        os.replace(temporary, arguments.report)
    sys.stdout.buffer.write(payload)
    failures = sum(result["status"] not in (
        "authenticated-cache", "downloaded", "extracted-archive"
    ) for result in results)
    return 1 if failures else 0


def emit_fetch_list(arguments: argparse.Namespace) -> int:
    records = read_manifest(arguments.manifest)
    chosen = selected_records(records, arguments.start, arguments.limit, arguments.split)
    output = "source_id\tsplit\tsha256\tcache_filename\toriginal_url\tfallback_urls\tarchive_fallbacks\n"
    for record in chosen:
        output += "\t".join((
            str(record["source_id"]), str(record["split"]), str(record["sha256"]),
            str(record["cache_filename"]), str(record["original_url"]),
            " ".join(str(url) for url in record["fallback_urls"]),
            " ".join(f"{archive['url']}#{archive['member']}" for archive in record.get("archive_fallbacks", [])),
        )) + "\n"
    arguments.output.write_text(output, encoding="utf-8", newline="")
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("manifest", type=Path)
    result.add_argument("cache", type=Path, nargs="?")
    result.add_argument("--start", type=int, default=0)
    result.add_argument("--limit", type=int)
    result.add_argument("--split", choices=sorted(SPLITS))
    result.add_argument("--retry", type=int, default=2)
    result.add_argument("--offline-verify", action="store_true")
    result.add_argument("--report", type=Path)
    result.add_argument("--emit-fetch-list", dest="output", type=Path)
    return result


def main(argv: list[str] | None = None) -> int:
    arguments = parser().parse_args(argv)
    if arguments.retry < 0:
        raise SystemExit("--retry must be non-negative")
    if arguments.output:
        return emit_fetch_list(arguments)
    if arguments.cache is None:
        raise SystemExit("CACHE is required unless --emit-fetch-list is used")
    return reconstruct(arguments)


if __name__ == "__main__":
    raise SystemExit(main())
