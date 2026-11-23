#!/usr/bin/env python3
"""Independent Linux HEIF decode checks and production CLI input workflows."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import array
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("fixtures", type=Path)
parser.add_argument("artifacts", type=Path)
args = parser.parse_args()
root = args.artifacts.resolve()
root.mkdir(parents=True, exist_ok=True)
fixtures = args.fixtures.resolve()
env = dict(os.environ, OMP_NUM_THREADS="4", RT_SETTINGS=str(root / "settings"), RT_CACHE=str(root / "cache"))
source = Path(__file__).resolve().parents[3]
settings = root / "settings"
settings.mkdir(exist_ok=True)
(settings / "options").write_bytes((source / "rtdata/options/options.lin").read_bytes())
results = []
for line in (fixtures / "manifest.tsv").read_text().splitlines():
    name, bits, width, height = line.split("\t")
    if name.startswith("orientation-") or name in ("grid.heif", "crop.heic"):
        continue  # Exact pixel placement is checked in the engine harness.
    expected = array.array("H", (fixtures / (name + ".rgb16")).read_bytes())
    raw = subprocess.check_output(["ffmpeg", "-v", "error", "-threads", "1", "-i", str(fixtures / name),
        "-frames:v", "1", "-threads", "1", "-f", "rawvideo", "-pix_fmt", "rgb48le", "-"])
    decoded = array.array("H", raw)
    assert len(expected) == len(decoded) == int(width) * int(height) * 3, name
    differences = sorted(abs(a-b) for a, b in zip(expected, decoded))
    # Independent integer YCbCr->RGB converters round differently. The 8-bit
    # libheif path also rounds RGB to eight bits before promotion by the test.
    limit = 600 if int(bits) == 8 else 256
    assert differences[-1] <= limit, (name, differences[-1])
    results.append(dict(file=name, maximum_rgb16_difference=differences[-1]))

pp3 = root / "input.pp3"
source = Path(__file__).resolve().parents[3]
pp3.write_text("[Version]\nAppVersion=5.12\nVersion=352\n[Color Management]\nInputProfile=(embedded)\nOutputProfile=file:"
    + str(source / "rtdata/iccprofiles/output/RTv2_sRGB.icc") + "\n[Metadata]\nMode=1\n")
for name, suffix, flags in (("jpeg", ".jpg", ["-j92"]), ("png", ".png", ["-n", "-b16"]),
    ("tiff", ".tif", ["-t", "-b16"]), ("heif", ".heic", ["-H", "-b12", "-Hs3"])):
    output = root / (name + suffix)
    command = [str(args.binary.resolve()), "-Y", "-o", str(output), "-p", str(pp3), *flags,
        "-c", str(fixtures / "orientation-6.heic")]
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0 and output.exists(), result.stdout + result.stderr
    metadata = json.loads(subprocess.check_output(["exiftool", "-j", "-n", str(output)], text=True))[0]
    stream = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(output)], text=True))["streams"][0]
    crop = next((data for data in stream.get("side_data_list", []) if data["side_data_type"] == "Frame Cropping"), {})
    width = stream["width"] - crop.get("crop_left", 0) - crop.get("crop_right", 0)
    height = stream["height"] - crop.get("crop_top", 0) - crop.get("crop_bottom", 0)
    assert (width, height) == (48, 64), stream
    assert metadata.get("Orientation", 1) == 1 and metadata.get("Make") == "G5 camera", metadata
    results.append(dict(workflow=name, exit_code=result.returncode))

# Real AV1 in an HEIF container must not be accepted under a HEIC suffix.
avif = root / "renamed.heic"
subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(root / "png.png"), "-frames:v", "1", "-c:v", "libaom-av1",
    "-cpu-used", "8", "-threads", "1", "-f", "avif", str(avif)], check=True)
for name, path, diagnostic in (("avif", avif, "HEVC"),
    ("hdr", fixtures / "reject-transfer-16.heic", "HDR"),
    ("bad-data", fixtures / "wrong.heic", "container")):
    output = root / ("reject-" + name + ".jpg")
    result = subprocess.run([str(args.binary.resolve()), "-o", str(output), "-j", "-c", str(path)],
        env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode != 0 and not output.exists() and diagnostic in result.stdout + result.stderr, result.stdout + result.stderr
    results.append(dict(rejected=name, diagnostic=diagnostic))
(root / "results.json").write_text(json.dumps(results, indent=2) + "\n")
print(f"PASS {len(results)} independent decoder and CLI input checks")
