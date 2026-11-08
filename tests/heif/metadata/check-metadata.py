#!/usr/bin/env python3
"""Linux independent-reader checks and production CLI metadata regressions."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def command(*args, **kwargs):
    return subprocess.check_output(args, **kwargs)


def tags(path):
    return json.loads(command('exiftool', '-j', '-G1', '-EXIF:all', '-MakerNotes:all',
                              '-IPTC:all', '-XMP:all', str(path)))[0]


def comparable(data):
    return {key: value for key, value in data.items()
            if key not in ('SourceFile', 'IFD0:ModifyDate', 'IFD0:Software')}


def decoded_hash(path):
    return hashlib.sha256(command('ffmpeg', '-v', 'error', '-threads', '1', '-i', str(path),
                                  '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-')).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('engine_artifacts', type=Path)
    parser.add_argument('artifacts', type=Path)
    parser.add_argument('--jpeg-baseline', type=Path, help='Optional pre-G4 production CLI for exact JPEG comparisons')
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir(parents=True, exist_ok=True)
    engine = args.engine_artifacts.resolve()
    repo = Path(__file__).resolve().parents[3]
    profile = repo / 'rtdata/iccprofiles/output/RTv2_sRGB.icc'
    reports = []
    geometry = {'IFD0:Orientation', 'IFD0:ImageWidth', 'IFD0:ImageHeight',
                'ExifIFD:ExifImageWidth', 'ExifIFD:ExifImageHeight'}
    stripped = tags(engine / 'strip.heic')
    assert set(stripped) == {'SourceFile'}, stripped
    strip_hash = decoded_hash(engine / 'strip.heic')
    for mode in ('copy', 'edit', 'selected', 'sidecar', 'geometry', 'large-filtered', 'large-xmp'):
        output = engine / (mode + '.heic')
        actual = tags(output)
        assert not any(key.startswith('IPTC:') for key in actual), (mode, 'nonstandard IPTC')
        assert actual['IFD0:Software'].startswith('RawTherapee '), mode
        assert actual['XMP-tiff:Orientation'] == 'Horizontal (normal)', (mode, actual)
        assert actual['XMP-exif:ExifImageWidth'] == 64 and actual['XMP-exif:ExifImageHeight'] == 48, (mode, actual)
        if mode != 'selected':
            assert actual['ExifIFD:ExifImageWidth'] == 64 and actual['ExifIFD:ExifImageHeight'] == 48, (mode, actual)
        if mode == 'geometry':
            assert actual['IFD0:Orientation'] == 'Horizontal (normal)', actual
        else:
            assert 'IFD0:Orientation' not in actual, actual
        if mode in ('copy', 'edit', 'selected', 'sidecar', 'geometry', 'large-filtered'):
            jpeg = comparable(tags(engine / (mode + '.jpg')))
            for key, value in jpeg.items():
                if key.startswith(('IFD0:', 'ExifIFD:', 'Canon:')) and key not in geometry:
                    assert actual.get(key) == value, (mode, key, value, actual.get(key))
        if mode == 'edit':
            assert actual['XMP-dc:Description'] == 'Žluťoučký kůň 📷', actual
            assert actual['XMP-dc:Subject'] == ['kůň', 'léto', 'kůň'], actual
            assert actual['XMP-dc:Creator'] == ['Jana Nováková', 'Petr Černý'], actual
            assert actual['XMP-dc:Rights'] == '© 2026 Červený', actual
            assert actual['XMP-photoshop:City'] == 'České Budějovice', actual
        if mode == 'sidecar':
            assert actual['XMP-xmp:Label'] == 'sidecar label' and actual['IFD0:ImageDescription'] == 'sidecar caption', actual
            assert actual['XMP-dc:Description'] == 'explicit caption', actual
        if mode == 'large-xmp':
            assert len(actual['XMP-xmp:Label']) == 100000, mode
        digest = decoded_hash(output)
        assert digest == strip_hash, (mode, 'metadata changed rendered pixels')
        reports.append(dict(kind='engine', mode=mode, sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                            decoded_rgb_sha256=digest, metadata={k: v for k, v in actual.items() if k != 'SourceFile'}))

    modes = [('copy', 'Mode=0\n'), ('selected', 'Mode=1\nExifKeys=Exif.Image.Make;\n'),
             ('edit', 'Mode=1\nExifKeys=*;\n\n[Exif]\nArtist=edited artist\n\n[IPTC]\nCaption=Žluťoučký kůň 📷;\nKeywords=kůň;léto;kůň;\nCreator=Jana Nováková;Petr Černý;\nCopyright=© 2026 Červený;\nCity=České Budějovice;\n'),
             ('sidecar', 'Mode=0\n'), ('strip', 'Mode=2\n')]
    for mode, metadata in modes:
        processing = root / (mode + '.pp3')
        processing.write_text('[Version]\nVersion=349\n[Color Management]\nOutputProfile=file:' + str(profile) + '\n\n[MetaData]\n' + metadata)
        settings = root / (mode + '-settings')
        settings.mkdir(exist_ok=True)
        (settings / 'options').write_text('[File Browser]\nParseExtensions=jpg;\nParseExtensionsEnabled=1;\n[Metadata]\nXMPSynchronization=' + ('read' if mode == 'sidecar' else 'none') + '\n')
        env = os.environ.copy()
        env.update(RT_SETTINGS=str(settings), RT_CACHE=str(settings / 'cache'), OMP_NUM_THREADS='4')
        def export(binary, name, flags, suffix):
            output = root / (name + suffix)
            process = subprocess.run(['nice', '-n', '10', str(binary.resolve()), '-a', '-q', '-Y', '-O', str(output),
                                      '-p', str(processing), *flags, '-c', str(engine / 'source.jpg')], env=env,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
            (root / (name + '.log')).write_bytes(process.stdout)
            assert process.returncode == 0, (name, process.stdout)
            assert Path(str(output) + '.pp3').exists(), (name, 'sidecar missing')
            return output
        heif = export(args.binary, mode, ['-H'], '.heic')
        actual = tags(heif)
        assert command('exiftool', '-b', '-ICC_Profile', str(heif)) == profile.read_bytes(), (mode, 'ICC')
        if mode == 'strip':
            assert set(actual) == {'SourceFile'}, actual
        else:
            assert actual['IFD0:Make'] == 'Canon', actual
            assert actual['XMP-tiff:Orientation'] == 'Horizontal (normal)', actual
            if mode == 'selected':
                assert 'IFD0:Model' not in actual and 'Canon:SerialNumber' not in actual, actual
            if mode == 'edit':
                assert actual['IFD0:Artist'] == 'edited artist', actual
                assert actual['XMP-dc:Description'] == 'Žluťoučký kůň 📷' and actual['XMP-dc:Subject'] == ['kůň', 'léto', 'kůň'], actual
            if mode == 'sidecar':
                assert actual['XMP-xmp:Label'] == 'sidecar label', actual
        jpeg = export(args.binary, mode + '-jpeg', ['-j92'], '.jpg')
        jpeg_tags = tags(jpeg)
        digest = decoded_hash(jpeg)
        if args.jpeg_baseline:
            baseline = export(args.jpeg_baseline, mode + '-baseline', ['-j92'], '.jpg')
            assert comparable(tags(baseline)) == comparable(jpeg_tags), (mode, 'JPEG metadata regression')
            assert decoded_hash(baseline) == digest, (mode, 'JPEG pixels regression')
            assert mode == 'strip' or jpeg_tags['IFD0:Software'].startswith('RawTherapee '), mode
        reports.append(dict(kind='cli', mode=mode, metadata=comparable(actual),
                            jpeg_metadata=comparable(jpeg_tags), jpeg_decoded_rgb_sha256=digest,
                            jpeg_baseline_compared=bool(args.jpeg_baseline)))
    (root / 'results.json').write_text(json.dumps(reports, ensure_ascii=False, indent=2) + '\n')
    print('PASS seven independent HEIF reader checks and five HEIF/JPEG CLI metadata scenarios' +
          (' with exact pre-G4 JPEG comparisons' if args.jpeg_baseline else ''))


if __name__ == '__main__':
    main()
