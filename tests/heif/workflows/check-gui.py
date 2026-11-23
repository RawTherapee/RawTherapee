#!/usr/bin/env python3
"""Linux/Ninja integration check, reusing the built production GUI objects."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import os
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("artifacts", type=Path)
args = parser.parse_args()
build = args.build.resolve()
artifacts = args.artifacts.resolve()
source = Path(__file__).resolve().parents[3]
artifacts.mkdir(parents=True, exist_ok=True)
# Inspect Ninja's actual compiler/linker invocations; retain dependencies, flags,
# imported libraries and RPATH. Do not build a substitute implementation.
commands = subprocess.check_output(["ninja", "-t", "commands", "rth"], cwd=build, text=True).splitlines()
compile_line = next(line for line in commands if " -c " in line and line.endswith("/rtgui/main.cc"))
link_line = next(line for line in reversed(commands) if " -o rtgui/rawtherapee " in line)
compile_args = shlex.split(compile_line)
obj = artifacts / "gui-workflows.o"
for switch in ("-o", "-MT"):
    compile_args[compile_args.index(switch) + 1] = str(obj)
compile_args[compile_args.index("-MF") + 1] = str(obj) + ".d"
compile_args[compile_args.index("-c") + 1] = str(source / "tests/heif/workflows/gui-workflows.cc")
link_args = shlex.split(link_line)
if link_args[:2] == [":", "&&"]:
    link_args = link_args[2:]
if link_args[-2:] == ["&&", ":"]:
    link_args = link_args[:-2]
link_args[link_args.index("rtgui/CMakeFiles/rth.dir/main.cc.o")] = str(obj)
binary = artifacts / "gui-workflows"
link_args[link_args.index("-o") + 1] = str(binary)
for command in (compile_args, link_args):
    subprocess.run(["nice", "-n", "10", *command], cwd=build, check=True)
env = os.environ.copy()
env.update(RT_SETTINGS=str(artifacts / "settings"), RT_CACHE=str(artifacts / "cache"), OMP_NUM_THREADS="4")
subprocess.run([str(binary), str(source / "rtdata"), str(artifacts)], env=env, check=True, timeout=240)

# Inspect actual recovered queue exports with readers outside RawTherapee.
for name, depth, chroma in (("batch-0.heif", 12, "444"), ("batch-1.heif", 8, "422"), ("input.heic", 10, "420"), ("direct.HEIF", 10, "420")):
    output = artifacts / name
    if not output.exists() or output.read_bytes() == b"previous output":
        continue  # The unavailable build must preserve these (checked in C++).
    stream = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(output)], text=True))["streams"][0]
    expected = "yuv" + chroma + "p" + (str(depth) + "le" if depth > 8 else "")
    assert stream["pix_fmt"].replace("yuvj", "yuv") == expected and stream["color_range"] == "pc", (name, stream)
    embedded = subprocess.check_output(["exiftool", "-b", "-ICC_Profile", str(output)])
    assert embedded == (source / "rtdata/iccprofiles/output/RTv2_sRGB.icc").read_bytes(), (name, "ICC changed")
print("Independent queue output checks passed")
