#!/usr/bin/env python3
"""Verify production Image8/Image16/Imagefloat HEIF exports with independent readers."""
import array
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def command(*args):
    result = subprocess.run(args, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"{args}: {result.stderr.decode(errors='replace')}")
    return result.stdout


def sample(x, y, channel, kind):
    if kind == 'clamped':
        return 0 if x < 64 else .5 if x < 96 else 1
    if kind == 'tiny':
        return 32768 / 65535
    if y < 128:
        value = x / 1024
    else:
        patches = [(.05, .05, .05), (.95, .95, .95), (.85, .10, .15),
                   (.10, .80, .20), (.10, .15, .85), (.80, .65, .15),
                   (.15, .70, .80), (.70, .15, .75)]
        value = patches[min(7, x * 8 // 1025)][channel]
    if kind == 'image8':
        value = int(value * 255 + .5) / 255
    elif kind == 'image16':
        value = int(value * 65535 + .5) / 65535
    return value


def verify(path, directory):
    parts = path.stem.split('-')
    kind = parts[0]
    width, height = (129, 65) if kind in ('clamped', 'quality') else (1, 1) if kind == 'tiny' else (1025, 257)
    bps = int(parts[1]) if kind in ('float', 'image8', 'image16', 'tiny') else 12 if kind in ('wide', 'clamped') else 10
    chroma = parts[2] if kind in ('float', 'image8', 'image16') else '444' if kind in ('wide', 'clamped', 'quality') else '420'
    expected_icc = b'' if kind == 'no' else (directory / ('wide.icc' if kind == 'wide' else 'float-12-444.icc')).read_bytes()
    assert command('exiftool', '-b', '-ICC_Profile', str(path)) == expected_icc, path.name
    tags = json.loads(command('exiftool', '-j', '-n', '-BitDepthLuma', '-BitDepthChroma',
        '-ChromaFormat', '-ColorPrimaries', '-EXIF:all', '-XMP:all', '-Rotation', str(path)))[0]
    assert tags['BitDepthLuma'] == tags['BitDepthChroma'] == bps, tags
    assert tags['ChromaFormat'] == {'420': 1, '422': 2, '444': 3}[chroma], tags
    assert set(tags) <= {'SourceFile', 'BitDepthLuma', 'BitDepthChroma', 'ChromaFormat', 'Rotation'}, tags
    assert tags.get('Rotation', 0) == 0, tags
    dimensions = command('magick', 'identify', '-format', '%w %h', str(path)).decode()
    assert dimensions == f'{width} {height}', (path.name, dimensions)
    stream = json.loads(command('ffprobe', '-v', 'error', '-show_entries',
        'stream=codec_name,pix_fmt,color_range,color_space,color_transfer,color_primaries', '-of', 'json', str(path)))['streams'][0]
    expected = 'yuv' + chroma + 'p' + ('' if bps == 8 else f'{bps}le')
    assert stream['codec_name'] == 'hevc' and stream['pix_fmt'] in (expected, 'yuvj' + chroma + 'p' if bps == 8 else expected), stream
    assert stream['color_range'] == 'pc' and stream['color_space'] == 'bt709', stream
    assert stream.get('color_primaries', 'unknown') == stream.get('color_transfer', 'unknown') == 'unknown', stream
    raw = command('ffmpeg', '-v', 'error', '-threads', '1', '-apply_cropping', '0', '-i', str(path),
        '-frames:v', '1', '-vf', f'format=rgb48le,crop={width}:{height}:0:0:exact=1',
        '-pix_fmt', 'rgb48le', '-f', 'rawvideo', '-')
    values = array.array('H', raw)
    if sys.byteorder != 'little':
        values.byteswap()
    assert len(values) == width * height * 3, path.name
    report = dict(file=str(path.relative_to(directory)), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  icc_sha256=hashlib.sha256(expected_icc).hexdigest(), exiftool=tags, ffprobe=stream)
    if kind != 'quality':
        total, interior_max = 0., 0.
        levels = set()
        for y in range(height):
            for x in range(width):
                for c in range(3):
                    value = values[(y * width + x) * 3 + c] / 65535
                    error = abs(value - sample(x, y, c, kind))
                    total += error
                    if kind in ('clamped', 'tiny') or (8 < x < 1016 and 8 < y < 248 and abs(y - 128) > 8
                            and (y < 128 or 72 < x * 8 % 1025 < 953)):
                        interior_max = max(interior_max, error)
                    if c == 0 and y == 64:
                        levels.add(value)
        mean = total / (width * height * 3)
        assert mean < .035 and interior_max < .035, (path.name, mean, interior_max)
        if kind in ('float', 'image16') and bps > 8:
            assert len(levels) > 256, (path.name, len(levels))
        report.update(mean_rgb_error=mean, interior_max_error=interior_max, ramp_levels=len(levels))
    print('PASS', path.relative_to(directory), flush=True)
    return report


def main():
    directory = Path(sys.argv[1]).resolve()
    paths = sorted(p for p in directory.glob('*.heic') if p.is_file()) + sorted(directory.glob('*/*.heif')) + sorted(directory.glob('concurrent/*/*.heic'))
    assert len(paths) == 56, f'Expected 56 outputs, got {len(paths)}'
    report = dict(exiftool=command('exiftool', '-ver').decode().strip(),
                  ffmpeg=command('ffmpeg', '-version').decode().splitlines()[0],
                  imagemagick=command('magick', '-version').decode().splitlines()[0],
                  results=[verify(p, directory) for p in paths])
    destination = directory.parent / 'independent-export-readers.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print(destination)


if __name__ == '__main__':
    main()
