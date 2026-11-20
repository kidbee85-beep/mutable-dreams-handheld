#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/flicker_scan.py FILM.raw [OUT.bmp]: where the stage flickers, from brick-headless's "film" frames (1.5.4).

A film file is what the runner's "film PATH" ... "filmstop" writes: per frame 4 bytes of held buttons, then 1024 x 768
pixels (BGRA). Two measures, per region (left column, centre, right column):
  one-frame glitches - frames that differ from both neighbours where the neighbours agree (a picture shown for one
                       frame only: the "flicker as the cursor moves" of 1.5.3 on the RGB20S), with the buttons held;
  A-B-A pixels       - pixels that go back and forth three times or more over the film (a part drawn only every other
                       frame: the garden's sky beside the stage in 1.5.2).
OUT.bmp (optional): frame 10 with the A-B-A pixels of the side columns in magenta, the centre dimmed.
Make a film, e.g.:  printf 'handheld 1\\nsmall 1\\nwait 3000\\ntap START\\nwait 2000\\nfilm /tmp/g.raw\\nwait 300\\n
tap DOWN\\nwait 300\\ntap R1\\nwait 300\\nfilmstop\\n' > /tmp/s.txt; ./build/mac/brick-headless /tmp/s.txt /tmp/save
(paths without spaces: the runner splits its lines at spaces). Pure Python, slow for long films: 2-3 s are enough."""
import array, struct, sys

W, H = 1024, 768
F = 4 + W * H * 4
data = open(sys.argv[1], "rb").read()
n = len(data) // F
fr = [array.array("I", data[i * F + 4:(i + 1) * F]) for i in range(n)]
held = [int.from_bytes(data[i * F:i * F + 4], "little") for i in range(n)]
REG = {"left": (0, 152), "centre": (152, 872), "right": (872, 1024)}
print(f"{n} frames")
for name, (x0, x1) in REG.items():
    ev = {}
    for i in range(1, n - 1):
        c = 0
        for y in range(0, H, 3):
            for x in range(x0, x1, 3):
                k = y * W + x
                if fr[i - 1][k] == fr[i + 1][k] and fr[i][k] != fr[i - 1][k]:
                    c += 1
        if c > 40:
            ev[i] = (c, held[i])
    aba = 0
    for y in range(0, H, 2):
        for x in range(x0, x1, 2):
            k = y * W + x
            if sum(1 for i in range(2, n) if fr[i - 2][k] == fr[i][k] and fr[i - 1][k] != fr[i][k]) >= 3:
                aba += 1
    print(f"{name:7s} one-frame glitches {{frame: (sampled pixels, held buttons)}}: {ev}")
    print(f"{name:7s} A-B-A pixels (every 2nd): {aba}")
if len(sys.argv) > 2 and n > 12:
    out = array.array("B", bytes(W * H * 3))
    for y in range(H):
        for x in range(W):
            k = y * W + x
            p = fr[10][k]
            r, g, b = (p >> 16) & 255, (p >> 8) & 255, p & 255
            if x < 152 or x >= 872:
                if sum(1 for i in range(2, n) if fr[i - 2][k] == fr[i][k] and fr[i - 1][k] != fr[i][k]) >= 3:
                    r, g, b = 255, 0, 255
            else:
                r, g, b = r // 3, g // 3, b // 3
            o = ((H - 1 - y) * W + x) * 3
            out[o], out[o + 1], out[o + 2] = b, g, r
    hdr = b"BM" + struct.pack("<IHHI", 54 + len(out), 0, 0, 54) + struct.pack("<IiiHHIIiiII", 40, W, H, 1, 24, 0, len(out), 2835, 2835, 0, 0)
    open(sys.argv[2], "wb").write(hdr + out.tobytes())
    print(f"map: {sys.argv[2]}")
