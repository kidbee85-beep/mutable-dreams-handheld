#!/usr/bin/env python3
"""tools/preview640.py IN.png OUT.png [W H]: a stage shot made as small as a handheld's screen (area average,
like brick_sdl.c's MD_OUT), to judge what stays readable there"""
import struct, sys, zlib
exec(open(__file__.replace('preview640.py', 'pngdiff.py')).read().split('w, h, bp, a = load')[0])
W, H = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (640, 480)
w, h, bp, px = load(sys.argv[1])
def axis(src, dst):
    out = []
    r = src / dst
    for d in range(dst):
        a, b = d * r, (d + 1) * r; ws = []
        i = int(a)
        while i < b and i < src:
            lo, hi = max(i, a), min(i + 1, b)
            if hi > lo: ws.append((i, (hi - lo) / r))
            i += 1
        out.append(ws)
    return out
ax, ay = axis(w, W), axis(h, H)
rows = []
for y in range(H):
    row = bytearray([0])
    for x in range(W):
        acc = [0.0, 0.0, 0.0]
        for sy, wy in ay[y]:
            for sx, wx in ax[x]:
                k = (sy * w + sx) * bp
                for c in range(3): acc[c] += px[k + c] * wx * wy
        row += bytes(min(255, int(v + 0.5)) for v in acc)
    rows.append(bytes(row))
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[2], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) +
                              chunk(b'IDAT', zlib.compress(b''.join(rows), 6)) + chunk(b'IEND', b''))
