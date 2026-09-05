#!/usr/bin/env python3
"""Prepare authenticated catalog snapshots for the TGMR production corpus.

The program deliberately uses only the Python standard library.  Network I/O
is confined to the explicit ``snapshot``, ``collect-*``, and ``fetch``
commands.  Normalization and assembly consume already frozen inputs, so the
canonical corpus selection never depends on a live API or a changing catalog.
"""

from __future__ import annotations

import argparse
import base64
import collections
import concurrent.futures
import csv
import hashlib
import html
import json
import os
from pathlib import Path
import re
import sys
import threading
import time
from typing import Callable, Iterable, Iterator
import urllib.request
import urllib.error
import urllib.parse


CANDIDATE_FORMAT = "rawtherapee-tgmr-catalog-candidate-v1"
SOURCE_FORMAT = "rawtherapee-tgmr-corpus-source-manifest-v2"
SNAPSHOT_FORMAT = "rawtherapee-tgmr-catalog-snapshot-v1"
FETCHED_FORMAT = "rawtherapee-tgmr-fetched-candidate-v1"
REVIEW_FORMAT = "rawtherapee-tgmr-source-review-v1"
OPENIMAGES_REVIEW_QUEUE_FORMAT = "rawtherapee-tgmr-openimages-people-review-queue-v1"
OPENIMAGES_PEOPLE_DECISION_FORMAT = "rawtherapee-tgmr-openimages-people-review-decision-v1"
PEOPLE_REVIEW_QUEUE_FORMAT = "rawtherapee-tgmr-people-review-queue-v1"
PEOPLE_DECISION_FORMAT = "rawtherapee-tgmr-people-review-decision-v1"
DUPLICATE_REVIEW_QUEUE_FORMAT = "rawtherapee-tgmr-duplicate-cluster-review-queue-v1"
DUPLICATE_DECISION_FORMAT = "rawtherapee-tgmr-duplicate-review-decision-v1"
HARD_DHASH_DISTANCE = 5
HARD_PHASH_DISTANCE = 8
BORDERLINE_DHASH_DISTANCE = 7
BORDERLINE_PHASH_DISTANCE = 10
ACCEPTED_LICENSES = frozenset(
    ("CC0-1.0", "PDM-1.0", "CC-BY-2.0", "CC-BY-3.0", "CC-BY-4.0")
)
CONTENT_TAGS = frozenset(
    (
        "people", "skin-hair-clothing", "foliage", "fur-feathers",
        "architecture-brick", "textile-print", "metal-specular-jewelry",
        "food", "water-sky", "low-light", "astronomy-star-field",
        "macro-specimen",
    )
)
CHUNK = 1024 * 1024
SMITHSONIAN_AWS_INDEX = (
    "https://smithsonian-open-access.s3-us-west-2.amazonaws.com/metadata/edan/index.txt"
)
COMMONS_API = "https://commons.wikimedia.org/w/api.php"
COMMONS_REQUEST_INTERVAL_SECONDS = 0.5
CORPUS_SELECTION_SEED = "rawtherapee-tgmr-corpus-v1-selection"
_commons_last_request = 0.0


class CorpusPreparationError(ValueError):
    """A catalog snapshot or normalized record violates the frozen contract."""


