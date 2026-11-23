"""Offline integrity and split checks for the locally bundled release inputs."""
import collections
import hashlib
import json
from pathlib import Path
import unittest

from tools.tgmr_trainer.reconstruct_corpus import ManifestError, validate_record

ROOT = Path(__file__).resolve().parents[3]
CORPUS = ROOT / "tools/tgmr_trainer/corpus-v1"


@unittest.skipUnless((CORPUS / "release-metadata.json").is_file(),
                     "release data is optional for standalone trainer source copies")
class ReleaseTests(unittest.TestCase):
    def test_corpus_publication_preserves_model_review_boundary(self):
        metadata = json.loads((CORPUS / "release-metadata.json").read_bytes())
        publication = metadata["corpus_publication"]
        self.assertEqual(publication["status"], "published-github-release")
        self.assertEqual(publication["test_split_status"], "diagnostic")
        self.assertEqual(metadata["redistribution_review"], "pending")
        self.assertEqual(metadata["model_license_intended"], "CC-BY-4.0")
        self.assertIsNone(metadata["permanent_urls"]["zenodo"])
        base = publication["repository"] + "/releases/download/" + publication["tag"] + "/"
        for name in ("corpus-v1.jsonl.gz", "tgmr-corpus-v1.tgpc.gz", "release-manifest.json"):
            self.assertEqual(metadata["permanent_urls"][name], base + name)
        self.assertEqual(publication["release_commit"], "19b2a7ea11cf9ce215d996036523897a4213e437")
        self.assertEqual(publication["release_manifest_sha256"],
                         "bf9af6a414dc4d26a0a7c4892a1756aa5f54212bcafac78e9f4ebe0a50fcb89a")
        self.assertEqual(metadata["external_reproduction_inputs"]["tgmr-corpus-v1.tgpc.gz"],
                         {"bytes": 251806462,
                          "sha256": "e073c59d362df9ffb57e96d48acdb0d0347dc607da872bd9b9376d544532a59a"})

    def test_astronomy_tag_matches_native_manifest_contract(self):
        row = json.loads((CORPUS / "selected-sources.jsonl").read_text().splitlines()[0])
        row["content_tags"] = ["astronomy-star-field"]
        validate_record(row, 1)
        row["content_tags"] = ["unknown-tag"]
        with self.assertRaises(ManifestError):
            validate_record(row, 1)

    def test_artifact_integrity(self):
        metadata = json.loads((CORPUS / "release-metadata.json").read_bytes())
        for name, identity in metadata["artifacts"].items():
            with self.subTest(artifact=name):
                content = (ROOT / name).read_bytes()
                self.assertEqual(len(content), identity["bytes"])
                self.assertEqual(hashlib.sha256(content).hexdigest(), identity["sha256"])
        model = (ROOT / "rtdata/models/xtrans-tgmr-v2.tgmr").read_bytes()
        notice = (ROOT / "rtdata/models/xtrans-tgmr-CORPUS-NOTICE.txt").read_bytes()
        self.assertEqual(model[224:256], hashlib.sha256(notice).digest())

    def test_source_split_and_provenance(self):
        rows = [validate_record(json.loads(line), index + 1) for index, line in
                enumerate((CORPUS / "selected-sources.jsonl").read_text().splitlines())]
        self.assertEqual(len(rows), 5000)
        self.assertEqual(collections.Counter(r["split"] for r in rows),
                         {"train": 4000, "validation": 500, "test": 500})
        quotas = {"openimages-cvdf-v5-boxable": (3200, 400, 400),
                  "wikimedia-commons": (480, 60, 60),
                  "smithsonian-open-access": (320, 40, 40)}
        for catalog, counts in quotas.items():
            self.assertEqual(tuple(sum(r["catalog"]["name"] == catalog and r["split"] == s
                                       for r in rows) for s in ("train", "validation", "test")), counts)
        authors = collections.defaultdict(set)
        populations = collections.Counter()
        for row in rows:
            authors[row["author_id"]].add(row["split"])
            populations[row["author_id"]] += 1
            self.assertEqual(row["rights"]["review_status"], "approved")
            self.assertIn(row["license"], {"CC0-1.0", "CC-BY-2.0", "CC-BY-3.0", "CC-BY-4.0"})
            self.assertIn(row["people_review_status"],
                          {"not-applicable", "approved-no-minors-or-sensitive-content"})
        self.assertTrue(all(len(splits) == 1 for splits in authors.values()))
        self.assertLessEqual(max(populations.values()), 5)
        self.assertEqual(collections.Counter(r["split"] for r in rows
                         if "astronomy-star-field" in r["content_tags"]),
                         {"train": 12, "validation": 1, "test": 1})
        for field in ("source_id", "sha256", "decoded_pixel_sha256"):
            self.assertEqual(len({r[field] for r in rows}), 5000)
        order = json.loads((CORPUS / "training-order.json").read_bytes())
        self.assertEqual(order["output_manifest_sha256"],
                         hashlib.sha256((CORPUS / "selected-sources.jsonl").read_bytes()).hexdigest())
        self.assertEqual(order["ordered_source_ids"], [r["source_id"] for r in rows if r["split"] == "train"])


if __name__ == "__main__":
    unittest.main()
