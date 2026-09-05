import base64
import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest

import check_dscf0771 as guard


class PrivacyTest(unittest.TestCase):
    def setUp(self):
        self.crop = b"reviewed earring fixture"
        self.full = b"prohibited full-frame fixture"
        self.path = "devnotes/images/xtrans-neural/DSCF0771/DSCF0771-test-earring-500.png"
        self.policy = {"format": "rawtherapee-dscf0771-privacy-v1",
                       "allowed_crops": {self.path: {"sha256": hashlib.sha256(self.crop).hexdigest()}},
                       "prohibited_sha256": [hashlib.sha256(self.full).hexdigest()],
                       "prohibited_git_blobs": [], "reviewed_document_sha256": []}

    def test_reviewed_crop(self):
        guard.check_bytes(self.path, self.crop, self.policy)

    def test_changed_or_renamed_crop_needs_review(self):
        for path, data in [(self.path, b"changed"), (self.path.replace("test", "new"), self.crop)]:
            with self.assertRaises(guard.PrivacyError):
                guard.check_bytes(path, data, self.policy)

    def test_prohibited_blob_under_unrelated_name(self):
        with self.assertRaisesRegex(guard.PrivacyError, "full-frame"):
            guard.check_bytes("unrelated.png", self.full, self.policy)

    def test_raw_and_full_frame_names(self):
        for path in ("DSCF0771.RAF", "dscf0771.tiff", "DSCF0771-full-third.png", "DSCF0771.zip"):
            with self.assertRaises(guard.PrivacyError):
                guard.check_bytes(path, b"new bytes", self.policy)

    def test_embedded_copy(self):
        data = b'<img src="data:image/png;base64,' + base64.b64encode(self.full) + b'">'
        with self.assertRaisesRegex(guard.PrivacyError, "embedded"):
            guard.check_bytes("report.html", data, self.policy)

    def test_pdf_review_is_identity_bound(self):
        path, data = "doc/papers/test.pdf", b"opaque PDF"
        with self.assertRaisesRegex(guard.PrivacyError, "review"):
            guard.check_bytes(path, data, self.policy)
        self.policy["reviewed_document_sha256"] = [hashlib.sha256(data).hexdigest()]
        guard.check_bytes(path, data, self.policy)

    def test_numerical_provenance_is_not_an_image(self):
        guard.check_bytes("results.json", b'{"raw":"DSCF0771.RAF"}', self.policy)

    def test_manifest_forbidden_asset(self):
        path = str(Path(self.path).with_name("manifest.json"))
        with self.assertRaisesRegex(guard.PrivacyError, "advertises"):
            guard.check_asset_manifest(path, b'{"assets":[{"filename":"DSCF0771-full-third.png"}]}', self.policy)

    def test_delete_commit_does_not_hide_history(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            def git(*args):
                return subprocess.check_output(["git", "-C", directory, *args], stderr=subprocess.DEVNULL)
            git("init", "-q")
            git("config", "user.name", "Privacy test")
            git("config", "user.email", "privacy@example.invalid")
            (repo / "innocent.png").write_bytes(self.full)
            git("add", "innocent.png"); git("commit", "-qm", "add")
            self.policy["prohibited_git_blobs"] = [git("rev-parse", "HEAD:innocent.png").decode().strip()]
            git("rm", "-q", "innocent.png"); git("commit", "-qm", "delete")
            self.assertTrue(guard.audit(repo, self.policy)["pass"])
            self.assertFalse(guard.audit(repo, self.policy, history=True)["pass"])

    def test_deleted_embedded_copy_is_still_detected(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            def git(*args):
                return subprocess.check_output(["git", "-C", directory, *args], stderr=subprocess.DEVNULL)
            git("init", "-q")
            git("config", "user.name", "Privacy test")
            git("config", "user.email", "privacy@example.invalid")
            (repo / "report.md").write_bytes(b"data:image/png;base64," + base64.b64encode(self.full))
            git("add", "report.md"); git("commit", "-qm", "add")
            git("rm", "-q", "report.md"); git("commit", "-qm", "delete")
            self.assertFalse(guard.audit(repo, self.policy, history=True)["pass"])


if __name__ == "__main__":
    unittest.main()
