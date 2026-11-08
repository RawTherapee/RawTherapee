#!/usr/bin/env python3
"""Linux production export checks with the plugin-enabled libheif substituted."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', type=Path)
parser.add_argument('plugin_prefix', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
plugins = list((args.plugin_prefix / 'lib/libheif').glob('*.so'))
encoder = [p for p in plugins if 'x265' in p.name]
decoder = [p for p in plugins if 'libde265' in p.name]
assert len(encoder) == len(decoder) == 1, plugins
report = []
for enc, dec in ((0, 0), (0, 1), (1, 0), (1, 1)):
    directory = args.output / f'plugins-{enc}-{dec}'
    directory.mkdir(exist_ok=True)
    for plugin in (encoder if enc else []) + (decoder if dec else []):
        shutil.copy2(plugin, directory)
    assert sorted(p.name for p in directory.iterdir()) == sorted(p.name for p in
        (encoder if enc else []) + (decoder if dec else [])), 'stale plugins in test directory'
    environment = dict(os.environ, LD_LIBRARY_PATH=str((args.plugin_prefix / 'lib').resolve()),
                       LIBHEIF_PLUGIN_PATH=str(directory.resolve()))
    mode = '--codec-smoke' if enc else '--missing-encoder'
    result = subprocess.run([str(args.binary.resolve()), str((args.output / f'output-{enc}-{dec}').resolve()), mode],
                            env=environment, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    report.append(dict(encoder=enc, decoder=dec, exit_code=result.returncode,
                       stdout=result.stdout, stderr=result.stderr))
    print(f'PASS encoder={enc} decoder={dec}', flush=True)
(args.output / 'export-codecs.json').write_text(json.dumps(report, indent=2) + '\n')
