# SPDX-License-Identifier: GPL-3.0-or-later
"""Link a desktop integration harness against a Unix Makefiles GUI build.

Default execution is offline. Optional --ollama/--photo modes are documented in
README.md and forwarded to the harness.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path, help='Built RawTherapee Unix Makefiles directory')
    parser.add_argument('scratch', type=Path, help='Dedicated disposable output directory')
    parser.add_argument('mode', nargs=argparse.REMAINDER, help='Optional --ollama or --photo arguments')
    args = parser.parse_args()
    build = args.build.resolve()
    scratch = args.scratch.resolve()
    root = Path(__file__).resolve().parents[2]
    compile_commands = build / 'compile_commands.json'
    link_file = build / 'rtgui/CMakeFiles/rth.dir/link.txt'
    if not compile_commands.is_file() or not link_file.is_file():
        parser.error('Build rth using Unix Makefiles and -DCMAKE_EXPORT_COMPILE_COMMANDS=ON first.')
    if scratch == root:
        parser.error('Use a separate scratch directory, not the repository root.')
    scratch.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(root / 'tests/ai-negative/make_fixture.py'),
                    str(scratch / 'negative.dng')], check=True)
    entries = json.loads(compile_commands.read_text())
    entry = next(e for e in entries if e['file'].endswith('/rtgui/main.cc'))
    original = shlex.split(entry['command'])

    def compile_file(source, output, extra=()):
        cmd = original.copy()
        cmd[cmd.index('-o') + 1] = str(output)
        cmd[cmd.index('-c') + 1] = str(source)
        cmd += ['-I' + str(root), *extra]
        subprocess.run(cmd, cwd=entry['directory'], check=True)

    compile_file(entry['file'], scratch / 'app-main.o', ['-Dmain=rawtherapee_original_main'])
    compile_file(root / 'tests/ai-negative/test_gui.cc', scratch / 'gui-test.o')
    cmd = shlex.split(link_file.read_text())
    cmd[cmd.index('-o') + 1] = str(scratch / 'gui-test')
    cmd = [str(scratch / 'app-main.o') if p.endswith('rth.dir/main.cc.o') else p for p in cmd]
    cmd.insert(1, str(scratch / 'gui-test.o'))
    subprocess.run(cmd, cwd=build / 'rtgui', check=True)
    subprocess.run([str(scratch / 'gui-test'), str(root), str(scratch), *args.mode], check=True)


if __name__ == '__main__':
    main()
