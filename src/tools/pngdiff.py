#!/usr/bin/env python3
"""tools/pngdiff.py A.png B.png: how many pixels differ, and by how much at most (8-bit RGB(A) PNGs from shot)"""
import struct, sys, zlib
def load(p):
    d = open(p, 'rb').read(); i = 8; idat = b''; w = h = ct = 0
    while i < len(d):
        n, t = struct.unpack('>I4s', d[i:i + 8]); c = d[i + 8:i + 8 + n]; i += 12 + n
        if t == b'IHDR': w, h, _, ct = struct.unpack('>IIBB', c[:10])
        elif t == b'IDAT': idat += c
    bpp = 4 if ct == 6 else 3; raw = zlib.decompress(idat); out = bytearray(); prev = bytearray(w * bpp); k = 0
    for y in range(h):
        f = raw[k]; row = bytearray(raw[k + 1:k + 1 + w * bpp]); k += 1 + w * bpp
        for x in range(len(row)):
            a = row[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
            if f == 1: row[x] = (row[x] + a) & 255
            elif f == 2: row[x] = (row[x] + b) & 255
            elif f == 3: row[x] = (row[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                row[x] = (row[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += row; prev = row
    return w, h, bpp, out
w, h, bp, a = load(sys.argv[1]); w2, h2, bp2, b = load(sys.argv[2])
assert (w, h, bp) == (w2, h2, bp2)
n = mx = 0
for i in range(0, len(a), bp):
    dd = max(abs(a[i + k] - b[i + k]) for k in range(3))
    if dd: n += 1; mx = max(mx, dd)
print(f"{n} pixels differ ({100.0 * n / (w * h):.3f} %), largest difference {mx}")
