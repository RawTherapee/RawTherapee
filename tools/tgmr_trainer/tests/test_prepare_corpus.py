from __future__ import annotations

import base64
import csv
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock
import urllib.error

from tools.tgmr_trainer import prepare_corpus, reconstruct_corpus


def expanded_classification() -> dict[str, object]:
    return {
        "channel_means": [0.1, 0.2, 0.3],
        "chroma_ratio_mean": 0.1,
        "clipped_black_fraction": 0.0,
        "clipped_white_fraction": 0.0,
        "dhash": "0123456789abcdef",
        "gradient_rms": 0.01,
        "hue_degrees": 30.0,
        "hue_histogram": [1] * 12,
        "jpeg_blockiness": 0.0,
        "laplacian_rms": 0.01,
        "local_contrast": 0.02,
        "luminance_histogram": [1] * 16,
        "luminance_mean": 0.2,
        "luminance_p01": 0.01,
        "luminance_p99": 0.9,
        "luminance_stddev": 0.1,
        "perceptual_hash": "0123456789abcdef",
        "phash": "fedcba9876543210",
        "saturation_histogram": [1] * 8,
        "saturation_mean": 0.2,
    }


class PreparationTests(unittest.TestCase):
    def test_license_filter_accepts_only_frozen_permissive_set(self) -> None:
        self.assertEqual(
            prepare_corpus.license_id("https://creativecommons.org/licenses/by/4.0/"),
            "CC-BY-4.0",
        )
        self.assertEqual(prepare_corpus.license_id("CC0"), "CC0-1.0")
        self.assertIsNone(prepare_corpus.license_id(
            "https://creativecommons.org/licenses/by-sa/4.0/"
        ))
        self.assertIsNone(prepare_corpus.license_id(
            "https://creativecommons.org/licenses/by-nc/4.0/"
        ))
        self.assertIsNone(prepare_corpus.license_id("no known copyright restrictions"))

    def test_mediawiki_base36_sha1_verification(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.bin"
            path.write_bytes(b"MediaWiki base-36 SHA-1 fixture\n")
            value = int(hashlib.sha1(path.read_bytes()).hexdigest(), 16)
            alphabet = "0123456789abcdefghijklmnopqrstuvwxyz"
            digits = []
            while value:
                value, remainder = divmod(value, 36)
                digits.append(alphabet[remainder])
            base36 = "".join(reversed(digits)) or "0"
            self.assertTrue(prepare_corpus._advertised_matches(
                path, "sha1-base36:" + base36
            ))
            self.assertFalse(prepare_corpus._advertised_matches(
                path, "sha1-base36:" + ("0" if base36 != "0" else "1")
            ))

    def test_merge_and_release_manifest_are_canonical(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "first.jsonl"
            second = root / "second.jsonl"
            first.write_text(json.dumps({
                "catalog": "pass-v3", "format": prepare_corpus.CANDIDATE_FORMAT,
                "cache_filename": "b.jpg", "source_id": "pass-v3:b",
            }, sort_keys=True) + "\n", encoding="utf-8")
            second.write_text(json.dumps({
                "catalog": "openimages-cvdf-v5-boxable",
                "format": prepare_corpus.CANDIDATE_FORMAT,
                "cache_filename": "train/a.jpg",
                "source_id": "openimages-cvdf-v5-boxable:a",
            }, sort_keys=True) + "\n", encoding="utf-8")
            merged = root / "merged.jsonl"
            self.assertEqual(prepare_corpus.main([
                "merge", str(merged), str(first), str(second),
                "--cache-prefix", "openimages-cvdf-v5-boxable=open-images",
                "--cache-prefix", "pass-v3=pass",
            ]), 0)
            merged_values = [json.loads(line) for line in merged.read_text().splitlines()]
            self.assertEqual([value["source_id"] for value in merged_values], [
                "openimages-cvdf-v5-boxable:a", "pass-v3:b",
            ])
            self.assertEqual(
                [value["cache_filename"] for value in merged_values],
                ["open-images/train/a.jpg", "pass/b.jpg"],
            )

            artifacts = []
            for index, name in enumerate((
                    "corpus.jsonl", "corpus.tgpc", "corpus.tgpc.gz", "NOTICE.txt",
                    "statistics.json", "rights.json")):
                path = root / name
                path.write_bytes(f"artifact-{index}\n".encode())
                artifacts.append(path)
            optional = {}
            for index, (option, name) in enumerate((
                ("--source-manifest-gzip", "corpus.jsonl.gz"),
                ("--statistics-csv", "statistics.csv"),
                ("--statistics-html", "statistics.html"),
                ("--reconstruction", "reconstruction.tsv"),
            ), start=len(artifacts)):
                path = root / name
                path.write_bytes(f"artifact-{index}\n".encode())
                optional[option] = path
            checksums = root / "SHA256SUMS"
            release = root / "release.json"
            arguments = [
                "release-manifest", *(str(path) for path in artifacts), str(release),
                "--zenodo-doi", "10.5281/zenodo.fixture",
                "--github-release-url", "https://example.invalid/release",
            ]
            for option, path in optional.items():
                arguments.extend((option, str(path)))
            arguments.extend(("--checksums-output", str(checksums)))
            self.assertEqual(prepare_corpus.main(arguments), 0)
            first_bytes = release.read_bytes()
            first_checksums = checksums.read_bytes()
            release.unlink()
            checksums.unlink()
            self.assertEqual(prepare_corpus.main(arguments), 0)
            self.assertEqual(release.read_bytes(), first_bytes)
            self.assertEqual(checksums.read_bytes(), first_checksums)
            value = json.loads(first_bytes)
            self.assertEqual(value["corpus_id"], "tgmr-corpus-v1")
            self.assertEqual(len(value["artifacts"]), 11)
            self.assertEqual(value["artifacts"][-1]["role"], "checksums")
            self.assertEqual(len(checksums.read_text().splitlines()), 10)

    def test_merge_accepts_raw_snapshot_ids_without_record_format(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "first.jsonl"
            second = root / "second.jsonl"
            first.write_text('{"id":"smithsonian-b"}\n', encoding="utf-8")
            second.write_text('{"id":"smithsonian-a"}\n', encoding="utf-8")
            output = root / "merged.jsonl"
            self.assertEqual(prepare_corpus.main([
                "merge", str(output), str(first), str(second),
            ]), 0)
            self.assertEqual(
                [json.loads(line)["id"] for line in output.read_text().splitlines()],
                ["smithsonian-a", "smithsonian-b"],
            )

    def test_openimages_normalize_fetch_and_assemble(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.jpg"
            data = b"catalog-authenticated-image"
            source.write_bytes(data)
            csv_path = root / "openimages.csv"
            md5 = hashlib.md5(data).digest()  # noqa: S324 - catalog fixture identity.
            with csv_path.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=[
                    "ImageID", "OriginalURL", "OriginalLandingURL", "License",
                    "AuthorProfileURL", "Author", "Title", "OriginalMD5",
                ])
                writer.writeheader()
                writer.writerow({
                    "ImageID": "abc1234567890def", "OriginalURL": source.as_uri(),
                    "OriginalLandingURL": "https://www.flickr.com/photos/example/123456789/",
                    "License": "https://creativecommons.org/licenses/by/2.0/",
                    "AuthorProfileURL": "https://www.flickr.com/people/example/",
                    "Author": "Example Author", "Title": "Example",
                    "OriginalMD5": base64.b64encode(md5).decode(),
                })
            candidates = root / "candidates.jsonl"
            self.assertEqual(prepare_corpus.main([
                "normalize", "openimages", str(csv_path), str(candidates),
                "--revision", "fixture-boxable",
            ]), 0)
            first = json.loads(candidates.read_text())
            self.assertEqual(first["catalog"], "openimages-cvdf-v5-boxable")
            self.assertEqual(first["license"], "CC-BY-2.0")
            self.assertEqual(first["author_id"], "flickr-user:example")
            self.assertEqual(first["upstream_flickr_id"], "123456789")

            cache = root / "cache"
            fetched = root / "fetched.jsonl"
            report = root / "fetch-report.json"
            self.assertEqual(prepare_corpus.main([
                "fetch", str(candidates), str(cache), str(fetched),
                "--retry", "0", "--jobs", "2", "--request-delay", "0.001",
                "--report", str(report),
            ]), 0)
            fetched_record = json.loads(fetched.read_text())
            self.assertEqual(json.loads(report.read_text())["request_delay_seconds"], 0.001)
            self.assertNotIn(":", fetched_record["cache_filename"])
            source_id = "openimages-cvdf-v5-boxable:abc1234567890def"
            classification = root / "classifications.jsonl"
            classification.write_text(json.dumps({
                "cache_filename": fetched_record["cache_filename"],
                "classification": expanded_classification(),
                "decoded_pixel_sha256": "1" * 64,
                "file_type": "jpeg", "height": 800,
                "icc_identity": "assumed-srgb", "orientation": 1,
                "source_id": source_id, "width": 1200,
            }, sort_keys=True) + "\n", encoding="utf-8")
            reviews = root / "reviews.jsonl"
            reviews.write_text(json.dumps({
                "content_tags": ["people", "skin-hair-clothing"],
                "format": prepare_corpus.REVIEW_FORMAT,
                "people_review_status": "approved-no-minors-or-sensitive-content",
                "rights_evidence_revision": "fixture-review-1",
                "rights_evidence_sha256": "2" * 64,
                "rights_evidence_url": "https://example.invalid/review/abc123",
                "rights_review_status": "approved", "source_id": source_id,
            }, sort_keys=True) + "\n", encoding="utf-8")
            assembled = root / "assembled.jsonl"
            self.assertEqual(prepare_corpus.main([
                "assemble", str(fetched), str(classification), str(cache), str(assembled),
                "--reviews", str(reviews),
            ]), 0)
            record = reconstruct_corpus.read_manifest(assembled)[0]
            self.assertEqual(record["format"], reconstruct_corpus.FORMAT_V2)
            self.assertEqual(record["rights"]["review_status"], "approved")  # type: ignore[index]
            self.assertEqual(record["split"], "unassigned")

            proxy_record = json.loads(classification.read_text())
            proxy_record["source_sha256"] = fetched_record["sha256"]
            proxy_record["cache_filename"] = (
                "train/a/b/c/abc1234567890def.jpg"
            )
            local_source = cache / proxy_record["cache_filename"]
            local_source.parent.mkdir(parents=True)
            local_source.write_bytes((cache / fetched_record["cache_filename"]).read_bytes())
            classification.write_text(
                json.dumps(proxy_record, sort_keys=True) + "\n", encoding="utf-8"
            )
            local_assembled = root / "local-assembled.jsonl"
            self.assertEqual(prepare_corpus.main([
                "assemble", str(candidates), str(classification), str(cache),
                str(local_assembled), "--reviews", str(reviews),
            ]), 0)
            local_record = reconstruct_corpus.read_manifest(local_assembled)[0]
            self.assertEqual(local_record["sha256"], fetched_record["sha256"])
            self.assertEqual(
                local_record["original_url"],
                "https://open-images-dataset.s3.amazonaws.com/train/abc1234567890def.jpg",
            )

            proxy_record["format"] = "rawtherapee-tgmr-image-proxy-classification-v1"
            classification.write_text(
                json.dumps(proxy_record, sort_keys=True) + "\n", encoding="utf-8"
            )
            with self.assertRaises(prepare_corpus.CorpusPreparationError):
                prepare_corpus.main([
                    "assemble", str(fetched), str(classification), str(cache),
                    str(root / "proxy-assembled.jsonl"), "--reviews", str(reviews),
                ])

            migrated_cache = root / "migrated-cache"
            migrated_cache.mkdir()
            legacy = migrated_cache / prepare_corpus._legacy_candidate_cache_filename(first)
            legacy.write_bytes(data)
            migrated_fetched = root / "migrated-fetched.jsonl"
            migrated_report = root / "migrated-fetch-report.json"
            self.assertEqual(prepare_corpus.main([
                "fetch", str(candidates), str(migrated_cache), str(migrated_fetched),
                "--retry", "0", "--report", str(migrated_report),
            ]), 0)
            self.assertFalse(legacy.exists())
            self.assertEqual(
                json.loads(migrated_report.read_text())["results"][0]["status"],
                "migrated-cache",
            )

    def test_fetch_honors_http_retry_after(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = {
                "advertised_checksum": None,
                "catalog": "wikimedia-commons",
                "format": prepare_corpus.CANDIDATE_FORMAT,
                "original_url": "https://upload.wikimedia.org/fixture.jpg",
                "upstream_source_id": "fixture",
            }
            candidates = root / "candidates.jsonl"
            prepare_corpus.write_jsonl(candidates, [candidate], False)
            throttled = urllib.error.HTTPError(
                candidate["original_url"], 429, "rate limited",
                {"Retry-After": "2"}, io.BytesIO(b"rate limited"),
            )
            payload = b"retry-after-fixture"
            with mock.patch.object(
                prepare_corpus.urllib.request, "urlopen",
                side_effect=[throttled, io.BytesIO(payload)],
            ), mock.patch.object(prepare_corpus.time, "sleep") as sleep:
                output = root / "fetched.jsonl"
                self.assertEqual(prepare_corpus.main([
                    "fetch", str(candidates), str(root / "cache"), str(output),
                    "--retry", "1",
                ]), 0)
            sleep.assert_called_once()
            self.assertAlmostEqual(sleep.call_args.args[0], 2.0, places=3)
            self.assertEqual(json.loads(output.read_text())["bytes"], len(payload))

    def test_fetch_rejects_negative_request_delay(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(
                prepare_corpus.CorpusPreparationError, "request-delay"
            ):
                prepare_corpus.main([
                    "fetch", str(root / "missing.jsonl"), str(root / "cache"),
                    str(root / "output.jsonl"), "--request-delay", "-0.1",
                ])

    def test_fetch_migrates_unhinted_cache_extension(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / "cache"
            cache.mkdir()
            candidate = {
                "advertised_checksum": None,
                "catalog": "smithsonian-open-access",
                "file_type_hint": "jpeg",
                "format": prepare_corpus.CANDIDATE_FORMAT,
                "original_url": "https://ids.si.edu/ids/download?id=fixture_screen",
                "upstream_source_id": "fixture",
            }
            candidates = root / "candidates.jsonl"
            prepare_corpus.write_jsonl(candidates, [candidate], False)
            old_path = cache / prepare_corpus._unhinted_candidate_cache_filename(candidate)
            old_path.write_bytes(b"catalog-declared-jpeg-fixture")
            output = root / "fetched.jsonl"
            report = root / "report.json"
            self.assertEqual(prepare_corpus.main([
                "fetch", str(candidates), str(cache), str(output),
                "--retry", "0", "--report", str(report),
            ]), 0)
            fetched = json.loads(output.read_text())
            self.assertTrue(fetched["cache_filename"].endswith(".jpg"))
            self.assertFalse(old_path.exists())
            self.assertEqual(json.loads(report.read_text())["results"][0]["status"],
                             "migrated-cache")

    def test_openimages_annotation_review_queue_and_decisions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rules = root / "rules.json"
            rules.write_text(json.dumps({
                "derived_rules": {"low-light": {"maximum_linear_luminance_mean": 0.08}},
                "format": "rawtherapee-tgmr-openimages-content-tags-v1",
                "label_names": {
                    "people": ["Person"], "skin-hair-clothing": ["Person"],
                },
            }, sort_keys=True) + "\n", encoding="utf-8")
            descriptions = root / "classes.csv"
            descriptions.write_text(
                "LabelName,DisplayName\n/m/person,Person\n", encoding="utf-8"
            )
            human = root / "human.csv"
            boxes = root / "boxes.csv"
            manifest = root / "pending.jsonl"
            split_counts = {"train": 0, "validation": 0, "test": 0}
            required = {"train": 770, "validation": 105, "test": 105}
            records = []
            human_rows = []
            index = 0
            while any(split_counts[name] < required[name] for name in required):
                upstream = f"{index:016x}"
                source_id = "openimages-cvdf-v5-boxable:" + upstream
                author_id = "fixture-author:" + str(index)
                split = prepare_corpus._assigned_split(
                    "rawtherapee-tgmr-corpus-v1-selection", author_id
                )
                index += 1
                if split_counts[split] >= required[split]:
                    continue
                split_counts[split] += 1
                signature = hashlib.sha256(source_id.encode()).hexdigest()
                classification = expanded_classification()
                classification.update({
                    "dhash": signature[:16], "perceptual_hash": signature[:16],
                    "phash": signature[16:32], "luminance_mean": 0.05,
                })
                records.append({
                    "advertised_checksum": None, "archive_fallbacks": [],
                    "author": f"Fixture Author {index}", "author_id": author_id,
                    "author_url": f"https://example.invalid/author/{index}",
                    "cache_filename": f"train/{upstream[0]}/{upstream[1]}/{upstream[2]}/{upstream}.jpg",
                    "catalog": {
                        "name": "openimages-cvdf-v5-boxable",
                        "revision": "fixture-openimages",
                        "snapshot_sha256": "1" * 64,
                    },
                    "classification": classification,
                    "content_tags": [], "decoded_pixel_sha256": signature,
                    "fallback_urls": [], "file_type": "jpeg",
                    "format": prepare_corpus.SOURCE_FORMAT,
                    "height": 800, "icc_identity": "assumed-srgb",
                    "landing_page": f"https://example.invalid/image/{index}",
                    "license": "CC-BY-2.0",
                    "license_url": "https://creativecommons.org/licenses/by/2.0/",
                    "orientation": 1,
                    "original_url": f"https://example.invalid/{upstream}.jpg",
                    "patch_coordinates": [], "patch_sampling_seed": "0x1",
                    "people_review_status": "pending",
                    "rights": {
                        "evidence_revision": "fixture-openimages",
                        "evidence_sha256": "1" * 64,
                        "evidence_url": "https://creativecommons.org/licenses/by/2.0/",
                        "review_status": "pending",
                    },
                    "selected": False, "selection_status": "candidate-pending-review",
                    "sha256": hashlib.sha256((source_id + ":bytes").encode()).hexdigest(),
                    "source_id": source_id, "split": "unassigned",
                    "title": f"Fixture {index}", "upstream_flickr_id": None,
                    "upstream_source_id": upstream, "width": 1200,
                })
                human_rows.append([upstream, "verification", "/m/person", "1"])
            records[1]["classification"]["dhash"] = "0000000000000000"
            records[1]["classification"]["perceptual_hash"] = "0000000000000000"
            records[1]["classification"]["phash"] = "0000000000000000"
            records[2]["classification"]["dhash"] = "000000000000003f"
            records[2]["classification"]["perceptual_hash"] = "000000000000003f"
            records[2]["classification"]["phash"] = "ffffffffffffffff"
            records[0]["author_url"] = ""
            human_rows.append([
                records[0]["upstream_source_id"], "verification", "/m/person", "0",
            ])
            prepare_corpus.write_jsonl(manifest, records, False)
            with human.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.writer(stream)
                writer.writerow(["ImageID", "Source", "LabelName", "Confidence"])
                writer.writerows(human_rows)
            boxes.write_text(
                "ImageID,LabelName\n"
                + str(records[1]["upstream_source_id"]) + ",/m/person\n",
                encoding="utf-8",
            )

            reviews = root / "auto-reviews.jsonl"
            queue = root / "people-queue.jsonl"
            report = root / "review-report.json"
            page = root / "people-review.html"
            arguments = [
                "prepare-openimages-review", str(manifest), str(descriptions),
                str(human), str(boxes), str(root / "cache"), str(reviews),
                str(queue), str(report), "--rules", str(rules),
                "--class-descriptions-sha256", prepare_corpus.sha256_file(descriptions)[0],
                "--human-labels-sha256", prepare_corpus.sha256_file(human)[0],
                "--boxes-sha256", prepare_corpus.sha256_file(boxes)[0],
                "--rules-sha256", prepare_corpus.sha256_file(rules)[0],
                "--html", str(page),
            ]
            self.assertEqual(prepare_corpus.main(arguments), 0)
            review_values = list(prepare_corpus._jsonl(reviews))
            queue_values = list(prepare_corpus._jsonl(queue))
            self.assertEqual(len(review_values), len(records))
            self.assertEqual(sum(value["rights_review_status"] == "approved"
                                 for value in review_values), len(records) - 1)
            self.assertEqual(sum(value["rights_review_status"] == "pending"
                                 for value in review_values), 1)
            self.assertTrue(all(value["content_tags"] == [
                "low-light", "people", "skin-hair-clothing",
            ] for value in review_values))
            self.assertEqual(
                {name: sum(value["split"] == name for value in queue_values)
                 for name in required},
                {"train": 750, "validation": 94, "test": 94},
            )
            self.assertTrue(any(value["annotation_sources"] == [
                "human-image-label", "object-box",
            ] for value in queue_values))
            page_text = page.read_text(encoding="utf-8")
            self.assertTrue(page_text.startswith("<!doctype html>"))
            self.assertIn("Unchecked images are approved.", page_text)
            self.assertIn("rejected?'rejected':'approved-no-minors-or-sensitive-content'", page_text)
            self.assertIn("navigator.clipboard.writeText(text)", page_text)
            self.assertIn("the JSONL is selected below", page_text)
            self.assertIn("classList.toggle('rejected',input.checked)", page_text)
            self.assertIn("toggleAttribute('checked',input.checked)", page_text)
            # This HTML is assembled from a Python triple-quoted string.  A
            # single ``\n`` escape here would become a literal newline inside
            # the JavaScript string and prevent the entire script from parsing.
            self.assertIn("return lines.join('\\n')+'\\n';", page_text)
            self.assertNotIn("return lines.join('\n')+'\n';", page_text)
            self.assertNotIn('value="approved-no-minors-or-sensitive-content"', page_text)

            second_reviews = root / "auto-reviews-second.jsonl"
            second_queue = root / "people-queue-second.jsonl"
            second_report = root / "review-report-second.json"
            second_arguments = list(arguments)
            second_arguments[6] = str(second_reviews)
            second_arguments[7] = str(second_queue)
            second_arguments[8] = str(second_report)
            second_arguments[-1] = str(root / "people-review-second.html")
            self.assertEqual(prepare_corpus.main(second_arguments), 0)
            self.assertEqual(reviews.read_bytes(), second_reviews.read_bytes())
            self.assertEqual(queue.read_bytes(), second_queue.read_bytes())

            decisions = root / "people-decisions.jsonl"
            prepare_corpus.write_jsonl(decisions, ({
                "format": prepare_corpus.OPENIMAGES_PEOPLE_DECISION_FORMAT,
                "people_review_status": "approved-no-minors-or-sensitive-content",
                "source_id": value["source_id"],
            } for value in queue_values), False)
            applied = root / "reviewed.jsonl"
            self.assertEqual(prepare_corpus.main([
                "apply-openimages-people-decisions", str(reviews), str(queue),
                str(decisions), str(applied), "--require-complete",
            ]), 0)
            approved = {value["source_id"] for value in queue_values}
            self.assertEqual(sum(
                value["people_review_status"] == "approved-no-minors-or-sensitive-content"
                for value in prepare_corpus._jsonl(applied)
            ), len(approved))

            duplicate_input = root / "duplicate-input.jsonl"
            duplicate_records = []
            for value in records:
                updated = dict(value)
                updated["rights"] = dict(value["rights"])
                updated["rights"]["review_status"] = "approved"
                updated["people_review_status"] = (
                    "approved-no-minors-or-sensitive-content"
                )
                updated["selection_status"] = "candidate-reviewed"
                duplicate_records.append(updated)
            duplicate_records[0]["people_review_status"] = "rejected"
            prepare_corpus.write_jsonl(duplicate_input, duplicate_records, False)
            duplicate_pending = root / "duplicate-pending.jsonl"
            duplicate_queue = root / "duplicate-queue.jsonl"
            duplicate_report = root / "duplicate-report.json"
            duplicate_html = root / "duplicate-review.html"
            self.assertEqual(prepare_corpus.main([
                "prepare-duplicate-review", str(duplicate_input), str(root / "cache"),
                str(duplicate_pending), str(duplicate_queue),
                "--report", str(duplicate_report), "--html", str(duplicate_html),
            ]), 0)
            duplicate_values = list(prepare_corpus._jsonl(duplicate_queue))
            target_ids = {records[1]["source_id"], records[2]["source_id"]}
            target_cluster = next(value for value in duplicate_values if target_ids.issubset({
                member["source_id"] for member in value["members"]
            }))
            target_pair = next(value for value in target_cluster["pairs"] if {
                value["left_source_id"], value["right_source_id"],
            } == target_ids)
            self.assertEqual(target_pair["dhash_distance"], 6)
            self.assertEqual(
                json.loads(duplicate_report.read_text())["borderline_dhash_hamming_max"], 7
            )
            self.assertEqual(
                json.loads(duplicate_report.read_text())["candidates_after_author_cap"],
                len(duplicate_records) - 1,
            )
            duplicate_page = duplicate_html.read_text(encoding="utf-8")
            self.assertTrue(duplicate_page.startswith("<!doctype html>"))
            self.assertIn("navigator.clipboard.writeText(text)", duplicate_page)
            self.assertIn("classList.toggle('rejected',input.checked)", duplicate_page)
            self.assertIn("toggleAttribute('checked',input.checked)", duplicate_page)
            self.assertIn("return lines.join('\\n')+(lines.length?'\\n':'');", duplicate_page)
            self.assertNotIn("return lines.join('\n')+(lines.length?'\n':'');", duplicate_page)
            duplicate_pending_second = root / "duplicate-pending-second.jsonl"
            duplicate_queue_second = root / "duplicate-queue-second.jsonl"
            self.assertEqual(prepare_corpus.main([
                "prepare-duplicate-review", str(duplicate_input), str(root / "cache"),
                str(duplicate_pending_second), str(duplicate_queue_second),
            ]), 0)
            self.assertEqual(duplicate_pending.read_bytes(), duplicate_pending_second.read_bytes())
            self.assertEqual(duplicate_queue.read_bytes(), duplicate_queue_second.read_bytes())
            empty_decisions = root / "empty-decisions.jsonl"
            empty_decisions.write_text("", encoding="utf-8")
            with self.assertRaises(prepare_corpus.CorpusPreparationError):
                prepare_corpus.main([
                    "apply-duplicate-decisions", str(duplicate_pending),
                    str(duplicate_queue), str(empty_decisions),
                    str(root / "incomplete.jsonl"), "--require-complete",
                ])
            conservative_output = root / "conservative-reviewed.jsonl"
            self.assertEqual(prepare_corpus.main([
                "apply-duplicate-decisions", str(duplicate_pending),
                str(duplicate_queue), str(empty_decisions),
                str(conservative_output), "--reject-unreviewed-clusters",
            ]), 0)
            conservative_by_id = {
                value["source_id"]: value
                for value in prepare_corpus._jsonl(conservative_output)
            }
            involved_ids = {
                member["source_id"]
                for cluster in duplicate_values for member in cluster["members"]
            }
            self.assertTrue(all(
                conservative_by_id[source_id]["selection_status"]
                == "candidate-rejected-duplicate"
                for source_id in involved_ids
            ))
            decisions_values = []
            for value in duplicate_values:
                rejected = []
                if value["cluster_id"] == target_cluster["cluster_id"]:
                    rejected = [target_pair["right_source_id"]]
                decisions_values.append({
                    "cluster_id": value["cluster_id"],
                    "duplicate_review_status": "resolved",
                    "format": prepare_corpus.DUPLICATE_DECISION_FORMAT,
                    "reject_source_ids": rejected,
                })
            duplicate_decisions = root / "duplicate-decisions.jsonl"
            prepare_corpus.write_jsonl(duplicate_decisions, decisions_values, False)
            duplicate_output = root / "duplicate-reviewed.jsonl"
            self.assertEqual(prepare_corpus.main([
                "apply-duplicate-decisions", str(duplicate_pending),
                str(duplicate_queue), str(duplicate_decisions),
                str(duplicate_output), "--require-complete",
            ]), 0)
            output_by_id = {
                value["source_id"]: value
                for value in prepare_corpus._jsonl(duplicate_output)
            }
            self.assertEqual(
                output_by_id[target_pair["right_source_id"]]["selection_status"],
                "candidate-rejected-duplicate",
            )

    def test_openimages_normalize_incremental_start(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata = root / "openimages.csv"
            with metadata.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=[
                    "ImageID", "OriginalURL", "OriginalLandingURL", "License", "Author",
                ])
                writer.writeheader()
                for index in range(5):
                    writer.writerow({
                        "ImageID": f"{index:016x}",
                        "OriginalURL": f"https://example.invalid/{index}.jpg",
                        "OriginalLandingURL": f"https://example.invalid/image/{index}",
                        "License": "https://creativecommons.org/licenses/by/2.0/",
                        "Author": f"Author {index}",
                    })
            output = root / "increment.jsonl"
            self.assertEqual(prepare_corpus.main([
                "normalize", "openimages", str(metadata), str(output),
                "--revision", "fixture", "--eligible-only", "--start", "2", "--limit", "2",
            ]), 0)
            self.assertEqual(
                [value["upstream_source_id"] for value in prepare_corpus._jsonl(output)],
                ["0000000000000002", "0000000000000003"],
            )

    def test_pass_commons_and_smithsonian_normalizers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata = root / "pass.csv"
            metadata.write_text(
                ",unickname,datetaken,licensename,hash,latitude,longitude\n"
                "3,example,2020-01-01,Attribution License,"
                "21657882a4c879e3d08bf7eb5997451,,\n",
                encoding="utf-8",
            )
            urls = root / "pass-urls.txt"
            urls.write_text("https://example.invalid/pass.jpg\n", encoding="utf-8")
            pass_output = root / "pass.jsonl"
            prepare_corpus.main([
                "normalize", "pass", str(metadata), str(pass_output),
                "--revision", "3.0", "--urls", str(urls),
            ])
            pass_value = json.loads(pass_output.read_text())
            self.assertEqual(pass_value["license"], "CC-BY-4.0")
            self.assertEqual(pass_value["author_id"], "flickr-user:example")
            self.assertEqual(
                pass_value["upstream_source_id"],
                "21657882a4c879e3d08bf7eb5997451",
            )
            self.assertIsNone(pass_value["advertised_checksum"])

            commons_snapshot = root / "commons.jsonl"
            commons_snapshot.write_text(json.dumps({
                "imageinfo": {
                    "descriptionurl": "https://commons.wikimedia.org/wiki/File:Fixture.jpg",
                    "extmetadata": {
                        "Artist": {"value": "Example"},
                        "LicenseShortName": {"value": "CC BY 4.0"},
                        "LicenseUrl": {"value": "https://creativecommons.org/licenses/by/4.0/"},
                    },
                    "sha1": "a" * 40, "url": "https://upload.wikimedia.org/fixture.jpg",
                    "user": "Example", "userid": 42,
                },
                "pageid": 7, "title": "File:Fixture.jpg",
            }, sort_keys=True) + "\n", encoding="utf-8")
            commons_output = root / "commons-out.jsonl"
            prepare_corpus.main([
                "normalize", "commons", str(commons_snapshot), str(commons_output),
                "--revision", "fixture-revision",
            ])
            commons_value = json.loads(commons_output.read_text())
            self.assertEqual(commons_value["license"], "CC-BY-4.0")
            self.assertEqual(commons_value["author_id"], "commons-user:42")
            self.assertFalse(prepare_corpus._catalog_dimension_eligible(640, 480))
            self.assertTrue(prepare_corpus._catalog_dimension_eligible(1200, 800))
            self.assertIsNone(prepare_corpus._catalog_dimension_eligible(None, None))

            smithsonian_snapshot = root / "smithsonian.jsonl"
            smithsonian_snapshot.write_text(json.dumps({
                "author": "Smithsonian Institution", "id": "edan-1",
                "media": {"url": "https://ids.si.edu/fixture.jpg", "usage": "CC0"},
                "record_url": "https://www.si.edu/object/fixture", "title": "Fixture",
                "unit_code": "NMNH",
            }, sort_keys=True) + "\n", encoding="utf-8")
            smithsonian_output = root / "smithsonian-out.jsonl"
            prepare_corpus.main([
                "normalize", "smithsonian", str(smithsonian_snapshot), str(smithsonian_output),
                "--revision", "fixture-revision",
            ])
            self.assertEqual(json.loads(smithsonian_output.read_text())["license"], "CC0-1.0")

    def test_catalog_content_tag_title_guard(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            rules_path = Path(directory) / "rules.json"
            rules_path.write_text(json.dumps({
                "format": "rawtherapee-tgmr-catalog-content-tag-rules-v1",
                "rules": [{
                    "patterns": ["quality images of astronomy"],
                    "tags": ["astronomy-star-field"],
                    "title_patterns": ["milky way", "nebula", "perseid"],
                }],
            }, sort_keys=True) + "\n", encoding="utf-8")
            rules, digest = prepare_corpus._content_tag_rules(rules_path)
            self.assertEqual(digest, prepare_corpus.sha256_file(rules_path)[0])
            categories = ["Category:Quality images of astronomy"]
            self.assertEqual(prepare_corpus._tags_for_categories(
                categories, rules, "File:View of the Milky Way.jpg"
            ), ["astronomy-star-field"])
            self.assertEqual(prepare_corpus._tags_for_categories(
                categories, rules, "File:Panorama of the Moon.tif"
            ), [])
            self.assertEqual(prepare_corpus._tags_for_categories(
                ["Category:Quality night photography"], rules,
                "File:View of the Milky Way.jpg",
            ), [])

    def test_catalog_content_tag_title_terms_use_word_boundaries(self) -> None:
        rules = [{
            "patterns": ["category:people"],
            "tags": ["people", "skin-hair-clothing"],
            "title_terms": ["man", "self portrait"],
        }]
        self.assertEqual(prepare_corpus._tags_for_categories(
            [], rules, "File:Portrait of a man.jpg"
        ), ["people", "skin-hair-clothing"])
        self.assertEqual(prepare_corpus._tags_for_categories(
            [], rules, "File:Painted self-portrait.jpg"
        ), ["people", "skin-hair-clothing"])
        self.assertEqual(prepare_corpus._tags_for_categories(
            [], rules, "File:Humanity monument.jpg"
        ), [])

    def test_commons_category_enrichment_handles_continuation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            snapshot = root / "snapshot.jsonl"
            snapshot.write_text("".join(json.dumps({
                "catalog_categories": ["Category:Root"], "content_tags": [],
                "imageinfo": {}, "pageid": page_id, "title": f"File:{page_id}.jpg",
            }, sort_keys=True) + "\n" for page_id in (7, 8)), encoding="utf-8")
            calls = []
            old_api = prepare_corpus._commons_api_json
            try:
                def api(parameters):
                    calls.append(dict(parameters))
                    if len(calls) == 1:
                        return {
                            "continue": {"clcontinue": "7|next", "continue": "||"},
                            "query": {"pages": [
                                {"categories": [{"title": "Category:Portraits"}], "pageid": 7},
                                {"categories": [{"title": "Category:Trees"}], "pageid": 8},
                            ]},
                        }
                    return {"query": {"pages": [{
                        "categories": [{"title": "Category:Men"}], "pageid": 7,
                    }]}}
                prepare_corpus._commons_api_json = api
                output = root / "enriched.jsonl"
                report = root / "report.json"
                self.assertEqual(prepare_corpus.main([
                    "enrich-commons-categories", str(snapshot), str(output),
                    "--report", str(report),
                ]), 0)
            finally:
                prepare_corpus._commons_api_json = old_api
            records = [json.loads(line) for line in output.read_text().splitlines()]
            self.assertEqual(records[0]["file_categories"], [
                "Category:Men", "Category:Portraits",
            ])
            self.assertEqual(records[1]["file_categories"], ["Category:Trees"])
            self.assertEqual(json.loads(report.read_text())["categories"], 3)
            self.assertEqual(calls[0]["clshow"], "!hidden")

    def test_commons_category_snapshot_is_filtered_and_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe = root / "recipe.json"
            recipe.write_text(json.dumps({
                "api": prepare_corpus.COMMONS_API,
                "candidate_author_cap": 10,
                "format": "rawtherapee-tgmr-commons-category-recipe-v1",
                "roots": [{
                    "category": "Category:Quality fixture",
                    "content_tags": ["textile-print"],
                    "max_categories": 1,
                    "max_depth": 0,
                    "max_files": 2,
                    "max_members_per_category": 10,
                }],
                "seed": "fixture-seed",
                "target_records": 1,
            }, sort_keys=True), encoding="utf-8")
            old_members = prepare_corpus._commons_category_members
            old_api = prepare_corpus._commons_api_json
            try:
                prepare_corpus._commons_category_members = lambda category, limit: [{
                    "pageid": 7, "title": "File:Fixture.jpg", "type": "file",
                }]
                prepare_corpus._commons_api_json = lambda parameters: {
                    "query": {"pages": [{
                        "imageinfo": [{
                            "descriptionurl": "https://commons.wikimedia.org/wiki/File:Fixture.jpg",
                            "extmetadata": {
                                "Artist": {"value": "Fixture Author"},
                                "LicenseShortName": {"value": "CC BY 4.0"},
                                "LicenseUrl": {"value": "https://creativecommons.org/licenses/by/4.0/"},
                            },
                            "height": 900, "mime": "image/jpeg", "sha1": "a" * 40,
                            "url": "https://upload.wikimedia.org/fixture.jpg",
                            "user": "FixtureUser", "userid": 42, "width": 1200,
                        }],
                        "pageid": 7, "title": "File:Fixture.jpg",
                    }]},
                }
                snapshot = root / "snapshot.jsonl"
                report = root / "report.json"
                self.assertEqual(prepare_corpus.main([
                    "collect-commons-categories", str(recipe), str(snapshot),
                    "--report", str(report),
                ]), 0)
            finally:
                prepare_corpus._commons_category_members = old_members
                prepare_corpus._commons_api_json = old_api
            value = json.loads(snapshot.read_text(encoding="utf-8"))
            self.assertTrue(json.loads(report.read_text(encoding="utf-8"))["published"])
            self.assertEqual(value["content_tags"], ["textile-print"])
            self.assertEqual(value["catalog_categories"], ["Category:Quality fixture"])
            normalized = root / "candidates.jsonl"
            self.assertEqual(prepare_corpus.main([
                "normalize", "commons", str(snapshot), str(normalized),
                "--revision", "fixture-snapshot",
            ]), 0)
            candidate = json.loads(normalized.read_text(encoding="utf-8"))
            self.assertEqual(candidate["rights_review_status"], "approved")
            self.assertEqual(candidate["content_tags"], ["textile-print"])

    def test_commons_underfilled_snapshot_is_not_published(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe = root / "recipe.json"
            recipe.write_text(json.dumps({
                "api": prepare_corpus.COMMONS_API,
                "candidate_author_cap": 10,
                "format": "rawtherapee-tgmr-commons-category-recipe-v1",
                "roots": [{
                    "category": "Category:Underfilled fixture",
                    "content_tags": [],
                    "max_categories": 1,
                    "max_depth": 0,
                    "max_files": 1,
                    "max_members_per_category": 10,
                }],
                "seed": "underfilled-fixture-seed",
                "target_records": 2,
            }, sort_keys=True), encoding="utf-8")
            old_members = prepare_corpus._commons_category_members
            old_api = prepare_corpus._commons_api_json
            try:
                prepare_corpus._commons_category_members = lambda category, limit: [{
                    "pageid": 7, "title": "File:Only.jpg", "type": "file",
                }]
                prepare_corpus._commons_api_json = lambda parameters: {
                    "query": {"pages": [{
                        "imageinfo": [{
                            "descriptionurl": "https://commons.wikimedia.org/wiki/File:Only.jpg",
                            "extmetadata": {
                                "Artist": {"value": "Only Author"},
                                "LicenseShortName": {"value": "CC BY 4.0"},
                                "LicenseUrl": {"value": "https://creativecommons.org/licenses/by/4.0/"},
                            },
                            "height": 900, "mime": "image/jpeg", "sha1": "a" * 40,
                            "url": "https://upload.wikimedia.org/only.jpg",
                            "user": "OnlyUser", "userid": 42, "width": 1200,
                        }],
                        "pageid": 7, "title": "File:Only.jpg",
                    }]},
                }
                snapshot = root / "snapshot.jsonl"
                report = root / "report.json"
                self.assertEqual(prepare_corpus.main([
                    "collect-commons-categories", str(recipe), str(snapshot),
                    "--report", str(report),
                ]), 1)
            finally:
                prepare_corpus._commons_category_members = old_members
                prepare_corpus._commons_api_json = old_api
            self.assertFalse(snapshot.exists())
            report_value = json.loads(report.read_text(encoding="utf-8"))
            self.assertFalse(report_value["published"])
            self.assertEqual(report_value["records"], 1)

    def test_smithsonian_aws_collector_needs_no_api_key(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            unit = root / "nmnhento"
            unit.mkdir()
            shard = unit / "00.txt"
            eligible = {
                "content": {
                    "descriptiveNonRepeating": {
                        "metadata_usage": {"access": "CC0"},
                        "online_media": {"media": [{
                            "content": "https://ids.si.edu/ids/deliveryService/id/fallback",
                            "resources": [
                                {
                                    "height": 480, "label": "Screen Image",
                                    "url": "https://ids.si.edu/screen.jpg", "width": 640,
                                },
                                {
                                    "height": 9108, "label": "High-resolution JPEG",
                                    "url": "https://ids.si.edu/ids/download?id=fixture_screen",
                                    "width": 11608,
                                },
                            ],
                            "type": "Images", "usage": {"access": "CC0"},
                        }]},
                        "record_link": "https://www.si.edu/object/fixture",
                        "unit_code": "NMNHENTO",
                    },
                    "freetext": {
                        "name": [{"content": "Fixture Collector", "label": "Collector"}],
                    },
                    "indexedStructured": {
                        "online_media_type": ["Images"],
                        "scientific_name": ["Fixture example"],
                        "tax_class": ["Insecta"],
                        "topic": ["Insects"],
                    },
                },
                "id": "edanmdm-fixture-1", "title": "Fixture insect",
            }
            rejected = json.loads(json.dumps(eligible))
            rejected["id"] = "edanmdm-fixture-2"
            rejected["content"]["descriptiveNonRepeating"]["metadata_usage"]["access"] = "Usage conditions apply"
            shard.write_text(
                json.dumps(eligible, separators=(",", ":")) + "\n"
                + json.dumps(rejected, separators=(",", ":")) + "\n",
                encoding="utf-8",
            )
            unit_index = unit / "index.txt"
            unit_index.write_text(shard.as_uri() + "\n", encoding="utf-8")
            root_index = root / "index.txt"
            root_index.write_text(unit_index.as_uri() + "\n", encoding="utf-8")

            output = root / "smithsonian-aws.jsonl"
            report = root / "smithsonian-aws-report.json"
            snapshot_dir = root / "frozen-smithsonian-metadata"
            self.assertEqual(prepare_corpus.main([
                "collect-smithsonian", str(output), "--unit", "nmnhento",
                "--prefix", "00", "--index-url", root_index.as_uri(),
                "--snapshot-dir", str(snapshot_dir), "--report", str(report),
            ]), 0)
            records = list(prepare_corpus._jsonl(output))
            self.assertEqual(len(records), 1)
            self.assertEqual(
                records[0]["media"]["url"],  # type: ignore[index]
                "https://ids.si.edu/ids/download?id=fixture_screen",
            )
            self.assertEqual(records[0]["media"]["resource_label"], "High-resolution JPEG")  # type: ignore[index]
            self.assertEqual(
                records[0]["catalog_categories"],
                ["Fixture example", "Images", "Insecta", "Insects"],
            )
            self.assertEqual(records[0]["author"], "Smithsonian Institution")
            self.assertEqual(records[0]["author_id"], "smithsonian-record:edanmdm-fixture-1")
            normalized = root / "smithsonian-candidates.jsonl"
            self.assertEqual(prepare_corpus.main([
                "normalize", "smithsonian", str(output), str(normalized),
                "--revision", "fixture-aws-snapshot",
            ]), 0)
            candidate = json.loads(normalized.read_text(encoding="utf-8"))
            self.assertEqual(
                candidate["author_id"], "smithsonian-record:edanmdm-fixture-1"
            )
            self.assertEqual(candidate["file_type_hint"], "jpeg")
            self.assertTrue(prepare_corpus.candidate_cache_filename(candidate).endswith(".jpg"))

            report_value = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(report_value["records"], 1)
            self.assertEqual(report_value["units"][0]["shards"][0]["records"], 2)
            self.assertEqual(report_value["units"][0]["shards"][0]["eligible_records"], 1)
            self.assertEqual((snapshot_dir / "index.txt").read_bytes(), root_index.read_bytes())
            self.assertEqual(
                (snapshot_dir / "nmnhento" / "index.txt").read_bytes(),
                unit_index.read_bytes(),
            )
            self.assertEqual(
                (snapshot_dir / "nmnhento" / "00.txt").read_bytes(), shard.read_bytes()
            )

            # Offline replay must use only the frozen bytes, even after the live
            # fixture paths have changed, and reproduce both artifacts exactly.
            root_index.write_text("https://example.invalid/changed/index.txt\n", encoding="utf-8")
            unit_index.write_text("https://example.invalid/changed/ff.txt\n", encoding="utf-8")
            shard.write_text("{}\n", encoding="utf-8")
            offline_output = root / "smithsonian-offline.jsonl"
            offline_report = root / "smithsonian-offline-report.json"
            self.assertEqual(prepare_corpus.main([
                "collect-smithsonian", str(offline_output), "--unit", "nmnhento",
                "--prefix", "00", "--index-url", root_index.as_uri(),
                "--snapshot-dir", str(snapshot_dir), "--offline-snapshot",
                "--report", str(offline_report),
            ]), 0)
            self.assertEqual(offline_output.read_bytes(), output.read_bytes())
            self.assertEqual(offline_report.read_bytes(), report.read_bytes())

    def test_smithsonian_collector_rejects_missing_landing_page(self) -> None:
        record = {
            "id": "edanmdm-fixture-missing-link",
            "title": "No stable landing page",
            "content": {
                "descriptiveNonRepeating": {
                    "metadata_usage": {"access": "CC0"},
                    "online_media": {"media": [{
                        "type": "Images",
                        "usage": {"access": "CC0"},
                        "resources": [{
                            "label": "High-resolution JPEG",
                            "url": "https://ids.si.edu/ids/download?id=fixture.jpg",
                            "width": 1200,
                            "height": 900,
                        }],
                    }]},
                    "record_ID": "fixture-missing-link",
                }
            },
        }
        self.assertIsNone(prepare_corpus._smithsonian_record(record))

    def test_generic_people_review_exports_batch_approval_after_attestation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "sources.jsonl"
            source_id = "smithsonian-open-access:person-fixture"
            manifest.write_text(json.dumps({
                "author": "Fixture Artist", "author_id": "fixture-artist",
                "cache_filename": "fixture.jpg",
                "content_tags": ["people", "skin-hair-clothing"],
                "format": prepare_corpus.SOURCE_FORMAT,
                "landing_page": "https://www.si.edu/object/fixture",
                "rights": {
                    "evidence_revision": "snapshot-v1",
                    "evidence_sha256": "a" * 64,
                    "evidence_url": "https://www.si.edu/object/fixture",
                    "review_status": "approved",
                },
                "source_id": source_id, "title": "Portrait",
            }, sort_keys=True) + "\n", encoding="utf-8")
            reviews = root / "reviews.jsonl"
            queue = root / "queue.jsonl"
            page = root / "review.html"
            self.assertEqual(prepare_corpus.main([
                "prepare-catalog-people-review", str(manifest), str(root),
                str(reviews), str(queue), "--html", str(page),
            ]), 0)
            self.assertEqual(
                json.loads(queue.read_text())["format"],
                prepare_corpus.PEOPLE_REVIEW_QUEUE_FORMAT,
            )
            self.assertEqual(
                json.loads(queue.read_text())["split"],
                prepare_corpus._assigned_split(
                    prepare_corpus.CORPUS_SELECTION_SEED, "fixture-artist"
                ),
            )
            self.assertEqual(
                json.loads(reviews.read_text())["people_review_status"], "pending"
            )
            page_text = page.read_text(encoding="utf-8")
            self.assertIn("navigator.clipboard.writeText(text)", page_text)
            self.assertIn("the JSONL is selected below", page_text)
            self.assertIn("toggleAttribute('checked',input.checked)", page_text)
            self.assertIn("classList.toggle('approved'", page_text)
            self.assertIn("classList.toggle('rejected'", page_text)
            self.assertIn("I reviewed every image", page_text)
            self.assertIn("return lines.join('\\n')+'\\n';", page_text)
            self.assertIn("card.querySelector('.reject').checked?'rejected'", page_text)
            self.assertNotIn("type=\"radio\"", page_text)
            decisions = root / "decisions.jsonl"
            decisions.write_text(json.dumps({
                "format": prepare_corpus.PEOPLE_DECISION_FORMAT,
                "people_review_status": "approved-no-minors-or-sensitive-content",
                "source_id": source_id,
            }, sort_keys=True) + "\n", encoding="utf-8")
            reviewed = root / "reviewed.jsonl"
            self.assertEqual(prepare_corpus.main([
                "apply-catalog-people-decisions", str(reviews), str(queue),
                str(decisions), str(reviewed), "--require-complete",
            ]), 0)
            self.assertEqual(
                json.loads(reviewed.read_text())["people_review_status"],
                "approved-no-minors-or-sensitive-content",
            )

    def test_v2_archive_member_reconstruction(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = b"authenticated PASS archive member\n"
            archive = root / "pass.tar"
            with tarfile.open(archive, "w") as container:
                info = tarfile.TarInfo("PASS/fixture.bin")
                info.size = len(data)
                info.mtime = 0
                container.addfile(info, io.BytesIO(data))
            archive_sha, _ = prepare_corpus.sha256_file(archive)
            record = {
                "advertised_checksum": None,
                "archive_fallbacks": [{
                    "member": "PASS/fixture.bin",
                    "member_sha256": hashlib.sha256(data).hexdigest(),
                    "sha256": archive_sha,
                    "url": archive.as_uri(),
                }],
                "author": "Fixture", "author_id": "fixture-author",
                "author_url": "https://example.invalid/author",
                "cache_filename": "fixture.bin",
                "catalog": {"name": "pass-v3", "revision": "3.0", "snapshot_sha256": "3" * 64},
                "classification": expanded_classification(), "content_tags": [],
                "decoded_pixel_sha256": "4" * 64, "fallback_urls": [],
                "file_type": "fixture", "format": reconstruct_corpus.FORMAT_V2,
                "height": 7, "icc_identity": "none",
                "landing_page": "https://example.invalid/fixture",
                "license": "CC-BY-4.0",
                "license_url": "https://creativecommons.org/licenses/by/4.0/",
                "orientation": 1,
                "original_url": (root / "missing.bin").as_uri(),
                "patch_coordinates": [], "patch_sampling_seed": 1,
                "people_review_status": "not-applicable",
                "rights": {
                    "evidence_revision": "fixture", "evidence_sha256": "5" * 64,
                    "evidence_url": "https://example.invalid/rights", "review_status": "approved",
                },
                "selected": True, "selection_status": "accepted-corpus-v1",
                "sha256": hashlib.sha256(data).hexdigest(),
                "source_id": "pass-v3:fixture", "split": "train", "title": "Fixture",
                "upstream_flickr_id": None, "upstream_source_id": "fixture", "width": 7,
            }
            manifest = root / "manifest.jsonl"
            manifest.write_text(json.dumps(record, sort_keys=True) + "\n", encoding="utf-8")
            cache = root / "cache"
            self.assertEqual(reconstruct_corpus.main([
                str(manifest), str(cache), "--retry", "0",
            ]), 0)
            self.assertEqual((cache / "fixture.bin").read_bytes(), data)


if __name__ == "__main__":
    unittest.main()
