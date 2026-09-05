#!/usr/bin/env python3
"""Bind headline manuscript claims to canonical tracked experiment results."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


def load(relative: str) -> dict:
    with (ROOT / relative).open("r", encoding="utf-8") as stream:
        return json.load(stream)


def close(actual: float, expected: float, tolerance: float = 5e-10) -> None:
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=tolerance):
        raise AssertionError(f"expected {expected!r}, found {actual!r}")


def main() -> None:
    tgmr = load("devnotes/images/xtrans-tgmr/results.json")
    reduced = load("devnotes/images/xtrans-tgmr-reduce/results.json")
    native = load("devnotes/images/xtrans-tgmr-native-opt/results.json")

    selected = reduced["selected"]["evaluation"]
    close(selected["test"]["metric"]["psnr_db"], 35.05619630813631)
    close(selected["test"]["delta_vs_gmax_db"], 3.0020028800500356)
    close(selected["test"]["quality_retention"], 0.9080697701854353)
    close(selected["test"]["metric"]["p99_abs"], 0.08149449581761556)
    close(selected["external"]["worst_delta_vs_gmax_db"], -0.3143084744026794)
    close(selected["stars"]["hubble-bright"]["metric"]["psnr_db"], 15.921283365937187)
    close(selected["stars"]["nasa-hydra-starfield-bright"]["metric"]["psnr_db"], 36.447408906537824)
    close(selected["synthetic"]["tiny-star"]["delta_vs_gmax_db"], -0.27107154889296403)
    assert selected["native_center_exact"] is True

    full = tgmr["bsds_test"]["trained_t"]["methods"]["mmse"]
    close(full["psnr_db"], 35.51787731375211)
    close(full["p99_abs"], 0.0772908729004969)

    parity = native["numerical_parity"]
    close(parity["complete_evaluation_rms"], 4.293898261090749e-8, 1e-16)
    close(parity["complete_evaluation_maximum_abs"], 4.76837158203125e-7, 1e-16)
    assert parity["native_center_exact"] is True
    assert parity["deterministic_across_thread_counts"] is True
    large = native["large_streaming"][-1]
    assert large["pixels"] == 39_959_032
    close(large["median_seconds"], 5.83345102, 1e-9)
    close(large["mp_s"], 6.84998157403, 1e-11)

    expected_hashes = {
        "devnotes/images/xtrans-tgmr/results.json": "cd8156286cbd7f7f0135cdf11b1caa6ba060f7ea068db35ee54c6516512d5c4a",
        "devnotes/images/xtrans-tgmr-reduce/results.json": "daca02cdd66da7706a18be7fe4ce7ab649b006c555cfca9b94b465c0a606babc",
        "devnotes/images/xtrans-tgmr-native-opt/results.json": "d4bedf16d0258c0101fddc536d43e06f0adf02f474cb7a26a087388abe919018",
    }
    for relative, expected in expected_hashes.items():
        digest = hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
        if digest != expected:
            raise AssertionError(f"{relative}: expected {expected}, found {digest}")

    print("TGMR paper claims verified against canonical tracked artifacts")


if __name__ == "__main__":
    main()
