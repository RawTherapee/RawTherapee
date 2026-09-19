# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate a tiny uncompressed Bayer DNG for engine tests; no external image data."""
from pathlib import Path
import struct
import sys


def make_dng(path):
    width, height = 256, 192
    entries = []
    def tag(key, kind, count, data):
        entries.append((key, kind, count, data))
    def short(key, *values):
        tag(key, 3, len(values), struct.pack('<' + 'H' * len(values), *values))
    def long(key, value):
        tag(key, 4, 1, struct.pack('<I', value))
    def ascii_tag(key, value):
        data = value.encode() + b'\0'
        tag(key, 2, len(data), data)
    long(256, width); long(257, height)
    short(258, 16); short(259, 1); short(262, 32803)
    ascii_tag(271, 'Synthetic'); ascii_tag(272, 'Negative fixture')
    long(273, 0); short(277, 1); long(278, height); long(279, width * height * 2)
    short(284, 1); short(33421, 2, 2)
    tag(33422, 1, 4, bytes([0, 1, 1, 2]))
    tag(50706, 1, 4, bytes([1, 4, 0, 0]))
    tag(50707, 1, 4, bytes([1, 1, 0, 0]))
    ascii_tag(50708, 'Synthetic Negative fixture')
    tag(50710, 1, 3, bytes([0, 1, 2])); short(50711, 1)
    short(50713, 1, 1)
    tag(50714, 5, 1, struct.pack('<II', 0, 1))
    long(50717, 65535)
    matrix = [1, 0, 0, 0, 1, 0, 0, 0, 1]
    tag(50721, 10, 9, b''.join(struct.pack('<ii', v, 1) for v in matrix))
    tag(50728, 5, 3, struct.pack('<IIIIII', 1, 1, 1, 1, 1, 1))
    short(50778, 21)
    entries.sort()
    offset = 8 + 2 + len(entries) * 12 + 4
    extra = bytearray()
    directory = bytearray()
    strip_entry = None
    for key, kind, count, data in entries:
        if key == 273:
            strip_entry = len(directory) + 8
        if len(data) <= 4:
            value = data.ljust(4, b'\0')
        else:
            value = struct.pack('<I', offset + len(extra))
            extra += data
            if len(extra) % 2:
                extra += b'\0'
        directory += struct.pack('<HHI', key, kind, count) + value
    struct.pack_into('<I', directory, strip_entry, offset + len(extra))
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            channel = [[0, 1], [1, 2]][y % 2][x % 2]
            value = [20000 + 80*x, 10000 + 65*y, 5000 + 25*x][channel]
            pixels += struct.pack('<H', value)
    Path(path).write_bytes(b'II*\0' + struct.pack('<I', 8) + struct.pack('<H', len(entries)) + directory + b'\0'*4 + extra + pixels)

if __name__ == '__main__':
    make_dng(sys.argv[1])
