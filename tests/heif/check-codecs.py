#!/usr/bin/env python3
"""Exercise a plugin-enabled harness with encoder/decoder independently hidden."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("harness", type=Path)
    parser.add_argument("plugins", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    all_plugins = list(args.plugins.iterdir())
    encoder = [p for p in all_plugins if "x265" in p.name and p.suffix in (".so", ".dll", ".dylib")]
    decoder = [p for p in all_plugins if "libde265" in p.name and p.suffix in (".so", ".dll", ".dylib")]
    assert len(encoder) == len(decoder) == 1, (encoder, decoder)
    report = []
    for enc, dec in ((0, 0), (0, 1), (1, 0), (1, 1)):
        directory = args.output / f"plugins-{enc}-{dec}"
        directory.mkdir(exist_ok=True)
        # Do not clear directories supplied by a caller: stale plugins fail the
        # availability assertion below rather than silently changing the case.
        for plugin in (encoder if enc else []) + (decoder if dec else []):
            shutil.copy2(plugin, directory)
        environment = dict(os.environ, LIBHEIF_PLUGIN_PATH=str(directory.resolve()))
        capabilities = subprocess.run([str(args.harness.resolve()), "--expect-codecs", str(enc), str(dec)],
            env=environment, text=True, capture_output=True)
        assert capabilities.returncode == 0, capabilities.stdout + capabilities.stderr
        output = args.output / f"artifacts-{enc}-{dec}"
        operation = subprocess.run([str(args.harness.resolve()), "--roundtrip", str(output.resolve())],
            env=environment, text=True, capture_output=True)
        expected = 2 if not enc else 3 if not dec else 0
        assert operation.returncode == expected, operation.stdout + operation.stderr
        if not enc:
            assert "HEVC encoder unavailable" in operation.stderr, operation.stderr
        elif not dec:
            assert "HEVC decoder unavailable" in operation.stderr, operation.stderr
        report.append(dict(encoder=enc, decoder=dec, exit_code=operation.returncode,
                           capabilities=capabilities.stdout, error=operation.stderr,
                           roundtrip=operation.stdout))
        print(f"PASS encoder={enc} decoder={dec}: exit {expected}", flush=True)
    (args.output / "codec-checks.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
