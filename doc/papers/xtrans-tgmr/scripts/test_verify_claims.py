"""Offline regression tests for the manuscript-to-evidence binding."""

import copy
import hashlib
import unittest

import verify_claims as claims


class PaperEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.evidence = claims.load_evidence()
        cls.paper = (claims.ROOT / claims.PAPER).read_text(encoding="utf-8")

    def test_current_paper_and_evidence(self):
        claims.validate_evidence(self.evidence)
        claims.verify_manuscript(self.paper, self.evidence)

    def test_regeneration_is_idempotent(self):
        once = claims.render_blocks(self.paper, self.evidence)
        self.assertEqual(once, claims.render_blocks(once, self.evidence))

    def test_wrong_table_number_is_rejected(self):
        changed = self.paper.replace("34.063575", "36.063575", 1)
        self.assertNotEqual(changed, self.paper)
        with self.assertRaisesRegex(ValueError, "tables differ"):
            claims.verify_manuscript(changed, self.evidence)

    def test_wrong_abstract_is_rejected(self):
        changed = self.paper.replace("34.064 dB versus", "36.064 dB versus", 1)
        with self.assertRaisesRegex(ValueError, "abstract claim"):
            claims.verify_manuscript(changed, self.evidence)

    def test_missing_or_duplicate_blocks_are_rejected(self):
        marker = "<!-- evidence:production-table:start -->"
        for replacement in ("", marker + "\n" + marker):
            with self.subTest(replacement=replacement):
                with self.assertRaisesRegex(ValueError, "evidence block"):
                    claims.render_blocks(self.paper.replace(marker, replacement), self.evidence)

    def test_missing_end_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "endings"):
            claims.render_blocks(self.paper.replace("<!-- evidence:production-table:end -->", ""), self.evidence)

    def test_changed_metric_definition_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "denominator"):
            claims.verify_manuscript(self.paper.replace(r"\frac{1}{3N}", r"\frac{1}{2N}"), self.evidence)

    def test_two_channel_research_count_is_rejected(self):
        e = copy.deepcopy(self.evidence)
        e[claims.REDUCED]["selected"]["evaluation"]["test"]["metric"]["count"] = 11_520 * 2
        with self.assertRaisesRegex(ValueError, "three RGB"):
            claims.validate_evidence(e)

    def test_inconsistent_research_sse_is_rejected(self):
        e = copy.deepcopy(self.evidence)
        e[claims.REDUCED]["selected"]["evaluation"]["test"]["metric"]["sse"] *= 2
        with self.assertRaisesRegex(ValueError, "metric mismatch"):
            claims.validate_evidence(e)

    def test_duplicate_production_sources_are_rejected(self):
        e = copy.deepcopy(self.evidence)
        rows = e[claims.PRODUCTION]["source_psnr_deltas"]
        rows[1]["source_ordinal"] = rows[0]["source_ordinal"]
        with self.assertRaisesRegex(ValueError, "duplicated"):
            claims.validate_evidence(e)

    def test_model_mismatch_is_rejected(self):
        e = copy.deepcopy(self.evidence)
        e[claims.PRODUCTION]["model_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "model identities"):
            claims.validate_evidence(e)

    def test_gaussian_parent_mismatch_is_rejected(self):
        e = copy.deepcopy(self.evidence)
        e[claims.GAUSSIAN]["source_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "Student-t parent"):
            claims.validate_evidence(e)

    def test_insufficient_corpus_coverage_is_rejected(self):
        e = copy.deepcopy(self.evidence)
        e[claims.STATS]["brightness_counts"]["validation"] = [999, 30000, 33001]
        with self.assertRaisesRegex(ValueError, "coverage gate"):
            claims.validate_evidence(e)

    def test_changed_evidence_bytes_are_rejected(self):
        digest = hashlib.sha256(b"reviewed").hexdigest()
        claims.authenticate(b"reviewed", digest, "fixture")
        with self.assertRaisesRegex(ValueError, "SHA-256 changed"):
            claims.authenticate(b"changed", digest, "fixture")

    def test_gate_outcomes_are_computed(self):
        table = claims.blocks(self.evidence)["production-gates"]
        self.assertEqual(table.count("FAIL"), 2)
        self.assertIn("**408**", table)
        self.assertIn("**92**", table)
        self.assertIn("**65**", table)


if __name__ == "__main__":
    unittest.main()
