from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.tgmr_trainer import reconstruct_corpus


def record(source: Path, data: bytes) -> dict[str, object]:
    return {
        "format": reconstruct_corpus.FORMAT,
        "source_id": "fixture-1",
        "split": "train",
        "selected": True,
        "selection_status": "accepted-corpus-v1",
        "original_url": source.as_uri(),
        "fallback_urls": [],
        "landing_page": "https://example.invalid/fixture-1",
        "author": "TGMR test suite",
        "title": "fixture",
        "license": "CC0-1.0",
        "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
        "advertised_checksum": None,
        "sha256": hashlib.sha256(data).hexdigest(),
        "decoded_pixel_sha256": "0" * 64,
        "cache_filename": "fixture.bin",
        "file_type": "fixture",
        "width": 7,
        "height": 7,
        "orientation": 1,
        "icc_identity": "none",
        "classification": {
            "channel_means": [0.1, 0.2, 0.3],
            "chroma_ratio_mean": 0.1,
            "clipped_black_fraction": 0.0,
            "clipped_white_fraction": 0.0,
            "gradient_rms": 0.01,
            "hue_degrees": 30.0,
            "jpeg_blockiness": 0.0,
            "laplacian_rms": 0.01,
            "local_contrast": 0.02,
            "luminance_mean": 0.2,
            "luminance_p01": 0.01,
            "luminance_p99": 0.9,
            "luminance_stddev": 0.1,
            "perceptual_hash": "0123456789abcdef",
            "saturation_mean": 0.2,
        },
        "patch_sampling_seed": 1,
        "patch_coordinates": [],
    }


class ReconstructionTests(unittest.TestCase):
    def test_download_cache_and_offline_verification(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.bin"
            data = b"authenticated TGMR corpus fixture\n"
            source.write_bytes(data)
            source_record = record(source, data)
            source_record["cache_filename"] = "a/b/c/fixture.bin"
            manifest = root / "corpus-v1.jsonl"
            manifest.write_text(
                json.dumps(source_record, sort_keys=True, separators=(",", ":")) + "\n",
                encoding="utf-8",
            )
            cache = root / "cache"
            report = root / "report.json"
            self.assertEqual(
                reconstruct_corpus.main([str(manifest), str(cache), "--retry", "0", "--report", str(report)]),
                0,
            )
            self.assertEqual((cache / "a/b/c/fixture.bin").read_bytes(), data)
            self.assertEqual(
                reconstruct_corpus.main([str(manifest), str(cache), "--offline-verify"]), 0
            )
            parsed = json.loads(report.read_text())
            self.assertEqual(parsed["results"][0]["status"], "downloaded")

    def test_changed_cache_is_not_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.bin"
            source.write_bytes(b"expected")
            manifest = root / "corpus-v1.jsonl"
            manifest.write_text(json.dumps(record(source, b"expected")) + "\n")
            cache = root / "cache"
            cache.mkdir()
            (cache / "fixture.bin").write_bytes(b"changed")
            self.assertEqual(
                reconstruct_corpus.main([str(manifest), str(cache), "--offline-verify"]), 1
            )

    def test_forbidden_selected_license_is_rejected(self) -> None:
        value = record(Path("/tmp/source"), b"x")
        value["license"] = "CC-BY-NC-4.0"
        with self.assertRaises(reconstruct_corpus.ManifestError):
            reconstruct_corpus.validate_record(value, 1)

    def test_perceptual_near_duplicate_cannot_cross_splits(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = record(root / "first.bin", b"first")
            second = record(root / "second.bin", b"second")
            second["source_id"] = "fixture-2"
            second["cache_filename"] = "fixture-2.bin"
            second["author"] = "Independent TGMR test author"
            second["split"] = "test"
            second["decoded_pixel_sha256"] = "1" * 64
            second["classification"]["perceptual_hash"] = "0123456789abcdee"  # type: ignore[index]
            manifest = root / "corpus-v1.jsonl"
            manifest.write_text(
                json.dumps(first, sort_keys=True) + "\n"
                + json.dumps(second, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            with self.assertRaises(reconstruct_corpus.ManifestError):
                reconstruct_corpus.read_manifest(manifest)


if __name__ == "__main__":
    unittest.main()
