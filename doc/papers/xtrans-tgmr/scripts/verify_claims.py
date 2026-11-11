#!/usr/bin/env python3
"""Generate/check manuscript evidence blocks without fitting or external data.

The default is read-only. --update mechanically refreshes named blocks, but
still rejects changed evidence and inconsistent abstract/metric definitions.
This checks specified claims, not the scientific validity of arbitrary prose.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[4]
PAPER = Path("doc/papers/xtrans-tgmr/paper.md")
TGMR = "devnotes/images/xtrans-tgmr/results.json"
REDUCED = "devnotes/images/xtrans-tgmr-reduce/results.json"
NATIVE = "devnotes/images/xtrans-tgmr-native-opt/results.json"
PRODUCTION = "tools/tgmr_trainer/corpus-v1/diagnostic-test-comparison.json"
MODEL = "rtdata/models/xtrans-tgmr-v2.tgmr"
MODEL_META = MODEL + ".json"
RELEASE = "tools/tgmr_trainer/corpus-v1/release-metadata.json"
STATS = "tools/tgmr_trainer/corpus-v1/statistics.json"
GAUSSIAN = "doc/papers/xtrans-tgmr/evidence/gaussian-baseline.json"
EXPECTED_HASHES = {
    GAUSSIAN: "8e490f97e665ef7fe8ef37fb3ad3dfc7247cdf1bb52f1e8c7488fa184a8a103a",
    TGMR: "cd8156286cbd7f7f0135cdf11b1caa6ba060f7ea068db35ee54c6516512d5c4a",
    REDUCED: "daca02cdd66da7706a18be7fe4ce7ab649b006c555cfca9b94b465c0a606babc",
    NATIVE: "d4bedf16d0258c0101fddc536d43e06f0adf02f474cb7a26a087388abe919018",
    PRODUCTION: "9e2b799fa829d8cac33576a113594891caf0396a1f30037f6164474aff8a6dba",
    MODEL: "5707fbd67d1998ed3bac646ecce967297a2022776821d62944a24dbbb8615285",
    MODEL_META: "d753847f9389d5dd07a04d10013f81ffe85c4cfda1dd5120f90eb3e6e46d79af",
    RELEASE: "dc48927db99f2b53b5c08bc2f70f32b61f8c0fec11c1d05461f83e504bd5deca",
    STATS: "7f85de0ceaf5e5d81d691ec3188f185bafaeef7665fcc608c89511807de11b24",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def close(actual: float, expected: float, tolerance: float = 1e-10) -> None:
    require(math.isfinite(actual) and math.isfinite(expected)
            and math.isclose(actual, expected, rel_tol=0, abs_tol=tolerance),
            f"metric mismatch: {actual!r} != {expected!r}")


def authenticate(data: bytes, expected: str, name: str) -> None:
    actual = hashlib.sha256(data).hexdigest()
    require(actual == expected, f"{name}: evidence SHA-256 changed: {actual}")


def load_evidence(root: Path = ROOT) -> dict:
    evidence = {}
    for path, expected in EXPECTED_HASHES.items():
        data = (root / path).read_bytes()
        authenticate(data, expected, path)
        if path.endswith(".json"):
            evidence[path] = json.loads(data)
    return evidence


def research_metric(metric: dict) -> None:
    require(metric["count"] == 11_520 * 3, "research count must include three RGB channels")
    close(metric["sse"] / metric["count"], metric["mse"])
    close(-10 * math.log10(metric["mse"]), metric["psnr_db"])


def validate_evidence(e: dict) -> None:
    selected = e[REDUCED]["selected"]["evaluation"]
    for metric in (e[GAUSSIAN]["metric"], selected["test"]["metric"],
                   e[TGMR]["bsds_test"]["trained_t"]["methods"]["mmse"],
                   e[TGMR]["bsds_test"]["responsibility_only"]["methods"]["mmse"]):
        research_metric(metric)
    require(e[GAUSSIAN]["source_sha256"] == e[TGMR]["parents"]["gmr_results_sha256"],
            "Gaussian excerpt is not bound to the Student-t parent")
    require(e[GAUSSIAN]["model_logical_sha256"] == e[TGMR]["stage_a"]["frozen"]["model_logical_sha256"],
            "responsibility-only comparison changed experts")
    require(selected["native_center_exact"], "research native center is not exact")
    p = e[PRODUCTION]
    require(p["sources"] == 500 and p["patches"] == 64_000,
            "production population changed")
    require(sum(p["phase_counts"]) == 64_000 and len(p["phase_counts"]) == 18,
            "balanced phase count changed")
    deltas = [row["delta"] for row in p["source_psnr_deltas"]]
    require(len(deltas) == 500 and len({r["source_ordinal"] for r in p["source_psnr_deltas"]}) == 500,
            "production source deltas incomplete or duplicated")
    close(min(deltas), p["worst_source_psnr_delta"])
    close(sorted(deltas)[len(deltas) // 2], p["median_source_psnr_delta"])
    for method in ("tgmr", "markesteijn"):
        # Production JSON serializes MSE to 12 decimal places. Do not require
        # unrounded arithmetic to match the separately serialized PSNR exactly.
        close(p[method]["mse"], 10 ** (-p[method]["psnr"] / 10), 5.1e-13)
        for rows in p[method]["strata"].values():
            require(sum(row["patches"] for row in rows) == 64_000,
                    "strata must partition, not multiply, evaluated centers")
    m = e[MODEL_META]
    require(m["artifact_sha256"] == p["model_sha256"] == EXPECTED_HASHES[MODEL],
            "production model identities differ")
    require(m["artifact_bytes"] == 6_073_768, "production model size changed")
    require(m["corpus_sha256"] == p["corpus_payload_sha256"] == e[STATS]["payload_sha256"],
            "corpus identities differ")
    require(e[STATS]["record_count"] == 1_152_000, "corpus count changed")
    require(e[STATS]["strata"] == {"brightness": [0.08, 0.65], "chroma": [0.03, 0.15],
                                    "texture_gradient_rms": [0.01, 0.05]},
            "published corpus stratum boundaries changed")
    for field in ("brightness_counts", "chroma_counts", "texture_counts"):
        for split, count in (("train", 1_024_000), ("validation", 64_000), ("test", 64_000)):
            rows = e[STATS][field][split]
            require(sum(rows) == count and min(rows) >= (10_000 if split == "train" else 1_000),
                    "published corpus coverage gate failed")
    config = m["trainer_configuration"]
    for field, value in {"components": 32, "gaussian_iterations": 10,
                         "student_iterations": 30, "degrees_of_freedom": 3,
                         "backend": "canonical", "batch_size": 4096}.items():
        require(config[field] == value, f"fitting setting changed: {field}")
    v = m["validation"]
    require(v["scalar_values"] == v["patches"] * 18 * 3,
            "all-phase validation must count three channels per phase")
    parity = e[NATIVE]["numerical_parity"]
    require(parity["native_center_exact"] and parity["deterministic_across_thread_counts"],
            "native parity invariant changed")


def table(headers: list[str], rows: list[list[str]]) -> str:
    return "\n".join(["| " + " | ".join(headers) + " |",
                      "|:--|" + "--:|" * (len(headers) - 1)]
                     + ["| " + " | ".join(row) + " |" for row in rows])


def blocks(e: dict) -> dict[str, str]:
    selected = e[REDUCED]["selected"]["evaluation"]
    p = e[PRODUCTION]
    m = e[MODEL_META]
    native = e[NATIVE]
    result = {}
    rows = []
    for label, metric in [
        ("Gaussian K64 experts and weights", e[GAUSSIAN]["metric"]),
        ("Gaussian experts, t weights ($\\nu=5$)", e[TGMR]["bsds_test"]["responsibility_only"]["methods"]["mmse"]),
        ("Fitted dense K64 Student-t ($\\nu=3$)", e[TGMR]["bsds_test"]["trained_t"]["methods"]["mmse"]),
        ("Reduced K32/S9/q8 ($\\nu=3$)", selected["test"]["metric"]),
    ]:
        rows.append([label, f'{metric["psnr_db"]:.3f}', f'{metric["p99_abs"]:.6f}'])
    gmax = selected["test"]["metric"]["psnr_db"] - selected["test"]["delta_vs_gmax_db"]
    rows.insert(0, ["GMAX linear reference", f"{gmax:.3f}", "—"])
    result["research-table"] = table(["Research configuration", "RGB PSNR (dB)", "p99 scalar absolute error"], rows)
    rows = []
    for label, obj in [("Hubble bright targets", selected["stars"]["hubble-bright"]),
                       ("Hydra bright targets", selected["stars"]["nasa-hydra-starfield-bright"]),
                       ("Tiny-star control", selected["synthetic"]["tiny-star"])]:
        rows.append([label, f'{obj["metric"]["psnr_db"]:.3f}', f'{obj["delta_vs_gmax_db"]:+.3f}'])
    result["research-safety"] = table(["Research subset", "K32/S9/q8 PSNR (dB)", "Delta to GMAX (dB)"], rows)
    rows = []
    for label, key, precision in [("Pooled RGB PSNR (dB)", "psnr", 6),
                                  ("p99 center RGB RMS", "patch_rms_p99", 6),
                                  ("Worst center RGB RMS", "worst_patch_rms", 6),
                                  ("Median source PSNR (dB)", "median_source_psnr", 6)]:
        rows.append([label] + [f'{p[method][key]:.{precision}f}' for method in ("tgmr", "markesteijn")])
    result["production-table"] = table(["Diagnostic measure", "TGMR production-v1", "Markesteijn 3-pass"], rows)
    delta = p["tgmr"]["psnr"] - p["markesteijn"]["psnr"]
    deltas = [r["delta"] for r in p["source_psnr_deltas"]]
    result["production-gates"] = (
        f'Pooled gain is **{delta:+.6f} dB**; paired median source gain is '
        f'**{p["median_source_psnr_delta"]:+.6f} dB**. TGMR wins on '
        f'**{sum(d > 0 for d in deltas)}** sources and loses on **{sum(d < 0 for d in deltas)}**. '
        f'**{sum(d < -0.5 for d in deltas)}** sources lose more than 0.5 dB; '
        f'the worst paired delta is **{min(deltas):.6f} dB**.\n\n'
        + table(["Original gate", "Required", "Outcome"], [
            ["Pooled gain", "at least +2 dB", "FAIL" if delta < 2 else "PASS"],
            ["Median paired source gain", "positive", "PASS" if p["median_source_psnr_delta"] > 0 else "FAIL"],
            ["Worst paired source loss", "at most 0.5 dB", "FAIL" if min(deltas) < -0.5 else "PASS"],
            ["Aggregate p99 RMS", "no regression", "PASS" if p["tgmr"]["patch_rms_p99"] <= p["markesteijn"]["patch_rms_p99"] else "FAIL"],
        ]))
    rows = []
    for group in ("brightness", "chroma", "texture"):
        for a, b in zip(p["tgmr"]["strata"][group], p["markesteijn"]["strata"][group]):
            require(a["level"] == b["level"] and a["patches"] == b["patches"], "stratum correspondence changed")
            rows.append([group.capitalize() + " / " + a["level"], f'{a["patches"]:,}',
                         f'{a["psnr"]:.3f}', f'{b["psnr"]:.3f}', f'{a["psnr"]-b["psnr"]:+.3f}'])
    result["strata-table"] = table(["Marginal stratum", "Centers", "TGMR (dB)", "Mark. (dB)", "Delta (dB)"], rows)
    parity = native["numerical_parity"]
    large = native["large_streaming"][-1]
    result["native-table"] = table(["Research native measurement", "Value"], [
        ["Reference/native error RMS", f'{parity["complete_evaluation_rms"]:.6e}'],
        ["Maximum absolute error", f'{parity["complete_evaluation_maximum_abs"]:.6e}'],
        ["Standalone mosaic pixels", f'{large["pixels"]:,}'],
        ["Median mosaic time (s)", f'{large["median_seconds"]:.6f}'],
        ["Throughput (megapixels/s)", f'{large["mp_s"]:.3f}'],
        ["Measured center exact / thread-repeat deterministic", "yes / yes"],
    ])
    # Split long hashes into two code lines to keep PDF margins intact. The
    # newline is typography, not part of the SHA-256 identity.
    identity_rows = [("Production-v1 TGMR v2 model", m["artifact_sha256"]),
                     ("TGPC payload (not the compressed file)", m["corpus_sha256"]),
                     ("Trainer configuration", m["trainer_configuration_sha256"]),
                     ("Trainer source/build inputs", m["trainer_revision_sha256"]),
                     ("Balanced-phase comparison JSON", EXPECTED_HASHES[PRODUCTION])]
    result["identity-block"] = (
        f'Model size: **{m["artifact_bytes"]:,} bytes**. SHA-256 values below are\n'
        'displayed as two consecutive halves; join them without whitespace.\n\n'
        + "\n\n".join(label + "\n\n```text\n" + digest[:32] + "\n" + digest[32:] + "\n```"
                        for label, digest in identity_rows))
    return result


def render_blocks(text: str, evidence: dict) -> str:
    generated = blocks(evidence)
    found = re.findall(r"<!-- evidence:([a-z-]+):start -->", text)
    require(len(found) == len(generated) and set(found) == set(generated),
            "missing, duplicate, or unknown evidence block")
    ends = re.findall(r"<!-- evidence:([a-z-]+):end -->", text)
    require(ends == found, "misordered or missing evidence block endings")
    for name, value in generated.items():
        pattern = re.compile(r"(<!-- evidence:" + name + r":start -->).*?(<!-- evidence:" + name + r":end -->)", re.S)
        text, count = pattern.subn(lambda match: match[1] + "\n\n" + value + "\n\n" + match[2], text)
        require(count == 1, f"invalid evidence block: {name}")
    return text


def verify_prose(text: str, e: dict) -> None:
    abstract = text.split("abstract: |\n", 1)[1].split("\nkeywords:", 1)[0]
    p = e[PRODUCTION]
    losses = sum(row["delta"] < -0.5 for row in p["source_psnr_deltas"])
    for phrase in [f'{p["tgmr"]["psnr"]:.3f} dB versus {p["markesteijn"]["psnr"]:.3f} dB',
                   f'{losses} sources lose more than 0.5 dB',
                   f'worst paired loss is {-p["worst_source_psnr_delta"]:.3f} dB',
                   'measured over all three center RGB channels',
                   '500 diagnostic sources']:
        require(phrase in abstract, f"abstract claim missing or inconsistent: {phrase}")
    require(r"\frac{1}{3N}" in text, "RGB MSE denominator must be 3N")
    require('not ordinary Bayesian' in text and 'tempered weights' in text,
            "tempered weighting qualification missing")
    require('It cannot detect a finite but visibly' in text,
            "runtime fallback quality limitation missing")


def verify_manuscript(text: str, evidence: dict) -> None:
    verify_prose(text, evidence)
    require(text == render_blocks(text, evidence),
            "paper tables differ from evidence; run verify_claims.py --update")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update", action="store_true", help="refresh generated manuscript blocks")
    args = parser.parse_args()
    evidence = load_evidence()
    validate_evidence(evidence)
    path = ROOT / PAPER
    text = path.read_text(encoding="utf-8")
    expected = render_blocks(text, evidence)
    verify_prose(expected, evidence)
    if args.update:
        path.write_text(expected, encoding="utf-8")
    else:
        verify_manuscript(text, evidence)
    print(f"TGMR paper: {len(EXPECTED_HASHES)} identities, metric contracts, abstract, and {len(blocks(evidence))} manuscript blocks verified")


if __name__ == "__main__":
    main()
