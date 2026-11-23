#!/usr/bin/env python3
"""Check the production CLI, including independent HEIF stream inspection."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("input", type=Path)
parser.add_argument("artifacts", type=Path)
parser.add_argument("--unavailable", action="store_true")
args = parser.parse_args()
root = args.artifacts.resolve()
root.mkdir(parents=True, exist_ok=True)
settings = root / "settings"
settings.mkdir(exist_ok=True)
(settings / "options").write_text("[File Browser]\nParseExtensions=png;\nParseExtensionsEnabled=1;\n")
env = os.environ.copy()
env.update(RT_SETTINGS=str(settings), RT_CACHE=str(root / "cache"), OMP_NUM_THREADS="4")
binary, source = str(args.binary.resolve()), str(args.input.resolve())
profile = Path(__file__).resolve().parents[3] / "rtdata/iccprofiles/output/RTv2_sRGB.icc"
processing = root / "output-profile.pp3"
processing.write_text("[Version]\nVersion=349\n[Color Management]\nOutputProfile=file:" + str(profile) + "\n")
results = []

def run(name, flags, valid, suffix=".heic", sidecar=False):
    output = root / (name + suffix)
    command = ["nice", "-n", "10", binary, "-a", "-q", "-Y", "-O" if sidecar else "-o", str(output), "-p", str(processing), *flags, "-c", source]
    proc = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    (root / (name + ".log")).write_text(proc.stdout)
    assert (proc.returncode == 0) == valid, (name, proc.returncode, proc.stdout)
    assert output.exists() == valid, (name, "unexpected output")
    assert (Path(str(output) + ".pp3").exists()) == (valid and sidecar), (name, "sidecar")
    results.append({"name": name, "flags": flags, "exit": proc.returncode})
    return output

invalid = [["-H101"], ["-H-1"], ["-Hgarbage"], ["-H92x"], ["-H", "-Hs"],
           ["-H", "-Hs0"], ["-H", "-Hs4"], ["-H", "-Hs1x"], ["-H", "-b16"],
           ["-b32", "-H"], ["-H", "-b16f"], ["-H", "-b8x"], ["-H", "-b"],
           ["-b10", "-j"], ["-n", "-b12"], ["-b16f", "-n"], ["-b12", "-t"], ["-Hs1", "-j"]]
for i, flags in enumerate(invalid):
    run("invalid-" + str(i), flags, False)
if args.unavailable:
    for i, flags in enumerate([["-H"], ["-b12", "-H", "-Hs3"]]):
        run("unavailable-" + str(i), flags, False)
        assert "HEIF export unavailable" in (root / ("unavailable-" + str(i) + ".log")).read_text()
else:
    cases = [("default", ["-H"], 10, 1, ".heic"),
             ("quality-zero", ["-H0", "-b8", "-Hs2"], 8, 2, ".heic"),
             ("quality-max", ["-b12", "-Hs3", "-H100"], 12, 3, ".HEIF"),
             ("order-before", ["-b12", "-H77", "-Hs3"], 12, 3, ".heic"),
             ("order-after", ["-H77", "-Hs3", "-b12"], 12, 3, ".heic"),
             ("float-reset", ["-b16f", "-b10", "-H"], 10, 1, ".heic")]
    for name, flags, depth, chroma, suffix in cases:
        output = run(name, flags, True, suffix, sidecar=True)
        info = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(output)], text=True))
        stream = info["streams"][0]
        expected = "yuvj" + {1:"420", 2:"422", 3:"444"}[chroma] + "p" if depth == 8 else "yuv" + {1:"420", 2:"422", 3:"444"}[chroma] + "p" + str(depth) + "le"
        assert stream["pix_fmt"].replace("yuvj", "yuv") == expected.replace("yuvj", "yuv") and stream["color_range"] == "pc", (name, stream)
        metadata = json.loads(subprocess.check_output(["exiftool", "-j", "-ICC_Profile:all", "-EXIF:all", "-XMP:all", str(output)], text=True))[0]
        assert "ProfileDescription" in metadata, (name, "missing ICC")
        embedded = subprocess.check_output(["exiftool", "-b", "-ICC_Profile", str(output)])
        assert embedded == profile.read_bytes(), (name, "ICC changed")
        assert metadata.get("Software", "").startswith("RawTherapee "), (name, "missing export metadata")
    before = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(root / "order-before.heic"), "-f", "rawvideo", "-pix_fmt", "rgb48le", "-"])
    after = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(root / "order-after.heic"), "-f", "rawvideo", "-pix_fmt", "rgb48le", "-"])
    assert before == after, "argument order changes output"
# Established formats must remain usable in both builds.
for name, flags, suffix in [("jpeg", ["-j92"], ".jpg"), ("png", ["-n", "-b16"], ".png"),
                           ("tiff", ["-t", "-b16"], ".tif"), ("tiff-float", ["-b32", "-t"], ".tif")]:
    run(name, flags, True, suffix)
(root / "results.json").write_text(json.dumps(results, indent=2) + "\n")
print(f"{len(results)} CLI scenarios passed")
