#!/usr/bin/env python3
"""Check production linear-DNG loading against generated, exact RGB pixels.

Requires a Ninja build with compile_commands.json and the rth-cli target built.
Example: python3 tools/test_linear_dng.py --build build/dev --sanitize
No external image library or downloaded camera sample is required.
"""

import argparse
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def lossless_jpeg(pixels, width, height):
    """Encode a small 12-bit RGB JPEG using lossless predictor 1 (SOF3)."""
    def segment(marker, data):
        return bytes((255, marker)) + struct.pack('>H', len(data) + 2) + data

    bits = []
    for y in range(height):
        for x in range(width):
            for c in range(3):
                index = (y * width + x) * 3 + c
                predictor = (pixels[index - 3] if x else
                             pixels[index - width * 3] if y else 2048)
                delta = pixels[index] - predictor
                size = abs(delta).bit_length()
                bits.append(f'{size:04b}')  # Categories 0..12 have 4-bit codes.
                if size:
                    amplitude = delta if delta >= 0 else delta + (1 << size) - 1
                    bits.append(f'{amplitude:0{size}b}')
    encoded = ''.join(bits)
    encoded += '1' * (-len(encoded) % 8)
    scan = bytes(int(encoded[i:i + 8], 2) for i in range(0, len(encoded), 8))
    scan = scan.replace(b'\xff', b'\xff\x00')
    huffman = bytes([0, 0, 0, 0, 13] + [0] * 12 + list(range(13)))
    frame = struct.pack('>BHHB', 12, height, width, 3)
    frame += bytes([1, 0x11, 0, 2, 0x11, 0, 3, 0x11, 0])
    start = bytes([3, 1, 0, 2, 0, 3, 0, 1, 0, 0])
    return (b'\xff\xd8' + segment(0xc4, huffman) + segment(0xc3, frame)
            + segment(0xda, start) + scan + b'\xff\xd9')


def fixture(path, maker, encoding, area):
    width, height = 64, 48
    top, left, bottom, right = area
    # Coordinate-dependent, nonconstant values distinguish wrong rows, channels,
    # offsets, truncation and repeated cropping at every pixel, including edges.
    pixels = [(y * 61 + x * 17 + c * 701) % 4096
              for y in range(height) for x in range(width) for c in range(3)]
    if encoding == 'lossless12':
        raw = lossless_jpeg(pixels, width, height)
        compression, depth = 7, 12
    elif encoding == 'packed12':
        raw = bytearray()
        for a, b in zip(pixels[::2], pixels[1::2]):
            raw.extend((a >> 4, ((a & 15) << 4) | (b >> 8), b & 255))
        compression, depth = 1, 12
    else:
        raw = struct.pack('<' + 'H' * len(pixels), *pixels)
        compression, depth = 1, 16

    def short(*values):
        return 3, len(values), struct.pack('<' + 'H' * len(values), *values)

    def long(*values):
        return 4, len(values), struct.pack('<' + 'I' * len(values), *values)

    def string(value):
        data = value.encode('ascii') + b'\0'
        return 2, len(data), data

    tags = {
        254: long(0), 256: long(width), 257: long(height),
        258: short(depth, depth, depth), 259: short(compression),
        262: short(34892), 271: string(maker), 272: string('Linear DNG test'),
        273: long(0), 274: short(1), 277: short(3), 278: long(height),
        279: long(len(raw)), 284: short(1),
        50706: (1, 4, bytes([1, 4, 0, 0])),
        50707: (1, 4, bytes([1, 1, 0, 0])),
        50708: string(maker + ' Linear DNG test'),
        50714: short(0), 50717: long(4095),
        50719: long(2, 1), 50720: long(right - left - 4, bottom - top - 2),
        50721: (10, 9, b''.join(struct.pack('<ii', int(i % 4 == 0), 1) for i in range(9))),
        50728: (5, 3, struct.pack('<IIIIII', 1, 1, 1, 1, 1, 1)),
        50778: short(21), 50829: long(*area),
    }
    data = bytearray(b'II\x2a\x00' + struct.pack('<I', 8))
    data += struct.pack('<H', len(tags)) + bytes(12 * len(tags) + 4)
    for index, (tag, (kind, count, value)) in enumerate(sorted(tags.items())):
        if len(value) > 4:
            if len(data) % 2:
                data.append(0)
            field = struct.pack('<I', len(data))
            data += value
        else:
            field = value.ljust(4, b'\0')
        offset = 10 + 12 * index
        data[offset:offset + 12] = struct.pack('<HHI', tag, kind, count) + field
    strip_offset = 10 + 12 * sorted(tags).index(273) + 8
    data[strip_offset:strip_offset + 4] = struct.pack('<I', len(data))
    # RawTherapee's dcraw identification rejects files smaller than 100 KB.
    path.write_bytes((data + raw).ljust(100000, b'\0'))
    expected = [right - left, bottom - top]
    expected.extend(pixels[(y * width + x) * 3 + c]
                    for y in range(top, bottom) for x in range(left, right) for c in range(3))
    path.with_suffix('.bin').write_bytes(struct.pack('<' + 'H' * len(expected), *expected))


