"""Check Nikon metadata strength and lens-profile switches on the reference NEFs."""

import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile

from PIL import Image


def render(cli, nef, directory, distortion, vignette):
    profile = directory / "lens.pp3"
    profile.write_text(
        "[Version]\nVersion=353\n"
        "[Common Properties for Transformations]\nAutoFill=false\n"
        "[LensProfile]\nLcMode=metadata\n"
        f"UseDistortion={str(distortion).lower()}\n"
        f"UseVignette={str(vignette).lower()}\nUseCA=false\n",
        encoding="utf-8",
    )
    output = directory / "render.png"
    subprocess.run(
        [cli, "-q", "-Y", "-p", profile, "-o", output, "-n", "-c", nef],
        env=dict(os.environ, RT_SETTINGS=str(directory / "settings"), RT_CACHE=str(directory / "cache")),
        check=True,
    )
    with Image.open(output) as image:
        return image.size, hashlib.sha256(image.tobytes()).digest()


def check_strength(cli, nef, directory, off_image):
    # This test edits copies of this specific fixture. Verify its identity
    # before using file offsets for the little-endian 0100 block.
    data = bytearray(nef.read_bytes())
    if hashlib.sha256(data).hexdigest() != "a3314a026524396e603b461afc4fe61a8a5a3958232f3687bb1c8eca0dcbed93":
        raise ValueError("Strength test requires the original Z 50II #7762")
    block = 0x39208
    for power in range(8):
        struct.pack_into("<ii", data, block + 20 + 8 * power, 0, 1)

    def variant(name, strength, quadratic_denominator):
        struct.pack_into("<ii", data, block + 20 + 6 * 8, 1, quadratic_denominator)
        struct.pack_into("<ii", data, block + 0x60, *strength)
        path = directory / (name + ".NEF")
        path.write_bytes(data)
        return render(cli, path, directory, True, True)

    half = variant("half", (1, 2), 2)
    equivalent = variant("equivalent", (1, 1), 4)
    if half != equivalent or half == off_image:
        raise SystemExit("Half strength on r²/2 must give the same correction as r²/4")
    if variant("zero", (0, 1024), 4) != equivalent:
        raise SystemExit("Zero recorded strength must allow the full curve")
    for name, strength in (("invalid", (1, 0)), ("negative", (-1, 4))):
        if variant(name, strength, 4) != off_image:
            raise SystemExit(f"{name} strength changed the distortion-only image")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline_cli", type=Path, help="reference CLI with the original Nikon distortion correction")
    parser.add_argument("weighted_cli", type=Path, help="CLI built with weighted native DX vignetting enabled")
    parser.add_argument("nef", type=Path, help="Z 50II #7762 from the sample manifest")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        directory = Path(tmp)
        results = set()
        cli = args.weighted_cli.resolve()
        for distortion in (False, True):
            off = render(cli, args.nef, directory, distortion, False)
            on = render(cli, args.nef, directory, distortion, True)
            baseline = render(args.baseline_cli.resolve(), args.nef, directory, distortion, False)
            if baseline != off:
                raise SystemExit(f"Vignetting off changed pixels with UseDistortion={distortion}")
            if distortion:
                check_strength(cli, args.nef, directory, off)
            results.update((off, on))
        if len(results) != 4:
            raise SystemExit("Expected four distinct validation renders")
    print("All four switch combinations, strength scaling and invalid metadata checks passed.")


if __name__ == "__main__":
    main()