def canonical_json(value: object) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def canonical_pretty(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode()


def sha256_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while block := stream.read(CHUNK):
            digest.update(block)
            size += len(block)
    return digest.hexdigest(), size


def atomic_bytes(path: Path, payload: bytes, force: bool) -> None:
    if path.exists() and not force:
        raise CorpusPreparationError(f"refusing to replace output: {path}")
    temporary = path.with_name(path.name + ".tmp")
    try:
        with temporary.open("wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def write_jsonl(path: Path, records: Iterable[dict[str, object]], force: bool) -> None:
    payload = "".join(canonical_json(record) + "\n" for record in records).encode()
    atomic_bytes(path, payload, force)


def require_sha256(value: object, field: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise CorpusPreparationError(f"{field} must be lowercase SHA-256")
    return value


def require_url(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.startswith(("https://", "http://", "file://")):
        raise CorpusPreparationError(f"{field} must be an HTTP(S) or file URL")
    return value


def clean_text(value: object, fallback: str) -> str:
    if value is None:
        return fallback
    cleaned = re.sub(r"\s+", " ", html.unescape(str(value))).strip()
    return cleaned or fallback


def portable_id(value: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._:-]+", "-", value).strip("-")
    if not cleaned:
        raise CorpusPreparationError("catalog record produced an empty portable ID")
    return cleaned


def flickr_photo_id(*urls: str) -> str | None:
    for url in urls:
        match = re.search(r"/photos/[^/]+/([0-9]{5,})(?:/|$)", url)
        if not match:
            match = re.search(
                r"/([0-9]{5,})_[A-Za-z0-9]+(?:_[a-z])?\.(?:jpe?g|png)(?:[?#]|$)",
                url,
                re.IGNORECASE,
            )
        if match:
            return match.group(1)
    return None


def normalized_author_id(author_url: str, author: str, prefix: str) -> str:
    match = re.search(r"flickr\.com/(?:people|photos)/([^/?#]+)", author_url, re.IGNORECASE)
    if match:
        return portable_id("flickr-user:" + urllib.parse.unquote(match.group(1)).casefold())
    return portable_id(prefix + ":" + author.casefold())


def _candidate_suffix(record: dict[str, object], use_hint: bool = True) -> str:
    hint = str(record.get("file_type_hint") or "").casefold() if use_hint else ""
    if hint in ("jpeg", "jpg"):
        return ".jpg"
    if hint == "png":
        return ".png"
    if hint in ("tif", "tiff"):
        return ".tiff"
    parsed = urllib.parse.urlparse(str(record.get("original_url", "")))
    suffix = Path(parsed.path).suffix.lower()
    if not suffix:
        identifiers = urllib.parse.parse_qs(parsed.query).get("id", [])
        if identifiers:
            suffix = Path(identifiers[0]).suffix.lower()
    if suffix == ".jpeg":
        suffix = ".jpg"
    if suffix not in (".jpg", ".png", ".tif", ".tiff"):
        suffix = ".img"
    return suffix


def candidate_cache_filename(record: dict[str, object]) -> str:
    identity = portable_id(
        f"{clean_text(record.get('catalog'), '')}:{clean_text(record.get('upstream_source_id'), '')}"
    )
    suffix = _candidate_suffix(record)
    # A colon is legal on the Linux preparation host but not in Windows file
    # names. Keep cache names portable because they are frozen into source
    # manifest v2 and consumed by the reconstruction utility on every platform.
    stem = re.sub(r"[^A-Za-z0-9._-]+", "-", identity).strip("-._")[:160]
    identity_suffix = hashlib.sha256(identity.encode()).hexdigest()[:16]
    return f"{stem}-{identity_suffix}{suffix}"


def _legacy_candidate_cache_filename(record: dict[str, object]) -> str:
    """Return the pre-v2 portable-cache fix name for one-time local migration."""
    identity = portable_id(
        f"{clean_text(record.get('catalog'), '')}:{clean_text(record.get('upstream_source_id'), '')}"
    )
    suffix = _candidate_suffix(record, use_hint=False)
    return identity + suffix


def _unhinted_candidate_cache_filename(record: dict[str, object]) -> str:
    identity = portable_id(
        f"{clean_text(record.get('catalog'), '')}:{clean_text(record.get('upstream_source_id'), '')}"
    )
    stem = re.sub(r"[^A-Za-z0-9._-]+", "-", identity).strip("-._")[:160]
    identity_suffix = hashlib.sha256(identity.encode()).hexdigest()[:16]
    return f"{stem}-{identity_suffix}{_candidate_suffix(record, use_hint=False)}"


def safe_cache_path(cache: Path, relative: str) -> Path:
    name = Path(relative)
    if (
        name.is_absolute()
        or not name.parts
        or any(component in ("", ".", "..") for component in name.parts)
    ):
        raise CorpusPreparationError("cache filename must be a safe relative path")
    root = cache.resolve()
    destination = (cache / name).resolve(strict=False)
    try:
        destination.relative_to(root)
    except ValueError as error:
        raise CorpusPreparationError("cache filename escapes the cache") from error
    return destination


def license_id(value: object) -> str | None:
    text = str(value or "").strip().lower().rstrip("/")
    aliases = {
        "attribution license": "CC-BY-2.0",
        "cc0": "CC0-1.0",
        "public domain mark": "PDM-1.0",
        "https://creativecommons.org/publicdomain/zero/1.0": "CC0-1.0",
        "https://creativecommons.org/publicdomain/mark/1.0": "PDM-1.0",
        "https://creativecommons.org/licenses/by/2.0": "CC-BY-2.0",
        "https://creativecommons.org/licenses/by/3.0": "CC-BY-3.0",
        "https://creativecommons.org/licenses/by/4.0": "CC-BY-4.0",
        "cc-by-2.0": "CC-BY-2.0",
        "cc-by-3.0": "CC-BY-3.0",
        "cc-by-4.0": "CC-BY-4.0",
        "cc by 2.0": "CC-BY-2.0",
        "cc by 3.0": "CC-BY-3.0",
        "cc by 4.0": "CC-BY-4.0",
    }
    return aliases.get(text)


def license_url(identifier: str) -> str:
    return {
        "CC0-1.0": "https://creativecommons.org/publicdomain/zero/1.0/",
        "PDM-1.0": "https://creativecommons.org/publicdomain/mark/1.0/",
        "CC-BY-2.0": "https://creativecommons.org/licenses/by/2.0/",
        "CC-BY-3.0": "https://creativecommons.org/licenses/by/3.0/",
        "CC-BY-4.0": "https://creativecommons.org/licenses/by/4.0/",
    }[identifier]


def candidate(
    *, catalog: str, revision: str, snapshot_sha256: str,
    upstream_id: str, original_url: str, landing_page: str,
    author: str, author_id: str, author_url: str, title: str,
    license_name: str | None, advertised_checksum: str | None,
    flickr_id: str | None = None, rights_evidence_url: str | None = None,
    archive_fallbacks: list[dict[str, object]] | None = None,
    catalog_categories: list[str] | None = None,
    content_tags: list[str] | None = None,
    approve_complete_rights: bool = False,
    content_tag_rules_sha256: str | None = None,
    file_type_hint: str | None = None,
) -> dict[str, object]:
    accepted = license_name in ACCEPTED_LICENSES
    complete_rights = all((
        clean_text(author, "") and clean_text(author, "").casefold() != "unknown",
        isinstance(author_url, str) and author_url.startswith(("https://", "http://")),
        isinstance(landing_page, str) and landing_page.startswith(("https://", "http://")),
        isinstance(rights_evidence_url or landing_page, str)
        and str(rights_evidence_url or landing_page).startswith(("https://", "http://")),
    ))
    tags = sorted(set(content_tags or []))
    if any(tag not in CONTENT_TAGS for tag in tags):
        raise CorpusPreparationError("candidate contains an unknown content tag")
    output = {
        "advertised_checksum": advertised_checksum,
        "author": clean_text(author, "unknown"),
        "author_id": portable_id(author_id),
        "author_url": author_url,
        "archive_fallbacks": archive_fallbacks or [],
        "catalog": catalog,
        "catalog_categories": catalog_categories or [],
        "content_tags": tags,
        "catalog_revision": revision,
        "catalog_snapshot_sha256": snapshot_sha256,
        "format": CANDIDATE_FORMAT,
        "landing_page": landing_page,
        "license": license_name or "REJECTED-OR-UNKNOWN",
        "license_url": license_url(license_name) if accepted else rights_evidence_url or landing_page,
        "original_url": original_url,
        "rights_evidence_url": rights_evidence_url or landing_page,
        "rights_review_status": (
            "approved" if accepted and approve_complete_rights and complete_rights
            else "pending" if accepted else "rejected-license"
        ),
        "title": clean_text(title, upstream_id),
        "upstream_flickr_id": flickr_id,
        "upstream_source_id": upstream_id,
    }
    if content_tag_rules_sha256 is not None:
        output["content_tag_rules_sha256"] = require_sha256(
            content_tag_rules_sha256, "content_tag_rules_sha256"
        )
    if file_type_hint is not None:
        if file_type_hint not in ("jpeg", "png", "tiff"):
            raise CorpusPreparationError("candidate file_type_hint is invalid")
        output["file_type_hint"] = file_type_hint
    return output


def _content_tag_rules(path: Path | None) -> tuple[list[dict[str, object]], str | None]:
    if path is None:
        return [], None
    digest, _ = sha256_file(path)
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or set(value) != {"format", "rules"} or value.get(
        "format"
    ) != "rawtherapee-tgmr-catalog-content-tag-rules-v1":
        raise CorpusPreparationError("wrong catalog content-tag rules format")
    rules = value.get("rules")
    if not isinstance(rules, list):
        raise CorpusPreparationError("content-tag rules must be a list")
    for rule in rules:
        if not isinstance(rule, dict) or not set(rule).issubset({
            "patterns", "tags", "title_patterns", "title_terms",
        }) or not {"patterns", "tags"}.issubset(rule):
            raise CorpusPreparationError("malformed content-tag rule")
        patterns = rule.get("patterns")
        tags = rule.get("tags")
        if not isinstance(patterns, list) or not patterns or any(
            not isinstance(pattern, str) or not pattern.strip() for pattern in patterns
        ):
            raise CorpusPreparationError("content-tag patterns must be nonempty strings")
        if not isinstance(tags, list) or not tags or any(tag not in CONTENT_TAGS for tag in tags):
            raise CorpusPreparationError("content-tag rule contains invalid tags")
        title_patterns = rule.get("title_patterns")
        if title_patterns is not None and (
            not isinstance(title_patterns, list) or not title_patterns or any(
                not isinstance(pattern, str) or not pattern.strip()
                for pattern in title_patterns
            )
        ):
            raise CorpusPreparationError(
                "content-tag title_patterns must be nonempty strings"
            )
        title_terms = rule.get("title_terms")
        if title_terms is not None and (
            not isinstance(title_terms, list) or not title_terms or any(
                not isinstance(term, str) or not term.strip() for term in title_terms
            )
        ):
            raise CorpusPreparationError(
                "content-tag title_terms must be nonempty strings"
            )
    return rules, digest


def _title_term_matches(term: object, title: str) -> bool:
    """Match a complete word sequence, avoiding e.g. ``man`` in ``human``."""
    term_words = re.findall(r"[^\W_]+", str(term).casefold(), flags=re.UNICODE)
    title_words = re.findall(r"[^\W_]+", title.casefold(), flags=re.UNICODE)
    return bool(term_words) and any(
        title_words[index:index + len(term_words)] == term_words
        for index in range(len(title_words) - len(term_words) + 1)
    )


def _tags_for_categories(
    categories: Iterable[object], rules: list[dict[str, object]], title: str = "",
) -> list[str]:
    haystack = "\n".join(clean_text(category, "").casefold() for category in categories)
    title_haystack = clean_text(title, "").casefold()
    tags: set[str] = set()
    for rule in rules:
        category_match = any(
            str(pattern).casefold() in haystack for pattern in rule["patterns"]
        )
        title_patterns = rule.get("title_patterns")
        title_match = title_patterns is None or any(
            str(pattern).casefold() in title_haystack for pattern in title_patterns
        )
        title_term_match = any(
            _title_term_matches(term, title_haystack)
            for term in rule.get("title_terms", [])
        )
        if (category_match and title_match) or title_term_match:
            tags.update(map(str, rule["tags"]))
    return sorted(tags)


def normalize_openimages(path: Path, revision: str, digest: str) -> Iterator[dict[str, object]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        required = {"ImageID", "OriginalURL", "OriginalLandingURL", "License", "Author"}
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            raise CorpusPreparationError("Open Images CSV is missing required columns")
        for row in reader:
            identifier = clean_text(row.get("ImageID"), "")
            if not identifier:
                continue
            original = require_url(row.get("OriginalURL"), "Open Images OriginalURL")
            landing = require_url(row.get("OriginalLandingURL"), "Open Images OriginalLandingURL")
            author_url = str(row.get("AuthorProfileURL") or landing)
            author = clean_text(row.get("Author"), "unknown")
            encoded_md5 = str(row.get("OriginalMD5") or "")
            advertised = f"md5-base64:{encoded_md5}" if encoded_md5 else None
            flickr = flickr_photo_id(landing, original)
            yield candidate(
                catalog="openimages-cvdf-v5-boxable", revision=revision,
                snapshot_sha256=digest,
                upstream_id=identifier, original_url=original, landing_page=landing,
                author=author,
                author_id=normalized_author_id(author_url, author, "openimages-author"),
                author_url=author_url,
                title=clean_text(row.get("Title"), identifier),
                license_name=license_id(row.get("License")), advertised_checksum=advertised,
                flickr_id=flickr, rights_evidence_url=str(row.get("License") or landing),
            )


def normalize_pass(
    path: Path, urls_path: Path, revision: str, digest: str,
    archive_index: Path | None = None,
) -> Iterator[dict[str, object]]:
    urls_digest, _ = sha256_file(urls_path)
    with urls_path.open("r", encoding="utf-8", newline="") as stream:
        urls = [line.strip() for line in stream if line.strip()]
    archives: dict[str, list[dict[str, object]]] = {}
    if archive_index is not None:
        for value in _jsonl(archive_index):
            identity = clean_text(value.get("hash"), "")
            entry = {
                "member": clean_text(value.get("member"), ""),
                "member_sha256": require_sha256(value.get("member_sha256"), "member_sha256"),
                "sha256": require_sha256(value.get("sha256"), "archive sha256"),
                "url": require_url(value.get("url"), "archive URL"),
            }
            if Path(str(entry["member"])).is_absolute() or ".." in Path(str(entry["member"])).parts:
                raise CorpusPreparationError("PASS archive index contains an unsafe member")
            archives.setdefault(identity, []).append(entry)
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or not {"unickname", "licensename", "hash"}.issubset(reader.fieldnames):
            raise CorpusPreparationError("PASS CSV is missing required columns")
        for index, row in enumerate(reader):
            if index >= len(urls):
                raise CorpusPreparationError("PASS URL list is shorter than metadata CSV")
            identity = clean_text(row.get("hash"), "")
            # PASS calls this column ``hash``, but it is the hexadecimal
            # source/filename identity used to join pass_metadata.csv to the
            # image URL list.  It is not a checksum of the downloaded image:
            # the published catalog contains identities from 23 to 32 digits,
            # and their values do not match the image MD5.  Authenticate the
            # actual image bytes with our SHA-256 after retrieval instead.
            if not re.fullmatch(r"[0-9a-f]{23,32}", identity):
                raise CorpusPreparationError(
                    "PASS hash is not a lowercase hexadecimal source identity"
                )
            author = clean_text(row.get("unickname"), "unknown")
            original_url = require_url(urls[index], "PASS URL")
            author_url = "https://www.flickr.com/people/" + author
            yield candidate(
                catalog="pass-v3", revision=revision,
                snapshot_sha256=hashlib.sha256((digest + urls_digest).encode()).hexdigest(),
                upstream_id=identity, original_url=original_url,
                landing_page="https://robots.ox.ac.uk/~vgg/data/pass/",
                author=author,
                author_id=normalized_author_id(author_url, author, "pass-author"),
                author_url=author_url,
                title=identity, license_name="CC-BY-4.0",
                advertised_checksum=None,
                flickr_id=flickr_photo_id(original_url),
                rights_evidence_url="https://robots.ox.ac.uk/~vgg/data/pass/",
                archive_fallbacks=archives.get(identity, []),
            )


def _jsonl(path: Path) -> Iterator[dict[str, object]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        for number, line in enumerate(stream, 1):
            if not line.endswith("\n") or not line.strip():
                raise CorpusPreparationError(f"{path}:{number}: noncanonical JSONL line")
            value = json.loads(line)
            if not isinstance(value, dict):
                raise CorpusPreparationError(f"{path}:{number}: expected object")
            yield value


def normalize_commons(
    path: Path, revision: str, digest: str,
    tag_rules: list[dict[str, object]] | None = None,
    tag_rules_sha256: str | None = None,
) -> Iterator[dict[str, object]]:
    """Normalize frozen MediaWiki imageinfo results, one page per JSONL line."""
    for value in _jsonl(path):
        title = clean_text(value.get("title"), "")
        info = value.get("imageinfo")
        if not title or not isinstance(info, dict):
            raise CorpusPreparationError("Commons snapshot needs title and imageinfo object")
        metadata = info.get("extmetadata")
        if not isinstance(metadata, dict):
            raise CorpusPreparationError("Commons imageinfo lacks extmetadata")
        def meta(name: str) -> str:
            entry = metadata.get(name)
            return clean_text(entry.get("value") if isinstance(entry, dict) else entry, "")
        short = license_id(meta("LicenseShortName")) or license_id(meta("LicenseUrl"))
        page_id = str(value.get("pageid") or title)
        author = re.sub(r"<[^>]+>", "", meta("Artist")) or clean_text(info.get("user"), "unknown")
        commons_user = clean_text(info.get("userid") or info.get("user"), author)
        categories = value.get("catalog_categories", [])
        file_categories = value.get("file_categories", [])
        tags = value.get("content_tags", [])
        if not isinstance(categories, list) or not isinstance(file_categories, list) \
                or not isinstance(tags, list):
            raise CorpusPreparationError("Commons categories and content_tags must be lists")
        all_categories = sorted(set(map(str, categories)).union(
            map(str, file_categories)
        ))
        yield candidate(
            catalog="wikimedia-commons", revision=revision, snapshot_sha256=digest,
            upstream_id=page_id, original_url=require_url(info.get("url"), "Commons original URL"),
            landing_page=require_url(info.get("descriptionurl"), "Commons description URL"),
            author=author, author_id=portable_id("commons-user:" + commons_user.casefold()),
            author_url=str(info.get("userpage") or info.get("descriptionurl")),
            title=title, license_name=short,
            advertised_checksum=(
                f"sha1:{str(info.get('sha1') or '').casefold()}"
                if info.get("sha1") else None
            ),
            rights_evidence_url=str(info.get("descriptionurl")),
            catalog_categories=all_categories,
            content_tags=sorted(set(map(str, tags)).union(
                _tags_for_categories(all_categories, tag_rules or [], title)
            )),
            approve_complete_rights=True,
            content_tag_rules_sha256=tag_rules_sha256,
        )


def normalize_smithsonian(
    path: Path, revision: str, digest: str,
    tag_rules: list[dict[str, object]] | None = None,
    tag_rules_sha256: str | None = None,
) -> Iterator[dict[str, object]]:
    """Normalize frozen Smithsonian Open Access AWS/API records."""
    for value in _jsonl(path):
        identifier = clean_text(value.get("id"), "")
        media = value.get("media")
        if not identifier or not isinstance(media, dict):
            raise CorpusPreparationError("Smithsonian snapshot needs id and selected media object")
        usage = clean_text(media.get("usage"), "")
        accepted = "cc0" in usage.lower()
        landing = require_url(value.get("record_url"), "Smithsonian record URL")
        categories = value.get("catalog_categories", [])
        if not isinstance(categories, list):
            raise CorpusPreparationError("Smithsonian catalog_categories must be a list")
        yield candidate(
            catalog="smithsonian-open-access", revision=revision, snapshot_sha256=digest,
            upstream_id=identifier, original_url=require_url(media.get("url"), "Smithsonian media URL"),
            landing_page=landing, author=clean_text(value.get("author"), "Smithsonian Institution"),
            author_id=clean_text(
                value.get("author_id"), "smithsonian-record:" + identifier
            ), author_url=landing,
            title=clean_text(value.get("title"), identifier),
            license_name="CC0-1.0" if accepted else None,
            advertised_checksum=str(media.get("checksum")) if media.get("checksum") else None,
            rights_evidence_url=landing,
            catalog_categories=[
                clean_text(category, "") for category in categories
                if clean_text(category, "")
            ],
            content_tags=_tags_for_categories(
                categories, tag_rules or [], clean_text(value.get("title"), identifier)
            ),
            approve_complete_rights=True,
            content_tag_rules_sha256=tag_rules_sha256,
            file_type_hint="jpeg",
        )


def snapshot(arguments: argparse.Namespace) -> int:
    if arguments.output.exists() and not arguments.force:
        raise CorpusPreparationError(f"refusing to replace output: {arguments.output}")
    temporary = arguments.output.with_name(arguments.output.name + ".part")
    request = urllib.request.Request(
        arguments.url, headers={"User-Agent": "RawTherapee-TGMR-catalog-snapshot/1"}
    )
    digest = hashlib.sha256()
    size = 0
    try:
        with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as stream:
            while block := response.read(CHUNK):
                stream.write(block)
                digest.update(block)
                size += len(block)
            stream.flush()
            os.fsync(stream.fileno())
        actual = digest.hexdigest()
        if arguments.sha256 and actual != arguments.sha256:
            raise CorpusPreparationError("catalog snapshot SHA-256 differs from --sha256")
        os.replace(temporary, arguments.output)
    finally:
        temporary.unlink(missing_ok=True)
    report = {
        "bytes": size, "format": SNAPSHOT_FORMAT, "sha256": actual,
        "url": arguments.url,
    }
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def _read_id_list(path: Path) -> list[str]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        values = [line.strip() for line in stream if line.strip() and not line.startswith("#")]
    if len(values) != len(set(values)):
        raise CorpusPreparationError("identifier list contains duplicates")
    return values


def _catalog_dimension_eligible(width: object, height: object) -> bool | None:
    """Return a catalog-only resolution decision, or None when dimensions are absent."""
    try:
        parsed_width = int(str(width))
        parsed_height = int(str(height))
    except (TypeError, ValueError):
        return None
    if parsed_width <= 0 or parsed_height <= 0:
        return None
    return min(parsed_width, parsed_height) >= 512 \
        and parsed_width * parsed_height >= 750000


def collect_commons(arguments: argparse.Namespace) -> int:
    titles = _read_id_list(arguments.ids)
    records: list[dict[str, object]] = []
    found = 0
    rejected_dimensions = 0
    for offset in range(0, len(titles), 50):
        query = urllib.parse.urlencode({
            "action": "query", "format": "json", "formatversion": "2",
            "iiextmetadatafilter": "Artist|LicenseShortName|LicenseUrl",
            "iiprop": "url|sha1|size|timestamp|user|userid|extmetadata",
            "prop": "imageinfo", "redirects": "1",
            "titles": "|".join(titles[offset:offset + 50]),
        })
        request = urllib.request.Request(
            "https://commons.wikimedia.org/w/api.php?" + query,
            headers={"User-Agent": "RawTherapee-TGMR-catalog-snapshot/1"},
        )
        with urllib.request.urlopen(request, timeout=120) as response:
            payload = json.load(response)
        pages = payload.get("query", {}).get("pages", [])
        if not isinstance(pages, list):
            raise CorpusPreparationError("Commons API response lacks pages")
        for page in pages:
            if not isinstance(page, dict) or page.get("missing"):
                continue
            imageinfo = page.get("imageinfo")
            if not isinstance(imageinfo, list) or len(imageinfo) != 1:
                raise CorpusPreparationError("Commons page lacks one imageinfo revision")
            found += 1
            dimensions = _catalog_dimension_eligible(
                imageinfo[0].get("width"), imageinfo[0].get("height")
            )
            if dimensions is False:
                rejected_dimensions += 1
                continue
            records.append({
                "imageinfo": imageinfo[0], "pageid": page.get("pageid"),
                "title": page.get("title"),
            })
    records.sort(key=lambda value: str(value.get("title")))
    write_jsonl(arguments.output, records, arguments.force)
    sys.stdout.buffer.write(canonical_pretty({
        "format": "rawtherapee-tgmr-commons-snapshot-report-v1",
        "found": found, "records": len(records),
        "rejected_dimensions": rejected_dimensions, "requested": len(titles),
        "sha256": sha256_file(arguments.output)[0],
    }))
    return 0 if len(records) == len(titles) else 1


def _commons_api_json(parameters: dict[str, object]) -> dict[str, object]:
    """Read one MediaWiki API response with bounded maxlag retry handling."""
    global _commons_last_request
    query = dict(parameters)
    query.update({"format": "json", "formatversion": "2", "maxlag": "5"})
    url = COMMONS_API + "?" + urllib.parse.urlencode(query)
    last_error: Exception | None = None
    for attempt in range(8):
        try:
            remaining = COMMONS_REQUEST_INTERVAL_SECONDS - (
                time.monotonic() - _commons_last_request
            )
            if remaining > 0:
                time.sleep(remaining)
            request = urllib.request.Request(
                url, headers={
                    "User-Agent": (
                        "RawTherapee-TGMR-corpus/1.0 "
                        "(https://github.com/Beep6581/RawTherapee)"
                    )
                },
            )
            with urllib.request.urlopen(request, timeout=120) as response:
                payload = json.load(response)
            _commons_last_request = time.monotonic()
            if not isinstance(payload, dict):
                raise CorpusPreparationError("Commons API returned a non-object")
            api_error = payload.get("error")
            if isinstance(api_error, dict):
                code = clean_text(api_error.get("code"), "unknown")
                if code == "maxlag" and attempt < 7:
                    time.sleep(min(2 ** attempt, 8))
                    continue
                raise CorpusPreparationError(f"Commons API error: {code}")
            return payload
        except urllib.error.HTTPError as error:
            last_error = error
            _commons_last_request = time.monotonic()
            if attempt < 7 and error.code in (429, 500, 502, 503, 504):
                retry_after = error.headers.get("Retry-After")
                try:
                    delay = float(retry_after) if retry_after is not None else 2 ** attempt
                except ValueError:
                    delay = 2 ** attempt
                time.sleep(max(1.0, min(delay, 60.0)))
                continue
            break
        except (OSError, urllib.error.URLError) as error:
            last_error = error
            _commons_last_request = time.monotonic()
            if attempt < 7:
                time.sleep(min(2 ** attempt, 8))
                continue
    raise CorpusPreparationError(f"Commons API request failed: {last_error}")


def _commons_file_category_map(
    records: list[dict[str, object]],
) -> dict[str, list[str]]:
    """Retrieve file-page categories for exact frozen Commons page IDs."""
    page_ids = [clean_text(record.get("pageid"), "") for record in records]
    if any(not page_id for page_id in page_ids) or len(set(page_ids)) != len(page_ids):
        raise CorpusPreparationError("Commons snapshot has invalid or duplicate page IDs")
    result: dict[str, set[str]] = {page_id: set() for page_id in page_ids}
    seen: set[str] = set()
    for offset in range(0, len(page_ids), 50):
        batch = page_ids[offset:offset + 50]
        parameters: dict[str, object] = {
            "action": "query", "cllimit": "max", "clshow": "!hidden",
            "pageids": "|".join(batch), "prop": "categories",
        }
        while True:
            payload = _commons_api_json(parameters)
            query = payload.get("query")
            pages = query.get("pages") if isinstance(query, dict) else None
            if not isinstance(pages, list):
                raise CorpusPreparationError("Commons category response lacks pages")
            for page in pages:
                if not isinstance(page, dict):
                    raise CorpusPreparationError("Commons category page is malformed")
                page_id = clean_text(page.get("pageid"), "")
                if page_id not in result or page.get("missing"):
                    raise CorpusPreparationError(
                        "Commons category response changed a frozen page"
                    )
                seen.add(page_id)
                categories = page.get("categories", [])
                if not isinstance(categories, list):
                    raise CorpusPreparationError("Commons page categories are malformed")
                for category in categories:
                    if not isinstance(category, dict):
                        raise CorpusPreparationError("Commons page category is malformed")
                    title = clean_text(category.get("title"), "")
                    if not title.startswith("Category:"):
                        raise CorpusPreparationError("Commons returned an invalid category title")
                    result[page_id].add(title)
            continuation = payload.get("continue")
            if continuation is None:
                break
            if not isinstance(continuation, dict) or not continuation:
                raise CorpusPreparationError("Commons category continuation is malformed")
            parameters.update(continuation)
    if seen != set(page_ids):
        raise CorpusPreparationError("Commons category response omitted a frozen page")
    return {page_id: sorted(categories) for page_id, categories in result.items()}


def enrich_commons_categories(arguments: argparse.Namespace) -> int:
    """Add current file-page categories to an exact frozen Commons snapshot."""
    records = list(_jsonl(arguments.input))
    if not records:
        raise CorpusPreparationError("Commons snapshot is empty")
    category_map = _commons_file_category_map(records)
    output = []
    category_count = 0
    for record in records:
        if not isinstance(record.get("imageinfo"), dict) or not clean_text(
            record.get("title"), ""
        ):
            raise CorpusPreparationError("Commons snapshot row is malformed")
        updated = dict(record)
        categories = category_map[str(record["pageid"])]
        updated["file_categories"] = categories
        category_count += len(categories)
        output.append(updated)
    write_jsonl(arguments.output, output, arguments.force)
    report = {
        "categories": category_count,
        "format": "rawtherapee-tgmr-commons-category-enrichment-report-v1",
        "input_sha256": sha256_file(arguments.input)[0],
        "output_sha256": sha256_file(arguments.output)[0],
        "records": len(output),
    }
    if arguments.report is not None:
        atomic_bytes(arguments.report, canonical_pretty(report), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def _commons_category_members(
    category: str, max_members: int,
) -> list[dict[str, object]]:
    members: list[dict[str, object]] = []
    continuation: str | None = None
    while True:
        parameters: dict[str, object] = {
            "action": "query", "list": "categorymembers", "cmtitle": category,
            "cmtype": "file|subcat", "cmprop": "ids|title|type", "cmlimit": "500",
            "cmsort": "sortkey", "cmdir": "ascending",
        }
        if continuation is not None:
            parameters["cmcontinue"] = continuation
        payload = _commons_api_json(parameters)
        values = payload.get("query", {})
        values = values.get("categorymembers") if isinstance(values, dict) else None
        if not isinstance(values, list):
            raise CorpusPreparationError(f"Commons category response lacks members: {category}")
        for value in values:
            if not isinstance(value, dict):
                raise CorpusPreparationError("Commons category contains a non-object member")
            members.append(value)
            if len(members) >= max_members:
                return members
        next_value = payload.get("continue")
        continuation = (
            clean_text(next_value.get("cmcontinue"), "")
            if isinstance(next_value, dict) else ""
        )
        if not continuation:
            break
    return members


def _commons_recipe(path: Path) -> dict[str, object]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("format") != (
        "rawtherapee-tgmr-commons-category-recipe-v1"
    ):
        raise CorpusPreparationError("wrong Commons category recipe format")
    if set(value) != {
        "api", "candidate_author_cap", "format", "roots", "seed", "target_records"
    }:
        raise CorpusPreparationError("Commons category recipe has unknown or missing fields")
    if value.get("api") != COMMONS_API:
        raise CorpusPreparationError("Commons category recipe has an unexpected API")
    roots = value.get("roots")
    if not isinstance(roots, list) or not roots:
        raise CorpusPreparationError("Commons category recipe needs roots")
    for root in roots:
        if not isinstance(root, dict) or set(root) != {
            "category", "content_tags", "max_categories", "max_depth", "max_files",
            "max_members_per_category",
        }:
            raise CorpusPreparationError("malformed Commons category root")
        if not str(root.get("category", "")).startswith("Category:"):
            raise CorpusPreparationError("Commons root needs a Category: title")
        tags = root.get("content_tags")
        if not isinstance(tags, list) or any(tag not in CONTENT_TAGS for tag in tags):
            raise CorpusPreparationError("Commons root has invalid content tags")
        for field in (
            "max_categories", "max_depth", "max_files", "max_members_per_category",
        ):
            if not isinstance(root.get(field), int) or int(root[field]) < 0:
                raise CorpusPreparationError(f"Commons root {field} must be non-negative")
    if not isinstance(value.get("target_records"), int) or int(value["target_records"]) <= 0:
        raise CorpusPreparationError("Commons target_records must be positive")
    if not isinstance(value.get("candidate_author_cap"), int) \
            or int(value["candidate_author_cap"]) < 5:
        raise CorpusPreparationError("Commons candidate_author_cap must be at least five")
    if not clean_text(value.get("seed"), ""):
        raise CorpusPreparationError("Commons recipe needs a seed")
    return value


def collect_commons_categories(arguments: argparse.Namespace) -> int:
    """Freeze a deterministic, rights-filtered Commons metadata snapshot."""
    recipe = _commons_recipe(arguments.recipe)
    target = int(recipe["target_records"])
    if arguments.limit is not None:
        target = min(target, arguments.limit)
    seed = str(recipe["seed"])
    discovery: dict[str, dict[str, set[str]]] = {}
    root_reports = []
    for root in recipe["roots"]:  # type: ignore[union-attr]
        category = str(root["category"])
        max_depth = int(root["max_depth"])
        max_categories = int(root["max_categories"])
        max_files = int(root["max_files"])
        max_members_per_category = int(root["max_members_per_category"])
        root_tags = set(map(str, root["content_tags"]))
        queue: collections.deque[tuple[str, int]] = collections.deque([(category, 0)])
        seen_categories: set[str] = set()
        root_files: set[str] = set()
        while queue and len(seen_categories) < max_categories and len(root_files) < max_files:
            current, depth = queue.popleft()
            if current in seen_categories:
                continue
            seen_categories.add(current)
            members = _commons_category_members(current, max_members_per_category)
            subcategories = []
            for member in members:
                title = clean_text(member.get("title"), "")
                member_type = clean_text(member.get("type"), "")
                if member_type == "file" and title.startswith("File:"):
                    root_files.add(title)
                elif member_type == "subcat" and depth < max_depth \
                        and title.startswith("Category:"):
                    subcategories.append(title)
            for subcategory in sorted(set(subcategories), key=str.casefold):
                if subcategory not in seen_categories:
                    queue.append((subcategory, depth + 1))
        ordered_root_files = sorted(
            root_files,
            key=lambda title: (hashlib.sha256((seed + "\0" + category + "\0" + title).encode()).digest(), title),
        )[:max_files]
        for title in ordered_root_files:
            entry = discovery.setdefault(title, {"categories": set(), "tags": set()})
            entry["categories"].add(category)
            entry["tags"].update(root_tags)
        root_reports.append({
            "category": category, "categories_scanned": len(seen_categories),
            "files": len(ordered_root_files), "queue_remaining": len(queue),
        })

    ordered_titles = sorted(
        discovery,
        key=lambda title: (hashlib.sha256((seed + "\0" + title).encode()).digest(), title),
    )
    records: list[dict[str, object]] = []
    author_counts: collections.Counter[str] = collections.Counter()
    rejected_dimensions = 0
    rejected_license = 0
    rejected_type = 0
    rejected_author_cap = 0
    missing = 0
    for offset in range(0, len(ordered_titles), 50):
        titles = ordered_titles[offset:offset + 50]
        payload = _commons_api_json({
            "action": "query", "iiextmetadatafilter": "Artist|LicenseShortName|LicenseUrl",
            "iiprop": "url|sha1|size|mime|timestamp|user|userid|extmetadata",
            "prop": "imageinfo", "titles": "|".join(titles),
        })
        pages = payload.get("query", {})
        pages = pages.get("pages") if isinstance(pages, dict) else None
        if not isinstance(pages, list):
            raise CorpusPreparationError("Commons imageinfo response lacks pages")
        pages_by_title = {
            clean_text(page.get("title"), ""): page
            for page in pages if isinstance(page, dict)
        }
        for requested_title in titles:
            page = pages_by_title.get(requested_title)
            if page is None:
                missing += 1
                continue
            if not isinstance(page, dict) or page.get("missing"):
                missing += 1
                continue
            imageinfo = page.get("imageinfo")
            if not isinstance(imageinfo, list) or len(imageinfo) != 1:
                missing += 1
                continue
            info = imageinfo[0]
            title = clean_text(page.get("title"), "")
            if title not in discovery:
                raise CorpusPreparationError("Commons imageinfo changed a discovered title")
            if _catalog_dimension_eligible(info.get("width"), info.get("height")) is False:
                rejected_dimensions += 1
                continue
            mime = clean_text(info.get("mime"), "").casefold()
            if mime not in ("image/jpeg", "image/png", "image/tiff"):
                rejected_type += 1
                continue
            metadata = info.get("extmetadata")
            if not isinstance(metadata, dict):
                rejected_license += 1
                continue
            def meta(name: str) -> str:
                entry = metadata.get(name)
                return clean_text(entry.get("value") if isinstance(entry, dict) else entry, "")
            if license_id(meta("LicenseShortName")) not in ACCEPTED_LICENSES \
                    and license_id(meta("LicenseUrl")) not in ACCEPTED_LICENSES:
                rejected_license += 1
                continue
            author_key = clean_text(
                info.get("userid") or info.get("user") or meta("Artist"), "unknown"
            ).casefold()
            if author_counts[author_key] >= int(recipe["candidate_author_cap"]):
                rejected_author_cap += 1
                continue
            author_counts[author_key] += 1
            records.append({
                "catalog_categories": sorted(discovery[title]["categories"]),
                "content_tags": sorted(discovery[title]["tags"]),
                "imageinfo": info, "pageid": page.get("pageid"), "title": title,
            })
        # ordered_titles is already in the frozen selection order, so later
        # API pages cannot displace an accepted record once the target fills.
        if len(records) >= target:
            break
    records.sort(
        key=lambda value: (
            hashlib.sha256((seed + "\0" + str(value.get("title"))).encode()).digest(),
            str(value.get("title")),
        )
    )
    records = records[:target]
    category_map = _commons_file_category_map(records)
    for record in records:
        record["file_categories"] = category_map[str(record["pageid"])]
    records.sort(key=lambda value: str(value.get("title")).casefold())
    payload = "".join(canonical_json(record) + "\n" for record in records).encode()
    complete = len(records) == target
    if complete:
        atomic_bytes(arguments.output, payload, arguments.force)
    report = {
        "discovered_files": len(discovery), "format": (
            "rawtherapee-tgmr-commons-category-snapshot-report-v1"
        ), "missing": missing, "recipe_sha256": sha256_file(arguments.recipe)[0],
        "records": len(records), "rejected_dimensions": rejected_dimensions,
        "rejected_author_cap": rejected_author_cap,
        "rejected_license": rejected_license, "rejected_type": rejected_type,
        "published": complete, "roots": root_reports,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "target_records": target,
    }
    if arguments.report is not None:
        atomic_bytes(arguments.report, canonical_pretty(report), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0 if complete else 1


def _smithsonian_record(response: dict[str, object]) -> dict[str, object] | None:
    content = response.get("content")
    if not isinstance(content, dict):
        return None
    descriptive = content.get("descriptiveNonRepeating")
    if not isinstance(descriptive, dict):
        return None
    metadata_usage = descriptive.get("metadata_usage")
    metadata_access = (
        metadata_usage.get("access") if isinstance(metadata_usage, dict) else metadata_usage
    )
    if str(metadata_access).casefold() != "cc0":
        return None
    record_url = descriptive.get("record_link")
    if not isinstance(record_url, str) or not record_url.startswith(("https://", "http://")):
        # A stable landing page is part of the frozen rights/provenance
        # evidence.  Some otherwise usable Smithsonian records omit it; they
        # must not enter the candidate pool because normalization cannot
        # reconstruct that evidence later.
        return None
    online = descriptive.get("online_media")
    media_values = online.get("media") if isinstance(online, dict) else None
    if not isinstance(media_values, list):
        return None
    chosen = None
    for media in media_values:
        if not isinstance(media, dict):
            continue
        usage = media.get("usage")
        access = usage.get("access") if isinstance(usage, dict) else usage
        media_type = str(media.get("type") or "").lower()
        resources = media.get("resources")
        resource_values = resources if isinstance(resources, list) else []
        preferred_labels = ("High-resolution JPEG", "Screen Image")
        selected_resource = None
        for label in preferred_labels:
            candidates = (
                item for item in resource_values
                if isinstance(item, dict)
                and clean_text(item.get("label"), "") == label
                and isinstance(item.get("url"), str)
            )
            selected_resource = next(
                (item for item in candidates
                 if _catalog_dimension_eligible(item.get("width"), item.get("height"))
                 is not False),
                None,
            )
            if selected_resource is not None:
                break
        if resource_values and selected_resource is None:
            # The catalog supplied concrete renditions, but none met the
            # frozen resolution gate. Do not fetch a lower-quality fallback.
            continue
        url = selected_resource.get("url") if selected_resource else media.get("content")
        dimensions = _catalog_dimension_eligible(
            selected_resource.get("width") if selected_resource else None,
            selected_resource.get("height") if selected_resource else None,
        )
        if str(access).casefold() == "cc0" and "image" in media_type \
                and isinstance(url, str) and dimensions is not False:
            chosen = {
                "checksum": media.get("checksum"),
                "height": selected_resource.get("height") if selected_resource else None,
                "resource_label": (
                    selected_resource.get("label") if selected_resource else "Delivery Service"
                ),
                "url": url,
                "usage": "CC0",
                "width": selected_resource.get("width") if selected_resource else None,
            }
            break
    if chosen is None:
        return None
    freetext = descriptive.get("freetext")
    author = "Smithsonian Institution"
    author_id = "smithsonian-record:" + clean_text(
        descriptive.get("record_ID"), clean_text(response.get("id"), "unknown")
    )
    if isinstance(freetext, dict):
        names = freetext.get("name")
        creator_labels = (
            "architect", "artist", "author", "creator", "designer", "maker",
            "manufacturer", "photographer",
        )
        creators = []
        if isinstance(names, list):
            for name in names:
                if not isinstance(name, dict):
                    continue
                label = clean_text(name.get("label"), "").casefold()
                content_value = clean_text(name.get("content"), "")
                if content_value and any(token in label for token in creator_labels):
                    creators.append(content_value)
        if creators:
            creators = sorted(set(creators), key=str.casefold)
            author = "; ".join(creators)
            author_id = "smithsonian-creator:" + ";".join(
                creator.casefold() for creator in creators
            )
    categories = []
    indexed = content.get("indexedStructured")
    if isinstance(indexed, dict):
        for name in (
            "object_type", "online_media_type", "scientific_name", "tax_class",
            "tax_family", "tax_kingdom", "tax_order", "tax_phylum", "topic",
        ):
            values = indexed.get(name)
            if isinstance(values, list):
                categories.extend(clean_text(value, "") for value in values)
            elif values is not None:
                categories.append(clean_text(values, ""))
    return {
        "author": author,
        "author_id": author_id,
        "catalog_categories": sorted(set(filter(None, categories))),
        "id": response.get("id"),
        "media": chosen,
        "record_url": record_url,
        "title": response.get("title"),
        "unit_code": descriptive.get("unit_code"),
    }


def _read_catalog_bytes(
    url: str, *, snapshot_path: Path | None = None,
    snapshot_name: str | None = None, offline: bool = False, force: bool = False,
) -> tuple[bytes, dict[str, object]]:
    require_url(url, "remote catalog URL")
    if offline:
        if snapshot_path is None or not snapshot_path.is_file():
            raise CorpusPreparationError(f"missing offline catalog snapshot: {snapshot_path}")
        payload = snapshot_path.read_bytes()
    else:
        request = urllib.request.Request(
            url, headers={"User-Agent": "RawTherapee-TGMR-catalog-snapshot/1"},
        )
        chunks = []
        with urllib.request.urlopen(request, timeout=120) as response:
            while block := response.read(CHUNK):
                chunks.append(block)
        payload = b"".join(chunks)
        if snapshot_path is not None:
            snapshot_path.parent.mkdir(parents=True, exist_ok=True)
            atomic_bytes(snapshot_path, payload, force)
    identity: dict[str, object] = {
        "bytes": len(payload), "sha256": hashlib.sha256(payload).hexdigest(), "url": url,
    }
    if snapshot_name is not None:
        identity["snapshot_file"] = snapshot_name
    return payload, identity


def _catalog_index(
    url: str, *, snapshot_path: Path | None = None,
    snapshot_name: str | None = None, offline: bool = False, force: bool = False,
) -> tuple[list[str], dict[str, object]]:
    payload, identity = _read_catalog_bytes(
        url, snapshot_path=snapshot_path, snapshot_name=snapshot_name,
        offline=offline, force=force,
    )
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError as error:
        raise CorpusPreparationError(f"Smithsonian index is not UTF-8: {url}") from error
    values = [line.strip() for line in text.splitlines() if line.strip()]
    if not values:
        raise CorpusPreparationError(f"Smithsonian index is empty: {url}")
    if len(values) != len(set(values)):
        raise CorpusPreparationError(f"Smithsonian index contains duplicates: {url}")
    return [require_url(value, "Smithsonian index entry") for value in values], identity


def _smithsonian_unit(index_url: str) -> str:
    path = Path(urllib.parse.urlparse(index_url).path)
    if path.name != "index.txt" or path.parent.name == "edan":
        raise CorpusPreparationError(f"invalid Smithsonian unit-index URL: {index_url}")
    return path.parent.name.casefold()


def _scan_smithsonian_shard(
    url: str, visitor: Callable[[dict[str, object]], None],
    *, snapshot_path: Path | None = None, snapshot_name: str | None = None,
    offline: bool = False, force: bool = False,
) -> dict[str, object]:
    require_url(url, "Smithsonian shard URL")
    if offline:
        if snapshot_path is None or not snapshot_path.is_file():
            raise CorpusPreparationError(f"missing offline Smithsonian shard: {snapshot_path}")
        source = snapshot_path.open("rb")
    else:
        request = urllib.request.Request(
            url, headers={"User-Agent": "RawTherapee-TGMR-catalog-snapshot/1"},
        )
        source = urllib.request.urlopen(request, timeout=120)
    temporary = None
    snapshot_stream = None
    if snapshot_path is not None and not offline:
        if snapshot_path.exists() and not force:
            source.close()
            raise CorpusPreparationError(f"refusing to replace output: {snapshot_path}")
        snapshot_path.parent.mkdir(parents=True, exist_ok=True)
        temporary = snapshot_path.with_name(snapshot_path.name + ".tmp")
        snapshot_stream = temporary.open("wb")
    digest = hashlib.sha256()
    size = 0
    records = 0
    eligible = 0
    try:
        with source:
            for number, raw_line in enumerate(source, 1):
                if snapshot_stream is not None:
                    snapshot_stream.write(raw_line)
                digest.update(raw_line)
                size += len(raw_line)
                if not raw_line.strip():
                    continue
                records += 1
                try:
                    value = json.loads(raw_line)
                except (UnicodeDecodeError, json.JSONDecodeError) as error:
                    raise CorpusPreparationError(
                        f"malformed Smithsonian JSON record at {url}:{number}"
                    ) from error
                if not isinstance(value, dict):
                    raise CorpusPreparationError(
                        f"non-object Smithsonian JSON record at {url}:{number}"
                    )
                normalized = _smithsonian_record(value)
                if normalized is not None:
                    eligible += 1
                    visitor(normalized)
        if snapshot_stream is not None:
            snapshot_stream.flush()
            os.fsync(snapshot_stream.fileno())
            snapshot_stream.close()
            snapshot_stream = None
            os.replace(temporary, snapshot_path)
    finally:
        if snapshot_stream is not None:
            snapshot_stream.close()
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    identity: dict[str, object] = {
        "bytes": size, "eligible_records": eligible, "records": records,
        "sha256": digest.hexdigest(), "url": url,
    }
    if snapshot_name is not None:
        identity["snapshot_file"] = snapshot_name
    return identity


def collect_smithsonian_aws(arguments: argparse.Namespace) -> int:
    for path in (arguments.output, arguments.report):
        if path is not None and path.exists() and not arguments.force:
            raise CorpusPreparationError(f"refusing to replace output: {path}")
    requested_units = [unit.casefold() for unit in arguments.unit]
    if len(requested_units) != len(set(requested_units)):
        raise CorpusPreparationError("--unit values must be unique")
    prefixes = sorted(set(prefix.casefold() for prefix in (arguments.prefix or [])))
    if any(not re.fullmatch(r"[0-9a-f]{2}", prefix) for prefix in prefixes):
        raise CorpusPreparationError("--prefix values must be two lowercase hexadecimal digits")

    snapshot_root = arguments.snapshot_dir
    if arguments.offline_snapshot and snapshot_root is None:
        raise CorpusPreparationError("--offline-snapshot requires --snapshot-dir")
    if arguments.offline_snapshot and not snapshot_root.is_dir():
        raise CorpusPreparationError(f"offline snapshot directory is missing: {snapshot_root}")
    root_snapshot = snapshot_root / "index.txt" if snapshot_root is not None else None
    unit_index_urls, root_identity = _catalog_index(
        arguments.index_url, snapshot_path=root_snapshot,
        snapshot_name="index.txt" if snapshot_root is not None else None,
        offline=arguments.offline_snapshot, force=arguments.force,
    )
    available = {_smithsonian_unit(url): url for url in unit_index_urls}
    if len(available) != len(unit_index_urls):
        raise CorpusPreparationError("Smithsonian root index contains duplicate unit names")
    missing = sorted(set(requested_units) - available.keys())
    if missing:
        raise CorpusPreparationError(
            "Smithsonian root index lacks requested units: " + ", ".join(missing)
        )

    output_records: list[dict[str, object]] = []
    seen_ids: set[str] = set()
    unit_reports = []
    for unit in sorted(requested_units):
        unit_snapshot = snapshot_root / unit / "index.txt" if snapshot_root is not None else None
        shard_urls, unit_identity = _catalog_index(
            available[unit], snapshot_path=unit_snapshot,
            snapshot_name=f"{unit}/index.txt" if snapshot_root is not None else None,
            offline=arguments.offline_snapshot, force=arguments.force,
        )
        selected_shards = []
        for url in shard_urls:
            name = Path(urllib.parse.urlparse(url).path).name
            if not re.fullmatch(r"[0-9a-f]{2}\.txt", name.casefold()):
                raise CorpusPreparationError(f"invalid Smithsonian shard URL: {url}")
            if arguments.all_shards or name[:2].casefold() in prefixes:
                selected_shards.append(url)
        selected_shards.sort()
        if not selected_shards:
            raise CorpusPreparationError(f"no selected Smithsonian shards for unit {unit}")

        emitted = 0
        def add_record(record: dict[str, object]) -> None:
            nonlocal emitted
            if arguments.per_unit_limit is not None and emitted >= arguments.per_unit_limit:
                return
            identifier = clean_text(record.get("id"), "")
            if not identifier:
                raise CorpusPreparationError("Smithsonian record lacks an ID")
            if identifier in seen_ids:
                raise CorpusPreparationError(f"duplicate Smithsonian record ID: {identifier}")
            seen_ids.add(identifier)
            output_records.append(record)
            emitted += 1

        shard_reports = []
        for url in selected_shards:
            name = Path(urllib.parse.urlparse(url).path).name.casefold()
            shard_snapshot = snapshot_root / unit / name if snapshot_root is not None else None
            shard_reports.append(_scan_smithsonian_shard(
                url, add_record, snapshot_path=shard_snapshot,
                snapshot_name=f"{unit}/{name}" if snapshot_root is not None else None,
                offline=arguments.offline_snapshot, force=arguments.force,
            ))
        unit_reports.append({
            "emitted_records": emitted,
            "index": unit_identity,
            "shards": shard_reports,
            "unit": unit,
        })

    output_records.sort(key=lambda value: str(value.get("id")))
    write_jsonl(arguments.output, output_records, arguments.force)
    report = {
        "all_shards": arguments.all_shards,
        "format": "rawtherapee-tgmr-smithsonian-aws-snapshot-report-v1",
        "output_sha256": sha256_file(arguments.output)[0],
        "per_unit_limit": arguments.per_unit_limit,
        "prefixes": prefixes,
        "records": len(output_records),
        "root_index": root_identity,
        "units": unit_reports,
    }
    if arguments.report is not None:
        atomic_bytes(arguments.report, canonical_pretty(report), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def collect_smithsonian_api(arguments: argparse.Namespace) -> int:
    identifiers = _read_id_list(arguments.ids)
    records = []
    for identifier in identifiers:
        url = "https://api.si.edu/openaccess/api/v1.0/content/" + urllib.parse.quote(
            identifier, safe=""
        ) + "?" + urllib.parse.urlencode({"api_key": arguments.api_key})
        request = urllib.request.Request(
            url, headers={"User-Agent": "RawTherapee-TGMR-catalog-snapshot/1"}
        )
        with urllib.request.urlopen(request, timeout=120) as response:
            payload = json.load(response)
        raw = payload.get("response")
        if not isinstance(raw, dict):
            raise CorpusPreparationError(f"Smithsonian API response lacks record {identifier}")
        normalized = _smithsonian_record(raw)
        if normalized is not None:
            records.append(normalized)
    records.sort(key=lambda value: str(value.get("id")))
    write_jsonl(arguments.output, records, arguments.force)
    sys.stdout.buffer.write(canonical_pretty({
        "format": "rawtherapee-tgmr-smithsonian-snapshot-report-v1",
        "records": len(records), "requested": len(identifiers),
        "sha256": sha256_file(arguments.output)[0],
    }))
    return 0 if len(records) == len(identifiers) else 1


def normalize(arguments: argparse.Namespace) -> int:
    digest, _ = sha256_file(arguments.input)
    if arguments.snapshot_sha256 and digest != arguments.snapshot_sha256:
        raise CorpusPreparationError("input does not match --snapshot-sha256")
    tag_rules, tag_rules_sha256 = _content_tag_rules(arguments.tag_rules)
    if arguments.catalog == "openimages":
        records = normalize_openimages(arguments.input, arguments.revision, digest)
    elif arguments.catalog == "pass":
        if arguments.urls is None:
            raise CorpusPreparationError("PASS normalization requires --urls")
        records = normalize_pass(
            arguments.input, arguments.urls, arguments.revision, digest,
            arguments.archive_index,
        )
    elif arguments.catalog == "commons":
        records = normalize_commons(
            arguments.input, arguments.revision, digest, tag_rules, tag_rules_sha256
        )
    else:
        records = normalize_smithsonian(
            arguments.input, arguments.revision, digest, tag_rules, tag_rules_sha256
        )
    materialized = []
    eligible_index = 0
    for record in records:
        if arguments.eligible_only and record["license"] not in ACCEPTED_LICENSES:
            continue
        if eligible_index < arguments.start:
            eligible_index += 1
            continue
        eligible_index += 1
        materialized.append(record)
        if arguments.limit is not None and len(materialized) >= arguments.limit:
            break
    write_jsonl(arguments.output, materialized, arguments.force)
    summary = {
        "catalog": arguments.catalog, "format": "rawtherapee-tgmr-catalog-normalization-v1",
        "input_sha256": digest, "records": len(materialized), "start": arguments.start,
    }
    sys.stdout.buffer.write(canonical_pretty(summary))
    return 0


def _advertised_matches(path: Path, advertised: object) -> bool:
    if advertised is None:
        return True
    value = str(advertised)
    algorithm, separator, expected = value.partition(":")
    if not separator:
        raise CorpusPreparationError("advertised checksum lacks an algorithm")
    if algorithm == "md5":
        digest = hashlib.md5()  # noqa: S324 - verifies a catalog identity, not security.
        expected_bytes = expected.lower()
    elif algorithm == "md5-base64":
        digest = hashlib.md5()  # noqa: S324 - verifies a catalog identity, not security.
        try:
            expected_bytes = base64.b64decode(expected, validate=True).hex()
        except (ValueError, base64.binascii.Error) as error:
            raise CorpusPreparationError("malformed base64 catalog MD5") from error
    elif algorithm == "sha1":
        digest = hashlib.sha1()  # noqa: S324 - verifies a catalog identity, not security.
        expected_bytes = expected.lower()
    elif algorithm == "sha1-base36":
        if not re.fullmatch(r"[0-9a-z]+", expected.casefold()):
            raise CorpusPreparationError("malformed base-36 catalog SHA-1")
        digest = hashlib.sha1()  # noqa: S324 - verifies a catalog identity, not security.
        expected_bytes = expected.casefold().lstrip("0") or "0"
    else:
        raise CorpusPreparationError(f"unsupported advertised checksum: {algorithm}")
    with path.open("rb") as stream:
        while block := stream.read(CHUNK):
            digest.update(block)
    if algorithm == "sha1-base36":
        value = int(digest.hexdigest(), 16)
        alphabet = "0123456789abcdefghijklmnopqrstuvwxyz"
        encoded = "0"
        if value:
            digits = []
            while value:
                value, remainder = divmod(value, 36)
                digits.append(alphabet[remainder])
            encoded = "".join(reversed(digits))
        return encoded == expected_bytes
    return digest.hexdigest() == expected_bytes


def fetch(arguments: argparse.Namespace) -> int:
    if arguments.jobs <= 0:
        raise CorpusPreparationError("fetch --jobs must be positive")
    if arguments.request_delay < 0:
        raise CorpusPreparationError("fetch --request-delay must be non-negative")
    records = list(_jsonl(arguments.candidates))
    chosen = records[arguments.start:]
    if arguments.limit is not None:
        chosen = chosen[:arguments.limit]
    arguments.cache.mkdir(parents=True, exist_ok=True)
    filenames = [candidate_cache_filename(record) for record in chosen]
    if len(filenames) != len(set(filenames)):
        raise CorpusPreparationError("fetch input maps multiple records to one cache file")

    request_lock = threading.Lock()
    next_request_time = [0.0]

    def wait_for_request_slot() -> None:
        # A single process-wide schedule keeps concurrent workers from turning
        # a configured delay into one burst per thread.  Holding the lock while
        # waiting also lets a server-directed Retry-After pause every worker.
        with request_lock:
            delay = next_request_time[0] - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            next_request_time[0] = time.monotonic() + arguments.request_delay

    def defer_all_requests(delay: float) -> None:
        with request_lock:
            next_request_time[0] = max(next_request_time[0], time.monotonic() + delay)

    def fetch_one(record: dict[str, object]) -> tuple[dict[str, object] | None, dict[str, object]]:
        if record.get("format") != CANDIDATE_FORMAT:
            raise CorpusPreparationError("fetch input has the wrong candidate format")
        filename = candidate_cache_filename(record)
        destination = safe_cache_path(arguments.cache, filename)
        destination.parent.mkdir(parents=True, exist_ok=True)
        status = "authenticated-cache"
        migration_sources = (
            _unhinted_candidate_cache_filename(record),
            _legacy_candidate_cache_filename(record),
        )
        for migration_name in migration_sources:
            legacy = safe_cache_path(arguments.cache, migration_name)
            if not destination.exists() and legacy != destination and legacy.is_file() \
                    and _advertised_matches(legacy, record.get("advertised_checksum")):
                os.replace(legacy, destination)
                status = "migrated-cache"
                break
        if not destination.exists() or not _advertised_matches(
            destination, record.get("advertised_checksum")
        ):
            status = "unavailable-or-changed"
            part = destination.with_name(destination.name + ".part")
            for attempt in range(arguments.retry + 1):
                try:
                    wait_for_request_slot()
                    request = urllib.request.Request(
                        str(record["original_url"]),
                        headers={
                            "User-Agent": (
                                "RawTherapee-TGMR-corpus-bot/1 "
                                "(https://github.com/RawTherapee/RawTherapee)"
                            )
                        },
                    )
                    with urllib.request.urlopen(request, timeout=120) as response, part.open("wb") as stream:
                        while block := response.read(CHUNK):
                            stream.write(block)
                        stream.flush()
                        os.fsync(stream.fileno())
                    if not _advertised_matches(part, record.get("advertised_checksum")):
                        part.unlink(missing_ok=True)
                        status = "checksum-mismatch"
                        break
                    os.replace(part, destination)
                    status = "downloaded"
                    break
                except urllib.error.HTTPError as error:
                    part.unlink(missing_ok=True)
                    status = f"http-{error.code}"
                    if attempt < arguments.retry and error.code in (429, 500, 502, 503, 504):
                        retry_after = error.headers.get("Retry-After")
                        try:
                            delay = (
                                float(retry_after)
                                if retry_after is not None else float(2 ** attempt)
                            )
                        except ValueError:
                            delay = float(2 ** attempt)
                        # Wikimedia currently asks bulk clients to pause for
                        # ten minutes when its upload frontend is saturated.
                        # Honor such server-directed pacing instead of turning
                        # a retry loop into a burst of guaranteed failures.
                        delay = max(1.0, min(delay, 900.0))
                        defer_all_requests(delay)
                        continue
                    break
                except (OSError, urllib.error.URLError):
                    part.unlink(missing_ok=True)
                    status = "unavailable-or-changed"
                    if attempt < arguments.retry:
                        time.sleep(min(2 ** attempt, 8))
                        continue
                    break
            part.unlink(missing_ok=True)
        if status not in ("authenticated-cache", "downloaded", "migrated-cache"):
            return None, {
                "source_id": portable_id(
                    f"{record.get('catalog', '')}:{record.get('upstream_source_id', '')}"
                ),
                "status": status,
                "url": record["original_url"],
            }
        sha256, size = sha256_file(destination)
        fetched = dict(record)
        fetched.update({
            "bytes": size, "cache_filename": filename, "format": FETCHED_FORMAT,
            "sha256": sha256,
        })
        return fetched, {
            "bytes": size, "sha256": sha256,
            "source_id": portable_id(
                f"{record.get('catalog', '')}:{record.get('upstream_source_id', '')}"
            ),
            "status": status,
        }

    output: list[dict[str, object]] = []
    results: list[dict[str, object]] = []
    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=arguments.jobs) as executor:
        for fetched, result in executor.map(fetch_one, chosen):
            results.append(result)
            if fetched is None:
                failures += 1
            else:
                output.append(fetched)
    write_jsonl(arguments.output, output, arguments.force)
    report = {
        "failures": failures, "format": "rawtherapee-tgmr-candidate-fetch-report-v1",
        "fetched": len(output), "request_delay_seconds": arguments.request_delay,
        "requested": len(chosen), "results": results,
    }
    if arguments.report:
        atomic_bytes(arguments.report, canonical_pretty(report), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(report))
    return 1 if failures else 0


def read_reviews(path: Path | None) -> dict[str, dict[str, object]]:
    if path is None:
        return {}
    output: dict[str, dict[str, object]] = {}
    for value in _jsonl(path):
        if value.get("format") != REVIEW_FORMAT:
            raise CorpusPreparationError("wrong review record format")
        source_id = clean_text(value.get("source_id"), "")
        if source_id in output:
            raise CorpusPreparationError(f"duplicate review for {source_id}")
        tags = value.get("content_tags")
        if not isinstance(tags, list) or any(tag not in CONTENT_TAGS for tag in tags):
            raise CorpusPreparationError(f"invalid content tags for {source_id}")
        if value.get("rights_review_status") not in ("approved", "rejected", "pending"):
            raise CorpusPreparationError(f"invalid rights review for {source_id}")
        if value.get("people_review_status") not in (
            "not-applicable", "approved-no-minors-or-sensitive-content", "rejected", "pending"
        ):
            raise CorpusPreparationError(f"invalid people review for {source_id}")
        if value.get("normalized_author_id") is not None:
            portable_id(clean_text(value.get("normalized_author_id"), ""))
        output[source_id] = value
    return output


def assemble(arguments: argparse.Namespace) -> int:
    if arguments.jobs <= 0:
        raise CorpusPreparationError("assemble --jobs must be positive")
    classifications: dict[str, dict[str, object]] = {}
    for value in _jsonl(arguments.classifications):
        source_id = clean_text(value.get("source_id"), "")
        if source_id in classifications:
            raise CorpusPreparationError(f"duplicate classification for {source_id}")
        classifications[source_id] = value
    reviews = read_reviews(arguments.reviews)
    candidate_values = list(_jsonl(arguments.candidates))
    source_paths: dict[str, Path] = {}
    for candidate_value in candidate_values:
        candidate_format = candidate_value.get("format")
        if candidate_format not in (FETCHED_FORMAT, CANDIDATE_FORMAT):
            raise CorpusPreparationError(
                "assemble requires fetched or local catalog candidate records"
            )
        catalog = clean_text(candidate_value.get("catalog"), "")
        upstream = clean_text(candidate_value.get("upstream_source_id"), "")
        source_id = portable_id(f"{catalog}:{upstream}")
        classification_record = classifications.get(source_id)
        if classification_record is None:
            continue
        cache_filename = clean_text(classification_record.get("cache_filename"), "")
        if not cache_filename:
            cache_filename = clean_text(candidate_value.get("cache_filename"), "")
        source_paths[source_id] = safe_cache_path(arguments.cache, cache_filename)

    def authenticate(item: tuple[str, Path]) -> tuple[str, str]:
        source_id, source_path = item
        if not source_path.is_file():
            raise CorpusPreparationError(f"classified cache file is missing: {source_path}")
        return source_id, sha256_file(source_path)[0]

    source_sha256s: dict[str, str] = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=arguments.jobs) as executor:
        for source_id, digest in executor.map(authenticate, source_paths.items()):
            source_sha256s[source_id] = digest

    output: list[dict[str, object]] = []
    for candidate_value in candidate_values:
        candidate_format = candidate_value.get("format")
        if candidate_format not in (FETCHED_FORMAT, CANDIDATE_FORMAT):
            raise CorpusPreparationError(
                "assemble requires fetched or local catalog candidate records"
            )
        catalog = clean_text(candidate_value.get("catalog"), "")
        upstream = clean_text(candidate_value.get("upstream_source_id"), "")
        source_id = portable_id(f"{catalog}:{upstream}")
        classification_record = classifications.get(source_id)
        if classification_record is None:
            continue
        if classification_record.get("format") == (
            "rawtherapee-tgmr-image-proxy-classification-v1"
        ):
            raise CorpusPreparationError(
                f"proxy classification cannot be assembled as final metadata: {source_id}"
            )
        cache_filename = clean_text(classification_record.get("cache_filename"), "")
        if not cache_filename:
            cache_filename = clean_text(candidate_value.get("cache_filename"), "")
        source_sha256 = source_sha256s[source_id]
        classified_sha256 = clean_text(classification_record.get("source_sha256"), "")
        if classified_sha256 and require_sha256(
            classified_sha256, "classification source_sha256"
        ) != source_sha256:
            raise CorpusPreparationError(
                f"classified source bytes changed before assembly: {source_id}"
            )
        if candidate_format == FETCHED_FORMAT:
            if require_sha256(candidate_value.get("sha256"), "fetched sha256") != source_sha256:
                raise CorpusPreparationError(
                    f"fetched source bytes changed before assembly: {source_id}"
                )
        elif not classified_sha256:
            raise CorpusPreparationError(
                f"local catalog classification lacks source_sha256: {source_id}"
            )
        local_openimages = (
            candidate_format == CANDIDATE_FORMAT
            and catalog == "openimages-cvdf-v5-boxable"
        )
        if local_openimages:
            parts = Path(cache_filename).parts
            expected_name = upstream + ".jpg"
            if (
                len(parts) != 5
                or parts[0] not in ("train", "validation", "test")
                or parts[1:4] != (upstream[0], upstream[1], upstream[2])
                or parts[4] != expected_name
            ):
                raise CorpusPreparationError(
                    f"local Open Images cache layout is malformed: {source_id}"
                )
            reconstruction_url = (
                "https://open-images-dataset.s3.amazonaws.com/"
                f"{parts[0]}/{expected_name}"
            )
        else:
            reconstruction_url = candidate_value["original_url"]
        review = reviews.get(source_id, {})
        rights_status = str(review.get("rights_review_status") or candidate_value["rights_review_status"])
        tags = sorted(set(
            review["content_tags"] if "content_tags" in review
            else candidate_value.get("content_tags", [])
        ))
        people_status = str(review.get("people_review_status") or (
            "pending" if "people" in tags else "not-applicable"
        ))
        classification = classification_record.get("classification")
        if not isinstance(classification, dict):
            raise CorpusPreparationError(f"classification object missing for {source_id}")
        output.append({
            # Open Images metadata describes the Flickr original, whereas the
            # CVDF archive is a <=1024-pixel mirror rendition.  Its exact
            # SHA-256 is the authoritative source identity.
            "advertised_checksum": (
                None if local_openimages else candidate_value.get("advertised_checksum")
            ),
            "archive_fallbacks": candidate_value.get("archive_fallbacks", []),
            "author": candidate_value["author"],
            "author_id": portable_id(clean_text(
                review.get("normalized_author_id"), str(candidate_value["author_id"])
            )),
            "author_url": candidate_value["author_url"],
            "cache_filename": cache_filename,
            "catalog": {
                "name": catalog,
                "revision": candidate_value["catalog_revision"],
                "snapshot_sha256": candidate_value["catalog_snapshot_sha256"],
            },
            "classification": classification,
            "content_tags": tags,
            "decoded_pixel_sha256": classification_record["decoded_pixel_sha256"],
            "fallback_urls": [],
            "file_type": classification_record["file_type"],
            "format": SOURCE_FORMAT,
            "height": classification_record["height"],
            "icc_identity": classification_record["icc_identity"],
            "landing_page": candidate_value["landing_page"],
            "license": candidate_value["license"],
            "license_url": candidate_value["license_url"],
            "orientation": classification_record["orientation"],
            "original_url": reconstruction_url,
            "patch_coordinates": [],
            "patch_sampling_seed": "0x" + hashlib.sha256(source_id.encode()).hexdigest()[:16],
            "people_review_status": people_status,
            "rights": {
                "evidence_revision": str(review.get("rights_evidence_revision") or candidate_value["catalog_revision"]),
                "evidence_sha256": str(review.get("rights_evidence_sha256") or candidate_value["catalog_snapshot_sha256"]),
                "evidence_url": str(review.get("rights_evidence_url") or candidate_value["rights_evidence_url"]),
                "review_status": rights_status,
            },
            "selected": False,
            "selection_status": "candidate-reviewed" if rights_status == "approved" else "candidate-pending-review",
            "sha256": source_sha256,
            "source_id": source_id,
            "split": "unassigned",
            "title": candidate_value["title"],
            "upstream_flickr_id": candidate_value.get("upstream_flickr_id"),
            "upstream_source_id": upstream,
            "width": classification_record["width"],
        })
    write_jsonl(arguments.output, output, arguments.force)
    sys.stdout.buffer.write(canonical_pretty({
        "format": "rawtherapee-tgmr-source-assembly-v1", "records": len(output),
    }))
    return 0


def _assigned_split(seed: str, author_id: str) -> str:
    value = hashlib.sha256((seed + "\0" + author_id).encode()).digest()
    bucket = int.from_bytes(value[:8], "big") % 20
    return "train" if bucket < 16 else "validation" if bucket < 18 else "test"


def _stable_order(seed: str, identity: str) -> tuple[int, str]:
    value = hashlib.sha256((seed + "\0" + identity).encode()).digest()
    return int.from_bytes(value[:8], "big"), identity


def _hamming_hex(left: str, right: str) -> int:
    return (int(left, 16) ^ int(right, 16)).bit_count()


def _openimages_tag_map(
    descriptions_path: Path, rules_path: Path,
) -> tuple[dict[str, str], dict[str, set[str]], dict[str, object]]:
    rules = json.loads(rules_path.read_text(encoding="utf-8"))
    if not isinstance(rules, dict) or rules.get("format") != (
        "rawtherapee-tgmr-openimages-content-tags-v1"
    ):
        raise CorpusPreparationError("wrong Open Images content-tag rules format")
    label_rules = rules.get("label_names")
    derived_rules = rules.get("derived_rules")
    if not isinstance(label_rules, dict) or not isinstance(derived_rules, dict):
        raise CorpusPreparationError("Open Images content-tag rules are incomplete")
    if set(label_rules) - CONTENT_TAGS:
        raise CorpusPreparationError("Open Images rules contain an unknown content tag")
    by_name: dict[str, str] = {}
    by_mid: dict[str, str] = {}
    with descriptions_path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ["LabelName", "DisplayName"]:
            raise CorpusPreparationError("Open Images class descriptions have the wrong header")
        for row in reader:
            mid = clean_text(row.get("LabelName"), "")
            name = clean_text(row.get("DisplayName"), "")
            if not mid or not name or mid in by_mid or name in by_name:
                raise CorpusPreparationError("Open Images class descriptions are malformed")
            by_mid[mid] = name
            by_name[name] = mid
    tags_by_mid: dict[str, set[str]] = collections.defaultdict(set)
    for tag, names in label_rules.items():
        if not isinstance(names, list) or not names or any(
            not isinstance(name, str) for name in names
        ):
            raise CorpusPreparationError(f"Open Images tag rule {tag} is malformed")
        for name in names:
            if name not in by_name:
                raise CorpusPreparationError(
                    f"Open Images tag rule names an unknown class: {name}"
                )
            tags_by_mid[by_name[name]].add(tag)
    return by_mid, tags_by_mid, derived_rules


def _scan_openimages_annotations(
    path: Path, candidate_ids: set[str], labels: dict[str, set[str]],
    provenance: dict[str, set[str]], kind: str,
) -> tuple[int, int]:
    matched_rows = 0
    positive_rows = 0
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        required = {"ImageID", "LabelName"}
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            raise CorpusPreparationError(f"Open Images {kind} annotations have the wrong header")
        human = "Confidence" in reader.fieldnames
        for row in reader:
            identity = str(row.get("ImageID") or "")
            if identity not in candidate_ids:
                continue
            matched_rows += 1
            if human:
                try:
                    positive = float(str(row.get("Confidence") or "nan")) == 1.0
                except ValueError:
                    positive = False
                if not positive:
                    continue
            mid = clean_text(row.get("LabelName"), "")
            if not mid:
                raise CorpusPreparationError(f"Open Images {kind} annotation lacks a label")
            labels[identity].add(mid)
            provenance[identity].add(kind)
            positive_rows += 1
    return matched_rows, positive_rows


def _openimages_review_html(
    queue: list[dict[str, object]], cache_root: Path,
) -> bytes:
    cards: list[str] = []
    for entry in queue:
        source_id = str(entry["source_id"])
        path = safe_cache_path(cache_root, str(entry["cache_filename"])).absolute()
        tags = ", ".join(str(value) for value in entry["content_tags"])
        labels = ", ".join(str(value) for value in entry["positive_labels"])
        cards.append(
            '<article class="card" data-source-id="' + html.escape(source_id, quote=True)
            + '"><img loading="lazy" src="' + html.escape(path.as_uri(), quote=True)
            + '" alt=""><h2>' + html.escape(source_id) + '</h2><p><b>'
            + html.escape(str(entry["split"])) + '</b> · '
            + html.escape(str(entry["author"])) + '</p><p>'
            + html.escape(str(entry["title"])) + '</p><p>Tags: '
            + html.escape(tags) + '</p><p>Positive labels: '
            + html.escape(labels) + '</p><p><a href="'
            + html.escape(str(entry["landing_page"]), quote=True)
            + '">source and rights</a></p><label><input class="reject" '
            + 'type="checkbox"> reject: obvious minor or sensitive content'
            + '</label></article>'
        )
    body = "".join(cards)
    document = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>TGMR people review</title>
<style>body{font-family:sans-serif;margin:1rem}header{position:sticky;top:0;background:#fff;padding:.5rem;z-index:2}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:1rem}.card{border:1px solid #999;padding:.6rem}.card.rejected{border:3px solid #a00;background:#fee}.card img{width:100%;height:240px;object-fit:contain;background:#222}.card h2{font-size:.85rem;overflow-wrap:anywhere}.card p{font-size:.8rem}</style></head>
<body><header><strong>Unchecked images are approved.</strong> Tick only images
that must be rejected for an obvious minor or sensitive content.
<button id="export">Export all decisions</button>
<span id="count"></span><span id="exportStatus"></span>
<textarea id="exportText" hidden aria-label="Exported review decisions"></textarea>
</header><main class="grid">""" + body + """</main>
<script>'use strict';
function update(){const inputs=document.querySelectorAll('.reject');for(const input of inputs){input.toggleAttribute('checked',input.checked);input.closest('.card').classList.toggle('rejected',input.checked);}const rejected=document.querySelectorAll('.card.rejected').length;document.getElementById('count').textContent=rejected+' rejected; '+(inputs.length-rejected)+' approved';}
document.addEventListener('change',update);update();
function decisions(){const lines=[];for(const card of document.querySelectorAll('.card')){const rejected=card.querySelector('.reject:checked')!==null;lines.push(JSON.stringify({format:'rawtherapee-tgmr-openimages-people-review-decision-v1',people_review_status:rejected?'rejected':'approved-no-minors-or-sensitive-content',source_id:card.dataset.sourceId}));}return lines.join('\\n')+'\\n';}
document.getElementById('export').addEventListener('click',async()=>{const text=decisions();const area=document.getElementById('exportText');const status=document.getElementById('exportStatus');area.hidden=false;area.value=text;area.focus();area.select();let copied=false;try{await navigator.clipboard.writeText(text);copied=true;}catch(error){}const blob=new Blob([text],{type:'application/x-ndjson'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='openimages-people-decisions.jsonl';a.hidden=true;document.body.appendChild(a);a.click();status.textContent=' Prepared '+document.querySelectorAll('.card').length+' decisions; '+(copied?'copied to clipboard and ':'')+'download requested. If no file appears, the JSONL is selected below: press Ctrl+C and save it.';setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove();},30000);});
</script></body></html>
"""
    return document.encode("utf-8")


def prepare_openimages_review(arguments: argparse.Namespace) -> int:
    records = list(_jsonl(arguments.manifest))
    if not records:
        raise CorpusPreparationError("Open Images review input is empty")
    candidate_ids: set[str] = set()
    for record in records:
        catalog = record.get("catalog")
        if record.get("format") != SOURCE_FORMAT or not isinstance(catalog, dict) \
                or catalog.get("name") != "openimages-cvdf-v5-boxable":
            raise CorpusPreparationError("Open Images review input contains a foreign record")
        upstream = clean_text(record.get("upstream_source_id"), "")
        if not upstream or upstream in candidate_ids:
            raise CorpusPreparationError("Open Images review input has duplicate identities")
        candidate_ids.add(upstream)

    expected = {
        "class descriptions": arguments.class_descriptions_sha256,
        "human labels": arguments.human_labels_sha256,
        "boxes": arguments.boxes_sha256,
        "rules": arguments.rules_sha256,
    }
    paths = {
        "class descriptions": arguments.class_descriptions,
        "human labels": arguments.human_labels,
        "boxes": arguments.boxes,
        "rules": arguments.rules,
    }
    identities: dict[str, dict[str, object]] = {}
    for name, path in paths.items():
        digest, size = sha256_file(path)
        required = expected[name]
        if required and digest != required:
            raise CorpusPreparationError(f"Open Images {name} SHA-256 differs")
        identities[name] = {"bytes": size, "sha256": digest}

    class_names, tags_by_mid, derived = _openimages_tag_map(
        arguments.class_descriptions, arguments.rules
    )
    labels: dict[str, set[str]] = collections.defaultdict(set)
    provenance: dict[str, set[str]] = collections.defaultdict(set)
    human_rows, human_positive = _scan_openimages_annotations(
        arguments.human_labels, candidate_ids, labels, provenance, "human-image-label"
    )
    box_rows, box_positive = _scan_openimages_annotations(
        arguments.boxes, candidate_ids, labels, provenance, "object-box"
    )
    unknown_labels = sorted({mid for values in labels.values() for mid in values} - class_names.keys())
    if unknown_labels:
        raise CorpusPreparationError(
            "Open Images annotations contain labels absent from class descriptions: "
            + ", ".join(unknown_labels[:8])
        )

    low_light = derived.get("low-light")
    if not isinstance(low_light, dict) or not isinstance(
        low_light.get("maximum_linear_luminance_mean"), (int, float)
    ):
        raise CorpusPreparationError("Open Images low-light rule is malformed")
    low_light_max = float(low_light["maximum_linear_luminance_mean"])

    reviews: list[dict[str, object]] = []
    tagged: dict[str, list[str]] = {}
    rights_approved: set[str] = set()
    rights_counts: collections.Counter[str] = collections.Counter()
    for record in records:
        upstream = str(record["upstream_source_id"])
        tags = {tag for mid in labels[upstream] for tag in tags_by_mid.get(mid, ())}
        classification = record.get("classification")
        if not isinstance(classification, dict):
            raise CorpusPreparationError("Open Images review input lacks classification")
        if float(classification.get("luminance_mean", 1.0)) <= low_light_max:
            tags.add("low-light")
        tags_list = sorted(tags)
        tagged[upstream] = tags_list
        catalog = record["catalog"]
        complete = (
            record.get("license") in ACCEPTED_LICENSES
            and clean_text(record.get("author"), "").casefold() != "unknown"
            and bool(clean_text(record.get("author_id"), ""))
            and all(str(record.get(field, "")).startswith(("https://", "http://"))
                    for field in ("author_url", "landing_page", "license_url"))
            and isinstance(catalog, dict)
            and bool(clean_text(catalog.get("revision"), ""))
            and bool(re.fullmatch(r"[0-9a-f]{64}", str(catalog.get("snapshot_sha256", ""))))
        )
        rights_status = "approved" if complete else "pending"
        if complete:
            rights_approved.add(str(record["source_id"]))
        rights_counts[rights_status] += 1
        reviews.append({
            "content_tags": tags_list,
            "format": REVIEW_FORMAT,
            "people_review_status": "pending" if "people" in tags else "not-applicable",
            "rights_evidence_revision": catalog["revision"],
            "rights_evidence_sha256": catalog["snapshot_sha256"],
            "rights_evidence_url": record["license_url"],
            "rights_review_status": rights_status,
            "source_id": record["source_id"],
        })

    quality_eligible = [record for record in records if (
        min(int(record["width"]), int(record["height"])) >= 512
        and int(record["width"]) * int(record["height"]) >= 750000
        and str(record["source_id"]) in rights_approved
    )]
    seed = CORPUS_SELECTION_SEED
    split_quotas = {"train": 3200, "validation": 400, "test": 400}
    split_surplus_targets = {
        split: (quota * 120 + 99) // 100 for split, quota in split_quotas.items()
    }
    author_counts: collections.Counter[tuple[str, str]] = collections.Counter()
    split_capacity: collections.Counter[str] = collections.Counter()
    deduplicated_capacity: collections.Counter[str] = collections.Counter()
    duplicate_rejections: collections.Counter[str] = collections.Counter()
    bytes_seen: set[str] = set()
    pixels_seen: set[str] = set()
    flickr_seen: set[str] = set()
    signatures: list[tuple[str, str]] = []
    for record in sorted(
        quality_eligible,
        key=lambda value: _stable_order(seed, str(value["source_id"])),
    ):
        split = _assigned_split(seed, str(record["author_id"]))
        author_key = split, str(record["author_id"])
        if author_counts[author_key] >= 5:
            continue
        author_counts[author_key] += 1
        split_capacity[split] += 1
        classification = record["classification"]
        assert isinstance(classification, dict)
        source_bytes = str(record["sha256"])
        source_pixels = str(record["decoded_pixel_sha256"])
        flickr = str(record.get("upstream_flickr_id") or "")
        dhash = str(classification["dhash"])
        phash = str(classification["phash"])
        if source_bytes in bytes_seen:
            duplicate_rejections["compressed-sha256"] += 1
            continue
        if source_pixels in pixels_seen:
            duplicate_rejections["decoded-sha256"] += 1
            continue
        if flickr and flickr in flickr_seen:
            duplicate_rejections["flickr-photo-id"] += 1
            continue
        duplicate_kind = next((
            "dhash" if _hamming_hex(dhash, old_d) <= 5 else "phash"
            for old_d, old_p in signatures
            if _hamming_hex(dhash, old_d) <= 5 or _hamming_hex(phash, old_p) <= 8
        ), None)
        if duplicate_kind is not None:
            duplicate_rejections[duplicate_kind] += 1
            continue
        deduplicated_capacity[split] += 1
        bytes_seen.add(source_bytes)
        pixels_seen.add(source_pixels)
        if flickr:
            flickr_seen.add(flickr)
        signatures.append((dhash, phash))

    eligible = [record for record in quality_eligible
                if "people" in tagged[str(record["upstream_source_id"])]]
    targets = {"train": 750, "validation": 94, "test": 94}
    available: collections.Counter[str] = collections.Counter()
    pools: dict[str, list[dict[str, object]]] = {name: [] for name in targets}
    for record in eligible:
        split = _assigned_split(seed, str(record["author_id"]))
        available[split] += 1
        pools[split].append(record)
    ordered_people: list[tuple[str, dict[str, object]]] = []
    for split, pool in pools.items():
        for record in pool:
            ordered_people.append((split, record))
    ordered_people.sort(key=lambda item: _stable_order(seed, str(item[1]["source_id"])))
    queue: list[dict[str, object]] = []
    queued: collections.Counter[str] = collections.Counter()
    authors: collections.Counter[str] = collections.Counter()
    bytes_seen = set()
    pixels_seen = set()
    flickr_seen = set()
    signatures = []
    for split, record in ordered_people:
        if queued[split] >= targets[split]:
            continue
        author = str(record["author_id"])
        flickr = str(record.get("upstream_flickr_id") or "")
        classification = record["classification"]
        assert isinstance(classification, dict)
        dhash = str(classification["dhash"])
        phash = str(classification["phash"])
        if authors[author] >= 5 or str(record["sha256"]) in bytes_seen \
                or str(record["decoded_pixel_sha256"]) in pixels_seen \
                or (flickr and flickr in flickr_seen) \
                or any(_hamming_hex(dhash, old_d) <= 5
                       or _hamming_hex(phash, old_p) <= 8
                       for old_d, old_p in signatures):
            continue
        upstream = str(record["upstream_source_id"])
        queue.append({
            "annotation_sources": sorted(provenance[upstream]),
            "author": record["author"],
            "cache_filename": record["cache_filename"],
            "content_tags": tagged[upstream],
            "format": OPENIMAGES_REVIEW_QUEUE_FORMAT,
            "landing_page": record["landing_page"],
            "positive_labels": sorted(class_names[mid] for mid in labels[upstream]),
            "source_id": record["source_id"],
            "split": split,
            "title": record["title"],
        })
        queued[split] += 1
        authors[author] += 1
        bytes_seen.add(str(record["sha256"]))
        pixels_seen.add(str(record["decoded_pixel_sha256"]))
        if flickr:
            flickr_seen.add(flickr)
        signatures.append((dhash, phash))

    queue.sort(key=lambda entry: (
        ("train", "validation", "test").index(str(entry["split"])),
        _stable_order(seed, str(entry["source_id"])),
    ))
    write_jsonl(arguments.reviews, reviews, arguments.force)
    write_jsonl(arguments.people_queue, queue, arguments.force)
    if arguments.html:
        atomic_bytes(arguments.html, _openimages_review_html(queue, arguments.cache), arguments.force)
    expansion_required = {
        split: deduplicated_capacity[split] < split_surplus_targets[split]
        for split in split_quotas
    }
    report = {
        "annotation_rows": {
            "boxes_matched": box_rows, "boxes_positive": box_positive,
            "human_matched": human_rows, "human_positive": human_positive,
        },
        "annotation_snapshots": identities,
        "auto_rights": dict(sorted(rights_counts.items())),
        "candidate_manifest": {
            "records": len(records), "sha256": sha256_file(arguments.manifest)[0],
        },
        "format": "rawtherapee-tgmr-openimages-review-preparation-report-v1",
        "people_candidates_by_split": dict(sorted(available.items())),
        "people_queue_by_split": dict(sorted(queued.items())),
        "people_queue_sha256": sha256_file(arguments.people_queue)[0],
        "people_queue_targets": targets,
        "preselection_duplicate_rejections": dict(sorted(duplicate_rejections.items())),
        "quality_author_capacity_by_split": {
            split: split_capacity[split] for split in split_quotas
        },
        "quality_author_deduplicated_capacity_by_split": {
            split: deduplicated_capacity[split] for split in split_quotas
        },
        "reviews_sha256": sha256_file(arguments.reviews)[0],
        "split_quotas": split_quotas,
        "split_surplus_20_percent_targets": split_surplus_targets,
        "split_surplus_expansion_required": expansion_required,
        "suggested_next_candidate_batch": {
            "count": 2000,
            "start": arguments.candidate_pool_size,
        } if any(expansion_required.values()) else None,
        "tagged_candidates": sum(bool(values) for values in tagged.values()),
    }
    atomic_bytes(arguments.report, canonical_pretty(report), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(report))
    if any(queued[split] < target for split, target in targets.items()):
        return 1
    return 0


def apply_openimages_people_decisions(arguments: argparse.Namespace) -> int:
    reviews = list(_jsonl(arguments.reviews))
    queue = list(_jsonl(arguments.people_queue))
    queued_ids = {str(record.get("source_id") or "") for record in queue}
    if len(queued_ids) != len(queue) or any(
        record.get("format") != OPENIMAGES_REVIEW_QUEUE_FORMAT for record in queue
    ):
        raise CorpusPreparationError("Open Images people queue is malformed")
    decisions: dict[str, str] = {}
    for record in _jsonl(arguments.decisions):
        if record.get("format") != OPENIMAGES_PEOPLE_DECISION_FORMAT:
            raise CorpusPreparationError("wrong Open Images people-decision format")
        source_id = clean_text(record.get("source_id"), "")
        status = record.get("people_review_status")
        if source_id not in queued_ids or source_id in decisions:
            raise CorpusPreparationError("people decision has an unknown or duplicate source")
        if status not in ("approved-no-minors-or-sensitive-content", "rejected"):
            raise CorpusPreparationError("people decision has an invalid status")
        decisions[source_id] = str(status)
    if arguments.require_complete and decisions.keys() != queued_ids:
        raise CorpusPreparationError("people review decisions are incomplete")
    output = []
    for record in reviews:
        if record.get("format") != REVIEW_FORMAT:
            raise CorpusPreparationError("base source review is malformed")
        source_id = str(record.get("source_id") or "")
        updated = dict(record)
        if source_id in decisions:
            updated["people_review_status"] = decisions[source_id]
        output.append(updated)
    write_jsonl(arguments.output, output, arguments.force)
    report = {
        "approved": sum(status == "approved-no-minors-or-sensitive-content"
                        for status in decisions.values()),
        "decisions": len(decisions),
        "format": "rawtherapee-tgmr-openimages-people-review-application-v1",
        "output_sha256": sha256_file(arguments.output)[0],
        "rejected": sum(status == "rejected" for status in decisions.values()),
    }
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def _catalog_people_review_html(
    queue: list[dict[str, object]], cache_root: Path,
) -> bytes:
    cards = []
    for entry in queue:
        source_id = str(entry["source_id"])
        path = safe_cache_path(cache_root, str(entry["cache_filename"])).absolute()
        cards.append(
            '<article class="card" data-source-id="' + html.escape(source_id, quote=True)
            + '"><img loading="lazy" src="' + html.escape(path.as_uri(), quote=True)
            + '" alt=""><h2>' + html.escape(source_id) + '</h2><p><b>'
            + html.escape(str(entry["split"])) + '</b> · '
            + html.escape(str(entry["author"])) + '</p><p>'
            + html.escape(str(entry["title"])) + '</p><p>Tags: '
            + html.escape(", ".join(map(str, entry["content_tags"])))
            + '</p><p><a href="' + html.escape(str(entry["landing_page"]), quote=True)
            + '">source and rights</a></p><label><input class="reject" '
            + 'type="checkbox"> reject</label></article>'
        )
    document = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>TGMR people review</title>
<style>body{font-family:sans-serif;margin:1rem}header{position:sticky;top:0;background:#fff;padding:.5rem;z-index:2}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:1rem}.card{border:1px solid #999;padding:.6rem}.card.approved{border:3px solid #176b2c;background:#efe}.card.rejected{border:3px solid #a00;background:#fee}.card img{width:100%;height:240px;object-fit:contain;background:#222}.card h2{font-size:.85rem;overflow-wrap:anywhere}.card p{font-size:.8rem}</style></head>
<body><header><label><input id="complete" type="checkbox"> I reviewed every image</label> <button id="export">Export all decisions</button> <span id="count"></span><span id="exportStatus"></span>
<textarea id="exportText" hidden aria-label="Exported review decisions"></textarea>
</header><main class="grid">""" + "".join(cards) + """</main>
<script>'use strict';
function update(){for(const input of document.querySelectorAll('input')){input.toggleAttribute('checked',input.checked);}const complete=document.getElementById('complete').checked;for(const card of document.querySelectorAll('.card')){const rejected=card.querySelector('.reject').checked;card.classList.toggle('approved',complete&&!rejected);card.classList.toggle('rejected',rejected);}document.getElementById('count').textContent=document.querySelectorAll('.reject:checked').length+' rejected of '+document.querySelectorAll('.card').length+' images';}
document.addEventListener('change',update);update();
function decisions(){const lines=[];for(const card of document.querySelectorAll('.card')){lines.push(JSON.stringify({format:'rawtherapee-tgmr-people-review-decision-v1',people_review_status:card.querySelector('.reject').checked?'rejected':'approved-no-minors-or-sensitive-content',source_id:card.dataset.sourceId}));}return lines.join('\\n')+'\\n';}
document.getElementById('export').addEventListener('click',async()=>{update();const status=document.getElementById('exportStatus');if(!document.getElementById('complete').checked){status.textContent=' Confirm that every image was reviewed before exporting.';return;}const text=decisions();const area=document.getElementById('exportText');area.hidden=false;area.value=text;area.focus();area.select();let copied=false;try{await navigator.clipboard.writeText(text);copied=true;}catch(error){}const blob=new Blob([text],{type:'application/x-ndjson'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='tgmr-people-decisions.jsonl';a.hidden=true;document.body.appendChild(a);a.click();status.textContent=' Prepared '+document.querySelectorAll('.card').length+' decisions with '+document.querySelectorAll('.reject:checked').length+' rejections; '+(copied?'copied to clipboard and ':'')+'download requested. If no file appears, the JSONL is selected below: press Ctrl+C and save it.';setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove();},30000);});
</script></body></html>
"""
    return document.encode("utf-8")


def prepare_catalog_people_review(arguments: argparse.Namespace) -> int:
    records = list(_jsonl(arguments.manifest))
    reviews = []
    queue = []
    seen = set()
    seed = CORPUS_SELECTION_SEED
    for record in records:
        source_id = clean_text(record.get("source_id"), "")
        if record.get("format") != SOURCE_FORMAT or not source_id or source_id in seen:
            raise CorpusPreparationError("catalog people-review manifest is malformed")
        seen.add(source_id)
        tags = record.get("content_tags")
        rights = record.get("rights")
        if not isinstance(tags, list) or any(tag not in CONTENT_TAGS for tag in tags) \
                or not isinstance(rights, dict):
            raise CorpusPreparationError("catalog people-review metadata is malformed")
        people_status = "pending" if "people" in tags else "not-applicable"
        reviews.append({
            "content_tags": sorted(set(map(str, tags))), "format": REVIEW_FORMAT,
            "normalized_author_id": record["author_id"],
            "people_review_status": people_status,
            "rights_evidence_revision": rights["evidence_revision"],
            "rights_evidence_sha256": rights["evidence_sha256"],
            "rights_evidence_url": rights["evidence_url"],
            "rights_review_status": rights["review_status"], "source_id": source_id,
        })
        if people_status == "pending":
            queue.append({
                "author": record["author"], "cache_filename": record["cache_filename"],
                "content_tags": sorted(set(map(str, tags))),
                "format": PEOPLE_REVIEW_QUEUE_FORMAT,
                "landing_page": record["landing_page"], "source_id": source_id,
                "split": _assigned_split(seed, str(record["author_id"])),
                "title": record["title"],
            })
    reviews.sort(key=lambda value: str(value["source_id"]))
    queue.sort(key=lambda value: (
        ("train", "validation", "test").index(str(value["split"])),
        _stable_order(seed, str(value["source_id"])),
    ))
    write_jsonl(arguments.reviews, reviews, arguments.force)
    write_jsonl(arguments.people_queue, queue, arguments.force)
    if arguments.html is not None:
        atomic_bytes(
            arguments.html, _catalog_people_review_html(queue, arguments.cache), arguments.force
        )
    report = {
        "format": "rawtherapee-tgmr-catalog-people-review-preparation-v1",
        "manifest_sha256": sha256_file(arguments.manifest)[0],
        "people_by_split": dict(sorted(collections.Counter(
            str(value["split"]) for value in queue
        ).items())),
        "people_queue_sha256": sha256_file(arguments.people_queue)[0],
        "records": len(records), "reviews_sha256": sha256_file(arguments.reviews)[0],
    }
    if arguments.report is not None:
        atomic_bytes(arguments.report, canonical_pretty(report), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def apply_catalog_people_decisions(arguments: argparse.Namespace) -> int:
    reviews = list(_jsonl(arguments.reviews))
    queue = list(_jsonl(arguments.people_queue))
    queued_ids = {str(record.get("source_id") or "") for record in queue}
    if len(queued_ids) != len(queue) or any(
        record.get("format") != PEOPLE_REVIEW_QUEUE_FORMAT for record in queue
    ):
        raise CorpusPreparationError("catalog people queue is malformed")
    decisions: dict[str, str] = {}
    for record in _jsonl(arguments.decisions):
        if record.get("format") != PEOPLE_DECISION_FORMAT:
            raise CorpusPreparationError("wrong catalog people-decision format")
        source_id = clean_text(record.get("source_id"), "")
        status = record.get("people_review_status")
        if source_id not in queued_ids or source_id in decisions:
            raise CorpusPreparationError("people decision has an unknown or duplicate source")
        if status not in ("approved-no-minors-or-sensitive-content", "rejected"):
            raise CorpusPreparationError("people decision has an invalid status")
        decisions[source_id] = str(status)
    if arguments.require_complete and decisions.keys() != queued_ids:
        raise CorpusPreparationError("people review decisions are incomplete")
    output = []
    for record in reviews:
        if record.get("format") != REVIEW_FORMAT:
            raise CorpusPreparationError("base source review is malformed")
        updated = dict(record)
        source_id = str(record.get("source_id") or "")
        if source_id in decisions:
            updated["people_review_status"] = decisions[source_id]
        output.append(updated)
    write_jsonl(arguments.output, output, arguments.force)
    report = {
        "approved": sum(value == "approved-no-minors-or-sensitive-content"
                        for value in decisions.values()),
        "decisions": len(decisions),
        "format": "rawtherapee-tgmr-catalog-people-review-application-v1",
        "output_sha256": sha256_file(arguments.output)[0],
        "rejected": sum(value == "rejected" for value in decisions.values()),
    }
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def _duplicate_review_html(
    queue: list[dict[str, object]], cache_root: Path,
) -> bytes:
    cards = []
    for entry in queue:
        cluster_id = str(entry["cluster_id"])
        members = entry["members"]
        assert isinstance(members, list)
        images = []
        for member in members:
            assert isinstance(member, dict)
            path = safe_cache_path(cache_root, str(member["cache_filename"])).absolute()
            images.append(
                '<section><img loading="lazy" src="'
                + html.escape(path.as_uri(), quote=True) + '" alt=""><h3>'
                + html.escape(str(member["source_id"])) + '</h3><p>'
                + html.escape(str(member["author"])) + '</p><p>'
                + html.escape(str(member["title"])) + '</p><p><a href="'
                + html.escape(str(member["landing_page"]), quote=True)
                + '">source and rights</a></p><label><input class="reject" '
                + 'type="checkbox" data-source-id="'
                + html.escape(str(member["source_id"]), quote=True)
                + '"> reject as duplicate</label></section>'
            )
        cards.append(
            '<article class="card" data-cluster-id="'
            + html.escape(cluster_id, quote=True) + '"><h2>'
            + str(len(members)) + ' images, ' + str(len(entry["pairs"]))
            + ' borderline pair(s)</h2><label><input class="reviewed" '
            + 'type="checkbox"> cluster fully reviewed</label><div class="images">'
            + ''.join(images) + '</div></article>'
        )
    document = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>TGMR duplicate review</title>
<style>body{font-family:sans-serif;margin:1rem}header{position:sticky;top:0;background:#fff;padding:.5rem;z-index:2}.card{border:1px solid #999;padding:.8rem;margin:1rem 0}.card.reviewed-cluster{border-color:#176b2c}.images{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:1rem}.images section.rejected{border:3px solid #a00;background:#fee}.images img{width:100%;height:260px;object-fit:contain;background:#222}.images h3{font-size:.8rem;overflow-wrap:anywhere}.images p{font-size:.8rem}</style></head>
<body><header><button id="export">Export reviewed clusters</button> <span id="count"></span><span id="exportStatus"></span>
<textarea id="exportText" hidden aria-label="Exported duplicate-review decisions"></textarea>
</header><main>""" + "".join(cards) + """</main>
<script>'use strict';
function update(){for(const input of document.querySelectorAll('.reviewed,.reject')){input.toggleAttribute('checked',input.checked);}for(const card of document.querySelectorAll('.card')){card.classList.toggle('reviewed-cluster',card.querySelector('.reviewed').checked);for(const input of card.querySelectorAll('.reject')){input.closest('section').classList.toggle('rejected',input.checked);}}document.getElementById('count').textContent=document.querySelectorAll('.reviewed:checked').length+' of '+document.querySelectorAll('.card').length+' reviewed clusters; '+document.querySelectorAll('.reject:checked').length+' rejected images';}
document.addEventListener('change',update);update();
function decisions(){const lines=[];for(const card of document.querySelectorAll('.card')){if(!card.querySelector('.reviewed:checked'))continue;const rejected=[...card.querySelectorAll('.reject:checked')].map(x=>x.dataset.sourceId).sort();lines.push(JSON.stringify({cluster_id:card.dataset.clusterId,duplicate_review_status:'resolved',format:'rawtherapee-tgmr-duplicate-review-decision-v1',reject_source_ids:rejected}));}return lines.join('\\n')+(lines.length?'\\n':'');}
document.getElementById('export').addEventListener('click',async()=>{update();const text=decisions();const area=document.getElementById('exportText');const status=document.getElementById('exportStatus');area.hidden=false;area.value=text;area.focus();area.select();let copied=false;try{await navigator.clipboard.writeText(text);copied=true;}catch(error){}const blob=new Blob([text],{type:'application/x-ndjson'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='tgmr-duplicate-decisions.jsonl';a.hidden=true;document.body.appendChild(a);a.click();status.textContent=' Prepared '+document.querySelectorAll('.reviewed:checked').length+' reviewed-cluster decisions; '+(copied?'copied to clipboard and ':'')+'download requested. If no file appears, the JSONL is selected below: press Ctrl+C and save it.';setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove();},30000);});
</script></body></html>
"""
    return document.encode("utf-8")


def _source_review_candidate(record: dict[str, object]) -> bool:
    rights = record.get("rights")
    people_status = record.get("people_review_status")
    return (
        record.get("format") == SOURCE_FORMAT
        and isinstance(rights, dict)
        and rights.get("review_status") == "approved"
        and people_status in (
            "not-applicable", "approved-no-minors-or-sensitive-content",
        )
        and record.get("license") in ACCEPTED_LICENSES
        and min(int(record.get("width", 0)), int(record.get("height", 0))) >= 512
        and int(record.get("width", 0)) * int(record.get("height", 0)) >= 750000
    )


def prepare_duplicate_review(arguments: argparse.Namespace) -> int:
    records = list(_jsonl(arguments.manifest))
    if not records:
        raise CorpusPreparationError("duplicate review input is empty")
    candidates = []
    author_counts: collections.Counter[tuple[str, str]] = collections.Counter()
    for record in sorted(
        (value for value in records if _source_review_candidate(value)),
        key=lambda value: _stable_order(CORPUS_SELECTION_SEED, str(value["source_id"])),
    ):
        classification = record.get("classification")
        if not isinstance(classification, dict):
            raise CorpusPreparationError("duplicate review input lacks classification")
        dhash = str(classification.get("dhash") or "")
        phash = str(classification.get("phash") or "")
        if not re.fullmatch(r"[0-9a-f]{16}", dhash) or not re.fullmatch(
            r"[0-9a-f]{16}", phash
        ):
            raise CorpusPreparationError("duplicate review input has malformed signatures")
        split = _assigned_split(CORPUS_SELECTION_SEED, str(record["author_id"]))
        author_key = split, str(record["author_id"])
        # The review queue covers every candidate that can survive the frozen
        # five-image author cap. Final selection still rechecks all hard
        # duplicate identities and distances independently.
        if author_counts[author_key] >= 5:
            continue
        author_counts[author_key] += 1
        candidates.append((record, split, int(dhash, 16), int(phash, 16)))

    borderline_pairs = []
    hard_pairs = collections.Counter()
    candidate_details = {}
    adjacency: collections.defaultdict[str, set[str]] = collections.defaultdict(set)
    for right_index, (right, right_split, right_dhash, right_phash) in enumerate(candidates):
        for left, left_split, left_dhash, left_phash in candidates[:right_index]:
            dhash_distance = (left_dhash ^ right_dhash).bit_count()
            phash_distance = (left_phash ^ right_phash).bit_count()
            if dhash_distance <= HARD_DHASH_DISTANCE or phash_distance <= HARD_PHASH_DISTANCE:
                hard_pairs[
                    "dhash" if dhash_distance <= HARD_DHASH_DISTANCE else "phash"
                ] += 1
                continue
            if dhash_distance > BORDERLINE_DHASH_DISTANCE \
                    and phash_distance > BORDERLINE_PHASH_DISTANCE:
                continue
            left_id, right_id = sorted((str(left["source_id"]), str(right["source_id"])))
            if left_id == str(left["source_id"]):
                left_record, right_record = left, right
                canonical_left_split, canonical_right_split = left_split, right_split
            else:
                left_record, right_record = right, left
                canonical_left_split, canonical_right_split = right_split, left_split
            borderline_pairs.append({
                "dhash_distance": dhash_distance,
                "left_source_id": left_id,
                "phash_distance": phash_distance,
                "right_source_id": right_id,
            })
            candidate_details[left_id] = {
                "author": left_record["author"],
                "cache_filename": left_record["cache_filename"],
                "landing_page": left_record["landing_page"], "source_id": left_id,
                "split": canonical_left_split, "title": left_record["title"],
            }
            candidate_details[right_id] = {
                "author": right_record["author"],
                "cache_filename": right_record["cache_filename"],
                "landing_page": right_record["landing_page"], "source_id": right_id,
                "split": canonical_right_split, "title": right_record["title"],
            }
            adjacency[left_id].add(right_id)
            adjacency[right_id].add(left_id)

    queue = []
    seen: set[str] = set()
    for source_id in sorted(adjacency):
        if source_id in seen:
            continue
        stack = [source_id]
        seen.add(source_id)
        component = []
        while stack:
            current = stack.pop()
            component.append(current)
            for neighbor in sorted(adjacency[current], reverse=True):
                if neighbor not in seen:
                    seen.add(neighbor)
                    stack.append(neighbor)
        component.sort()
        component_set = set(component)
        pairs = sorted(
            (pair for pair in borderline_pairs
             if str(pair["left_source_id"]) in component_set
             and str(pair["right_source_id"]) in component_set),
            key=lambda pair: (str(pair["left_source_id"]), str(pair["right_source_id"])),
        )
        cluster_id = "duplicate-cluster-" + hashlib.sha256(
            "\0".join(component).encode()
        ).hexdigest()[:24]
        queue.append({
            "cluster_id": cluster_id,
            "format": DUPLICATE_REVIEW_QUEUE_FORMAT,
            "members": [candidate_details[value] for value in component],
            "pairs": pairs,
        })
    queue.sort(key=lambda value: str(value["cluster_id"]))
    involved = set(adjacency)

    pending = []
    for record in records:
        updated = dict(record)
        if str(record.get("source_id") or "") in involved:
            updated["selection_status"] = "candidate-pending-duplicate-review"
        pending.append(updated)
    write_jsonl(arguments.pending_manifest, pending, arguments.force)
    write_jsonl(arguments.duplicate_queue, queue, arguments.force)
    if arguments.html is not None:
        atomic_bytes(
            arguments.html, _duplicate_review_html(queue, arguments.cache), arguments.force
        )
    report = {
        "borderline_dhash_hamming_max": BORDERLINE_DHASH_DISTANCE,
        "borderline_clusters": len(queue),
        "borderline_pairs": len(borderline_pairs),
        "borderline_phash_hamming_max": BORDERLINE_PHASH_DISTANCE,
        "candidate_manifest_sha256": sha256_file(arguments.manifest)[0],
        "candidates_after_author_cap": len(candidates),
        "format": "rawtherapee-tgmr-duplicate-review-preparation-v1",
        "hard_dhash_hamming_max": HARD_DHASH_DISTANCE,
        "hard_duplicate_pairs": dict(sorted(hard_pairs.items())),
        "hard_phash_hamming_max": HARD_PHASH_DISTANCE,
        "involved_sources": len(involved),
        "pending_manifest_sha256": sha256_file(arguments.pending_manifest)[0],
        "queue_sha256": sha256_file(arguments.duplicate_queue)[0],
    }
    if arguments.report is not None:
        atomic_bytes(arguments.report, canonical_pretty(report), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def apply_duplicate_decisions(arguments: argparse.Namespace) -> int:
    records = list(_jsonl(arguments.pending_manifest))
    queue = list(_jsonl(arguments.duplicate_queue))
    clusters: dict[str, set[str]] = {}
    source_cluster: dict[str, str] = {}
    for entry in queue:
        if entry.get("format") != DUPLICATE_REVIEW_QUEUE_FORMAT:
            raise CorpusPreparationError("wrong duplicate-review queue format")
        cluster_id = clean_text(entry.get("cluster_id"), "")
        members = entry.get("members")
        if not cluster_id or cluster_id in clusters or not isinstance(members, list):
            raise CorpusPreparationError("duplicate-review cluster identity is malformed")
        source_ids = {
            clean_text(member.get("source_id"), "")
            for member in members if isinstance(member, dict)
        }
        if len(source_ids) != len(members) or len(source_ids) < 2 or "" in source_ids:
            raise CorpusPreparationError("duplicate-review cluster members are malformed")
        if any(source_id in source_cluster for source_id in source_ids):
            raise CorpusPreparationError("source occurs in multiple duplicate-review clusters")
        clusters[cluster_id] = source_ids
        for source_id in source_ids:
            source_cluster[source_id] = cluster_id

    decisions: dict[str, set[str]] = {}
    for entry in _jsonl(arguments.decisions):
        if entry.get("format") != DUPLICATE_DECISION_FORMAT:
            raise CorpusPreparationError("wrong duplicate-review decision format")
        cluster_id = clean_text(entry.get("cluster_id"), "")
        status = entry.get("duplicate_review_status")
        rejected_values = entry.get("reject_source_ids")
        if cluster_id not in clusters or cluster_id in decisions:
            raise CorpusPreparationError("duplicate decision has an unknown or duplicate cluster")
        if status != "resolved" or not isinstance(rejected_values, list):
            raise CorpusPreparationError("duplicate decision has an invalid status")
        rejected_values = list(map(str, rejected_values))
        rejected = set(rejected_values)
        if len(rejected) != len(rejected_values) or not rejected.issubset(clusters[cluster_id]):
            raise CorpusPreparationError("duplicate decision rejects an invalid source")
        decisions[cluster_id] = rejected
    if arguments.require_complete and decisions.keys() != clusters.keys():
        raise CorpusPreparationError("duplicate review decisions are incomplete")

    unreviewed_clusters = clusters.keys() - decisions.keys()
    explicit_rejected = set().union(*decisions.values()) if decisions else set()
    rejected_unreviewed = (
        set().union(*(clusters[cluster_id] for cluster_id in unreviewed_clusters))
        if arguments.reject_unreviewed_clusters and unreviewed_clusters else set()
    )
    rejected = explicit_rejected | rejected_unreviewed
    output = []
    for record in records:
        if record.get("format") != SOURCE_FORMAT:
            raise CorpusPreparationError("duplicate-review manifest contains a foreign record")
        updated = dict(record)
        source_id = str(record.get("source_id") or "")
        if source_id in rejected:
            updated["selection_status"] = "candidate-rejected-duplicate"
        elif source_id in source_cluster:
            updated["selection_status"] = (
                "candidate-reviewed"
                if source_cluster[source_id] in decisions
                else "candidate-pending-duplicate-review"
            )
        output.append(updated)
    write_jsonl(arguments.output, output, arguments.force)
    report = {
        "decisions": len(decisions),
        "distinct_clusters": sum(not value for value in decisions.values()),
        "explicitly_rejected_sources": len(explicit_rejected),
        "format": "rawtherapee-tgmr-duplicate-review-application-v1",
        "output_sha256": sha256_file(arguments.output)[0],
        "rejected_unreviewed_sources": len(rejected_unreviewed),
        "rejected_sources": len(rejected),
        "reviewed_clusters": len(decisions),
        "unreviewed_clusters": len(unreviewed_clusters),
    }
    sys.stdout.buffer.write(canonical_pretty(report))
    return 0


def release_manifest(arguments: argparse.Namespace) -> int:
    if not arguments.force and (
        arguments.output.exists()
        or (arguments.checksums_output is not None and arguments.checksums_output.exists())
    ):
        raise CorpusPreparationError("refusing to replace release output")
    inputs = [
        ("source-manifest", arguments.source_manifest),
        ("tgpc", arguments.tgpc),
        ("tgpc-gzip", arguments.tgpc_gzip),
        ("attribution-notice", arguments.attribution),
        ("corpus-statistics", arguments.statistics),
        ("rights-report", arguments.rights_report),
    ]
    inputs.extend((role, path) for role, path in (
        ("source-manifest-gzip", arguments.source_manifest_gzip),
        ("corpus-statistics-csv", arguments.statistics_csv),
        ("corpus-statistics-html", arguments.statistics_html),
        ("reconstruction-list", arguments.reconstruction),
    ) if path is not None)
    filenames = [path.name for _, path in inputs]
    if arguments.checksums_output is not None:
        filenames.append(arguments.checksums_output.name)
    if len(filenames) != len(set(filenames)):
        raise CorpusPreparationError("release artifacts must have unique filenames")
    artifacts = []
    for role, path in inputs:
        digest, size = sha256_file(path)
        artifacts.append({
            "bytes": size, "filename": path.name, "role": role, "sha256": digest,
        })
    if arguments.checksums_output is not None:
        checksum_bytes = "".join(
            f"{artifact['sha256']}  {artifact['filename']}\n"
            for artifact in artifacts
        ).encode("utf-8")
        atomic_bytes(arguments.checksums_output, checksum_bytes, arguments.force)
        digest, size = sha256_file(arguments.checksums_output)
        artifacts.append({
            "bytes": size,
            "filename": arguments.checksums_output.name,
            "role": "checksums",
            "sha256": digest,
        })
    value = {
        "artifacts": artifacts,
        "corpus_id": "tgmr-corpus-v1",
        "format": "rawtherapee-tgmr-corpus-release-manifest-v1",
        "github_release_url": arguments.github_release_url,
        "zenodo_doi": arguments.zenodo_doi,
    }
    atomic_bytes(arguments.output, canonical_pretty(value), arguments.force)
    sys.stdout.buffer.write(canonical_pretty(value))
    return 0


def merge_jsonl(arguments: argparse.Namespace) -> int:
    cache_prefixes: dict[str, Path] = {}
    for specification in arguments.cache_prefix:
        catalog, separator, prefix = specification.partition("=")
        prefix_path = Path(prefix)
        if (
            not separator or not catalog or not prefix
            or catalog in cache_prefixes or prefix_path.is_absolute()
            or any(part in ("", ".", "..") for part in prefix_path.parts)
        ):
            raise CorpusPreparationError(
                "merge --cache-prefix must be a unique CATALOG=safe/relative/path"
            )
        cache_prefixes[catalog] = prefix_path
    used_prefixes: set[str] = set()
    records = []
    identities = set()
    expected_format = None
    for path in arguments.inputs:
        for value in _jsonl(path):
            record_format = value.get("format")
            if expected_format is None:
                expected_format = record_format
            if record_format != expected_format:
                raise CorpusPreparationError("cannot merge different JSONL record formats")
            identity = (
                value.get("source_id")
                or f"{value.get('catalog', '')}:{value.get('upstream_source_id', '')}"
            )
            if identity == ":":
                identity = value.get("id") or value.get("title")
            if not identity:
                raise CorpusPreparationError("merged JSONL record has no stable identity")
            if identity in identities:
                raise CorpusPreparationError(f"duplicate merged identity: {identity}")
            identities.add(identity)
            merged = value
            catalog = value.get("catalog")
            catalog_name = (
                clean_text(catalog.get("name"), "")
                if isinstance(catalog, dict) else clean_text(catalog, "")
            )
            if catalog_name in cache_prefixes:
                cache_filename = clean_text(value.get("cache_filename"), "")
                cache_path = Path(cache_filename)
                if (
                    not cache_filename or cache_path.is_absolute()
                    or any(part in ("", ".", "..") for part in cache_path.parts)
                ):
                    raise CorpusPreparationError(
                        "cannot prefix a missing or unsafe cache filename"
                    )
                merged = dict(value)
                merged["cache_filename"] = (
                    cache_prefixes[catalog_name] / cache_path
                ).as_posix()
                used_prefixes.add(catalog_name)
            records.append(merged)
    unused_prefixes = sorted(cache_prefixes.keys() - used_prefixes)
    if unused_prefixes:
        raise CorpusPreparationError(
            "merge cache prefix did not match a catalog: " + ", ".join(unused_prefixes)
        )
    catalog_order = {
        "openimages-cvdf-v5-boxable": 0, "pass-v3": 1,
        "wikimedia-commons": 2, "smithsonian-open-access": 3,
    }
    def key(value: dict[str, object]) -> tuple[int, str]:
        catalog = value.get("catalog")
        if isinstance(catalog, dict):
            catalog = catalog.get("name")
        return catalog_order.get(str(catalog), 99), str(
            value.get("source_id") or value.get("upstream_source_id")
            or value.get("id") or value.get("title") or ""
        )
    records.sort(key=key)
    write_jsonl(arguments.output, records, arguments.force)
    sys.stdout.buffer.write(canonical_pretty({
        "format": "rawtherapee-tgmr-jsonl-merge-report-v1",
        "record_format": expected_format, "records": len(records),
        "sha256": sha256_file(arguments.output)[0],
    }))
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)
    snapshot_parser = commands.add_parser("snapshot")
    snapshot_parser.add_argument("url")
    snapshot_parser.add_argument("output", type=Path)
    snapshot_parser.add_argument("--sha256")
    snapshot_parser.add_argument("--force", action="store_true")
    snapshot_parser.set_defaults(function=snapshot)

    commons_parser = commands.add_parser("collect-commons")
    commons_parser.add_argument("ids", type=Path)
    commons_parser.add_argument("output", type=Path)
    commons_parser.add_argument("--force", action="store_true")
    commons_parser.set_defaults(function=collect_commons)

    commons_categories_parser = commands.add_parser(
        "collect-commons-categories",
        help="freeze a rights-filtered Commons snapshot from reviewed category roots",
    )
    commons_categories_parser.add_argument("recipe", type=Path)
    commons_categories_parser.add_argument("output", type=Path)
    commons_categories_parser.add_argument("--report", type=Path)
    commons_categories_parser.add_argument("--limit", type=int)
    commons_categories_parser.add_argument("--force", action="store_true")
    commons_categories_parser.set_defaults(function=collect_commons_categories)

    commons_enrich_parser = commands.add_parser(
        "enrich-commons-categories",
        help="freeze file-page categories for an exact Commons snapshot",
    )
    commons_enrich_parser.add_argument("input", type=Path)
    commons_enrich_parser.add_argument("output", type=Path)
    commons_enrich_parser.add_argument("--report", type=Path)
    commons_enrich_parser.add_argument("--force", action="store_true")
    commons_enrich_parser.set_defaults(function=enrich_commons_categories)

    smithsonian_parser = commands.add_parser(
        "collect-smithsonian",
        help="collect Smithsonian records anonymously from the public AWS metadata",
    )
    smithsonian_parser.add_argument("output", type=Path)
    smithsonian_parser.add_argument("--unit", action="append", required=True)
    shard_selection = smithsonian_parser.add_mutually_exclusive_group(required=True)
    shard_selection.add_argument("--prefix", action="append")
    shard_selection.add_argument("--all-shards", action="store_true")
    smithsonian_parser.add_argument("--per-unit-limit", type=int)
    smithsonian_parser.add_argument("--index-url", default=SMITHSONIAN_AWS_INDEX)
    smithsonian_parser.add_argument("--snapshot-dir", type=Path)
    smithsonian_parser.add_argument("--offline-snapshot", action="store_true")
    smithsonian_parser.add_argument("--report", type=Path)
    smithsonian_parser.add_argument("--force", action="store_true")
    smithsonian_parser.set_defaults(function=collect_smithsonian_aws)

    smithsonian_api_parser = commands.add_parser(
        "collect-smithsonian-api",
        help="optional diagnostic collector for an explicit Smithsonian ID list",
    )
    smithsonian_api_parser.add_argument("ids", type=Path)
    smithsonian_api_parser.add_argument("output", type=Path)
    smithsonian_api_parser.add_argument("--api-key", required=True)
    smithsonian_api_parser.add_argument("--force", action="store_true")
    smithsonian_api_parser.set_defaults(function=collect_smithsonian_api)

    normalize_parser = commands.add_parser("normalize")
    normalize_parser.add_argument("catalog", choices=("openimages", "pass", "commons", "smithsonian"))
    normalize_parser.add_argument("input", type=Path)
    normalize_parser.add_argument("output", type=Path)
    normalize_parser.add_argument("--revision", required=True)
    normalize_parser.add_argument("--snapshot-sha256")
    normalize_parser.add_argument("--urls", type=Path)
    normalize_parser.add_argument("--archive-index", type=Path)
    normalize_parser.add_argument(
        "--tag-rules", type=Path,
        default=Path(__file__).with_name("catalog-content-tags-v1.json"),
    )
    normalize_parser.add_argument("--limit", type=int)
    normalize_parser.add_argument("--start", type=int, default=0)
    normalize_parser.add_argument("--eligible-only", action="store_true")
    normalize_parser.add_argument("--force", action="store_true")
    normalize_parser.set_defaults(function=normalize)

    fetch_parser = commands.add_parser("fetch")
    fetch_parser.add_argument("candidates", type=Path)
    fetch_parser.add_argument("cache", type=Path)
    fetch_parser.add_argument("output", type=Path)
    fetch_parser.add_argument("--start", type=int, default=0)
    fetch_parser.add_argument("--limit", type=int)
    fetch_parser.add_argument("--retry", type=int, default=2)
    fetch_parser.add_argument("--jobs", type=int, default=1)
    fetch_parser.add_argument("--request-delay", type=float, default=0.0)
    fetch_parser.add_argument("--report", type=Path)
    fetch_parser.add_argument("--force", action="store_true")
    fetch_parser.set_defaults(function=fetch)

    assemble_parser = commands.add_parser("assemble")
    assemble_parser.add_argument("candidates", type=Path)
    assemble_parser.add_argument("classifications", type=Path)
    assemble_parser.add_argument("cache", type=Path)
    assemble_parser.add_argument("output", type=Path)
    assemble_parser.add_argument("--reviews", type=Path)
    assemble_parser.add_argument("--jobs", type=int, default=4)
    assemble_parser.add_argument("--force", action="store_true")
    assemble_parser.set_defaults(function=assemble)

    openimages_review_parser = commands.add_parser("prepare-openimages-review")
    openimages_review_parser.add_argument("manifest", type=Path)
    openimages_review_parser.add_argument("class_descriptions", type=Path)
    openimages_review_parser.add_argument("human_labels", type=Path)
    openimages_review_parser.add_argument("boxes", type=Path)
    openimages_review_parser.add_argument("cache", type=Path)
    openimages_review_parser.add_argument("reviews", type=Path)
    openimages_review_parser.add_argument("people_queue", type=Path)
    openimages_review_parser.add_argument("report", type=Path)
    openimages_review_parser.add_argument(
        "--rules", type=Path,
        default=Path(__file__).with_name("openimages-content-tags-v1.json"),
    )
    openimages_review_parser.add_argument("--class-descriptions-sha256")
    openimages_review_parser.add_argument("--human-labels-sha256")
    openimages_review_parser.add_argument("--boxes-sha256")
    openimages_review_parser.add_argument("--rules-sha256")
    openimages_review_parser.add_argument("--candidate-pool-size", type=int)
    openimages_review_parser.add_argument("--html", type=Path)
    openimages_review_parser.add_argument("--force", action="store_true")
    openimages_review_parser.set_defaults(function=prepare_openimages_review)

    apply_people_parser = commands.add_parser("apply-openimages-people-decisions")
    apply_people_parser.add_argument("reviews", type=Path)
    apply_people_parser.add_argument("people_queue", type=Path)
    apply_people_parser.add_argument("decisions", type=Path)
    apply_people_parser.add_argument("output", type=Path)
    apply_people_parser.add_argument("--require-complete", action="store_true")
    apply_people_parser.add_argument("--force", action="store_true")
    apply_people_parser.set_defaults(function=apply_openimages_people_decisions)

    catalog_people_parser = commands.add_parser("prepare-catalog-people-review")
    catalog_people_parser.add_argument("manifest", type=Path)
    catalog_people_parser.add_argument("cache", type=Path)
    catalog_people_parser.add_argument("reviews", type=Path)
    catalog_people_parser.add_argument("people_queue", type=Path)
    catalog_people_parser.add_argument("--report", type=Path)
    catalog_people_parser.add_argument("--html", type=Path)
    catalog_people_parser.add_argument("--force", action="store_true")
    catalog_people_parser.set_defaults(function=prepare_catalog_people_review)

    apply_catalog_people_parser = commands.add_parser("apply-catalog-people-decisions")
    apply_catalog_people_parser.add_argument("reviews", type=Path)
    apply_catalog_people_parser.add_argument("people_queue", type=Path)
    apply_catalog_people_parser.add_argument("decisions", type=Path)
    apply_catalog_people_parser.add_argument("output", type=Path)
    apply_catalog_people_parser.add_argument("--require-complete", action="store_true")
    apply_catalog_people_parser.add_argument("--force", action="store_true")
    apply_catalog_people_parser.set_defaults(function=apply_catalog_people_decisions)

    duplicate_parser = commands.add_parser("prepare-duplicate-review")
    duplicate_parser.add_argument("manifest", type=Path)
    duplicate_parser.add_argument("cache", type=Path)
    duplicate_parser.add_argument("pending_manifest", type=Path)
    duplicate_parser.add_argument("duplicate_queue", type=Path)
    duplicate_parser.add_argument("--report", type=Path)
    duplicate_parser.add_argument("--html", type=Path)
    duplicate_parser.add_argument("--force", action="store_true")
    duplicate_parser.set_defaults(function=prepare_duplicate_review)

    apply_duplicate_parser = commands.add_parser("apply-duplicate-decisions")
    apply_duplicate_parser.add_argument("pending_manifest", type=Path)
    apply_duplicate_parser.add_argument("duplicate_queue", type=Path)
    apply_duplicate_parser.add_argument("decisions", type=Path)
    apply_duplicate_parser.add_argument("output", type=Path)
    duplicate_completion = apply_duplicate_parser.add_mutually_exclusive_group()
    duplicate_completion.add_argument("--require-complete", action="store_true")
    duplicate_completion.add_argument(
        "--reject-unreviewed-clusters", action="store_true",
        help="reject every member of each cluster absent from the decisions file",
    )
    apply_duplicate_parser.add_argument("--force", action="store_true")
    apply_duplicate_parser.set_defaults(function=apply_duplicate_decisions)

    release_parser = commands.add_parser("release-manifest")
    release_parser.add_argument("source_manifest", type=Path)
    release_parser.add_argument("tgpc", type=Path)
    release_parser.add_argument("tgpc_gzip", type=Path)
    release_parser.add_argument("attribution", type=Path)
    release_parser.add_argument("statistics", type=Path)
    release_parser.add_argument("rights_report", type=Path)
    release_parser.add_argument("output", type=Path)
    release_parser.add_argument("--source-manifest-gzip", type=Path)
    release_parser.add_argument("--statistics-csv", type=Path)
    release_parser.add_argument("--statistics-html", type=Path)
    release_parser.add_argument("--reconstruction", type=Path)
    release_parser.add_argument("--checksums-output", type=Path)
    release_parser.add_argument("--zenodo-doi", required=True)
    release_parser.add_argument("--github-release-url", required=True)
    release_parser.add_argument("--force", action="store_true")
    release_parser.set_defaults(function=release_manifest)

    merge_parser = commands.add_parser("merge")
    merge_parser.add_argument("output", type=Path)
    merge_parser.add_argument("inputs", nargs="+", type=Path)
    merge_parser.add_argument(
        "--cache-prefix", action="append", default=[], metavar="CATALOG=PATH",
        help="prepend a safe relative path to one catalog's cache filenames",
    )
    merge_parser.add_argument("--force", action="store_true")
    merge_parser.set_defaults(function=merge_jsonl)
    return result


def main(argv: list[str] | None = None) -> int:
    arguments = parser().parse_args(argv)
    if getattr(arguments, "limit", None) is not None and arguments.limit < 0:
        raise SystemExit("--limit must be non-negative")
    if getattr(arguments, "start", 0) < 0 or getattr(arguments, "retry", 0) < 0:
        raise SystemExit("--start and --retry must be non-negative")
    if getattr(arguments, "per_unit_limit", None) is not None and arguments.per_unit_limit < 0:
        raise SystemExit("--per-unit-limit must be non-negative")
    if getattr(arguments, "candidate_pool_size", None) is not None \
            and arguments.candidate_pool_size < 0:
        raise SystemExit("--candidate-pool-size must be non-negative")
    if getattr(arguments, "jobs", 1) < 1:
        raise SystemExit("--jobs must be positive")
    if getattr(arguments, "sha256", None):
        require_sha256(arguments.sha256, "--sha256")
    if getattr(arguments, "snapshot_sha256", None):
        require_sha256(arguments.snapshot_sha256, "--snapshot-sha256")
    for attribute in (
        "class_descriptions_sha256", "human_labels_sha256", "boxes_sha256",
        "rules_sha256",
    ):
        if getattr(arguments, attribute, None):
            require_sha256(getattr(arguments, attribute), "--" + attribute.replace("_", "-"))
    if hasattr(arguments, "candidate_pool_size") and arguments.candidate_pool_size is None:
        arguments.candidate_pool_size = sum(1 for _ in _jsonl(arguments.manifest))
    return int(arguments.function(arguments))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CorpusPreparationError as error:
        print(f"prepare_corpus.py: {error}", file=sys.stderr)
        raise SystemExit(2)