def build_probe(build, output, sanitize):
    entries = json.loads((build / 'compile_commands.json').read_text())
    objects = []
    sources = [ROOT / 'tools/test_linear_dng.cc']
    if sanitize:
        sources += [ROOT / 'rtengine/rawimage.cc', ROOT / 'rtengine/dcraw.cc']
    for source in sources:
        template = source if source.parent.name == 'rtengine' else ROOT / 'rtengine/rawimage.cc'
        entry = next(e for e in entries if Path(e['file']) == template)
        command = shlex.split(entry['command'])
        command = command[:command.index('-o')]
        if sanitize:
            command += ['-O1', '-g', '-fsanitize=address', '-fno-omit-frame-pointer']
        obj = output / (source.stem + '.o')
        with (output / (source.stem + '-compile.log')).open('w') as log:
            subprocess.run(['nice', '-n', '10', *command, '-c', str(source), '-o', str(obj)],
                           cwd=entry['directory'], stdout=log, stderr=subprocess.STDOUT, check=True)
        objects.append(str(obj))
    link = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'rth-cli'], text=True)
    tokens = shlex.split(link.splitlines()[-1])
    split = tokens.index('-o')
    support = [str(build / token) for token in tokens[:split]
               if token.endswith('.o') and not token.endswith('/main-cli.cc.o')]
    libraries = tokens[split + 2:]
    if '&&' in libraries:
        libraries = libraries[:libraries.index('&&')]
    executable = output / 'test-linear-dng'
    command = [command[0], '-fopenmp', '-pthread']
    if sanitize:
        command += ['-fsanitize=address']
    command += objects + support + ['-o', str(executable)] + libraries
    (output / 'link.json').write_text(json.dumps(command, indent=2) + '\n')
    with (output / 'link.log').open('w') as log:
        subprocess.run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
    return executable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/dev')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--sanitize', action='store_true', help='Instrument RAW loading with AddressSanitizer')
    args = parser.parse_args()
    build = args.build.resolve()
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='linear-dng-', dir=build))
    output.mkdir(parents=True, exist_ok=True)
    print('Evidence:', output, flush=True)
    executable = build_probe(build, output, args.sanitize)
    env = dict(os.environ, OMP_NUM_THREADS='1', ASAN_OPTIONS='detect_leaks=0:halt_on_error=1')
    count = 0
    for maker in ['Sigma', 'Pentax', 'Sony', 'Test']:
        for encoding in ['unpacked16', 'packed12', 'lossless12']:
            for name, area in [('full', (0, 0, 48, 64)), ('active', (3, 5, 43, 59))]:
                path = output / f'{maker}-{encoding}-{name}.dng'
                fixture(path, maker, encoding, area)
                for decoder in ['dcraw', 'libraw']:
                    command = [str(executable), decoder, str(path), str(path.with_suffix('.bin'))]
                    with (output / f'{path.stem}-{decoder}.log').open('w') as log:
                        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
                    if result.returncode:
                        raise RuntimeError(f'{path.name} / {decoder} failed; see {output}')
                    count += 1
    print(f'PASS: {count} decoder runs; all active-area pixels, dimensions, offsets and Foveon stride checked')


if __name__ == '__main__':
    main()
