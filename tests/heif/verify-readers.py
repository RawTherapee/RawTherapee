#!/usr/bin/env python3
"""Check generated fixtures with readers independent of the C++ harness.

Requires ExifTool, FFmpeg, and ImageMagick; uses only Python's standard library.
Writes a JSON report beside the fixtures. Does not modify the HEIF files.
"""

import argparse
import array
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def command(*args):
    result = subprocess.run(args, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"Command failed: {args}\n{result.stderr.decode(errors='replace')}")
    return result.stdout


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sample(x, y, channel):
    if y < 128:
        return x / 1024
    patches = [(0.05, 0.05, 0.05), (0.95, 0.95, 0.95), (0.85, 0.10, 0.15),
               (0.10, 0.80, 0.20), (0.10, 0.15, 0.85), (0.80, 0.65, 0.15),
               (0.15, 0.70, 0.80), (0.70, 0.15, 0.75)]
    return patches[min(7, x * 8 // 1025)][channel]


def verify(path):
    bps, chroma, *profile_kind = path.stem.split("-")
    bps = int(bps)
    dual = profile_kind == ["icc", "nclx"]
    wide = profile_kind == ["wide", "icc"]
    icc = path.with_suffix(".icc")
    assert command("exiftool", "-b", "-ICC_Profile", str(path)) == icc.read_bytes(), \
        f"{path.name}: ExifTool ICC extraction differs"
    tags = json.loads(command("exiftool", "-j", "-n", "-BitDepthLuma", "-BitDepthChroma",
                              "-ChromaFormat", "-ColorPrimaries", "-TransferCharacteristics",
                              "-MatrixCoefficients", "-VideoFullRangeFlag", str(path)))[0]
    assert tags["BitDepthLuma"] == bps and tags["BitDepthChroma"] == bps, tags
    assert tags["ChromaFormat"] == {"420": 1, "422": 2, "444": 3}[chroma], tags
    if dual:
        assert [tags.get(tag) for tag in ("ColorPrimaries", "TransferCharacteristics",
                "MatrixCoefficients", "VideoFullRangeFlag")] == [1, 13, 1, 1], tags
    else:
        assert "ColorPrimaries" not in tags, tags
    dimensions = command("magick", "identify", "-format", "%w %h", str(path)).decode()
    assert dimensions == "1025 257", (path.name, dimensions)
    stream = json.loads(command("ffprobe", "-v", "error", "-show_entries",
        "stream=codec_name,width,height,pix_fmt,color_range,color_space,color_transfer,color_primaries",
        "-of", "json", str(path)))["streams"][0]
    assert stream["codec_name"] == "hevc", stream
    expected_format = "yuv" + chroma + "p" + ("" if bps == 8 else f"{bps}le")
    expected_formats = [expected_format]
    if bps == 8:
        expected_formats.append("yuvj" + chroma + "p")
    assert stream["pix_fmt"] in expected_formats, stream
    assert stream["color_range"] == "pc" and stream["color_space"] == "bt709", stream
    if wide:
        assert stream.get("color_primaries", "unknown") == "unknown", stream
        assert stream.get("color_transfer", "unknown") == "unknown", stream
    else:
        assert stream["color_primaries"] == "bt709", stream
        assert stream["color_transfer"] == "iec61966-2-1", stream

    # FFmpeg's native HEVC decoder uses no libheif/libde265 decoding path.
    # Apply clean aperture explicitly: FFprobe reports the padded coded size.
    raw = command("ffmpeg", "-v", "error", "-threads", "1", "-apply_cropping", "0",
                  "-i", str(path), "-frames:v", "1", "-vf",
                  "format=rgb48le,crop=1025:257:0:0:exact=1",
                  "-pix_fmt", "rgb48le", "-f", "rawvideo", "-")
    assert len(raw) == 1025 * 257 * 3 * 2, (path.name, len(raw))
    values = array.array("H", raw)
    if sys.byteorder != "little":
        values.byteswap()
    error_sum = 0
    interior_max = 0
    levels = set()
    for y in range(257):
        for x in range(1025):
            for channel in range(3):
                value = values[(y * 1025 + x) * 3 + channel]
                error = abs(value / 65535 - sample(x, y, channel))
                error_sum += error
                if (8 < x < 1016 and 8 < y < 248 and abs(y - 128) > 8
                        and (y < 128 or 72 < x * 8 % 1025 < 953)):
                    interior_max = max(interior_max, error)
                if channel == 0 and y == 64:
                    levels.add(value)
    mean = error_sum / (1025 * 257 * 3)
    assert mean < 0.035 and interior_max < 0.035, (path.name, mean, interior_max)
    assert bps == 8 or len(levels) > 256, (path.name, len(levels))
    return dict(file=path.name, sha256=sha256(path), icc_sha256=sha256(icc),
                exiftool=tags, ffprobe=stream, ffmpeg_mean_rgb_error=mean,
                ffmpeg_interior_max_error=interior_max, ffmpeg_ramp_levels=len(levels))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifacts", type=Path)
    args = parser.parse_args()
    paths = sorted(args.artifacts.glob("*.heic"))
    assert len(paths) == 19, f"Expected 19 fixtures, found {len(paths)}"
    report = dict(exiftool=command("exiftool", "-ver").decode().strip(),
                  ffmpeg=command("ffmpeg", "-version").decode().splitlines()[0],
                  imagemagick=command("magick", "-version").decode().splitlines()[0],
                  results=[])
    for path in paths:
        result = verify(path)
        report["results"].append(result)
        print(f"PASS {path.name}: FFmpeg mean error {result['ffmpeg_mean_rgb_error']:.6f}",
              flush=True)
    destination = args.artifacts.parent / "independent-readers.json"
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(destination)


if __name__ == "__main__":
    main()
