"""Test WAV files for the SAMPLES module (synthetic, for tests only; never shipped):
   python3 tools/make_test_samples.py DIR"""
import math, os, random, struct, sys
d = sys.argv[1]; os.makedirs(d, exist_ok=True)
def write(name, rate, chans, fmt, frames):
    bits = {"s16": 16, "s24": 24, "f32": 32}[fmt]
    data = bytearray()
    for fr in frames:
        for c in range(chans):
            v = max(-1.0, min(1.0, fr[c]))
            if fmt == "s16": data += struct.pack("<h", int(v * 32767))
            elif fmt == "s24": data += struct.pack("<i", int(v * 8388607))[:3]
            else: data += struct.pack("<f", v)
    tag = 3 if fmt == "f32" else 1
    hdr = b"RIFF" + struct.pack("<I", 36 + 8 + 12 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, tag, chans, rate, rate * chans * bits // 8, chans * bits // 8, bits)
    hdr += b"LIST" + struct.pack("<I", 4) + b"INFO"          # a chunk to skip
    open(os.path.join(d, name), "wb").write(hdr + b"data" + struct.pack("<I", len(data)) + data)
random.seed(7)
# a rain-like bed: filtered noise and drops, 48 kHz 24-bit stereo, 12 s
lp = [0.0, 0.0]; rain = []
for i in range(48000 * 12):
    fr = []
    for c in range(2):
        lp[c] += (random.uniform(-1, 1) - lp[c]) * 0.35
        fr.append(lp[c] * 0.25)
    rain.append(fr)
for k in range(140):
    at = random.randrange(48000 * 12 - 4800); f = random.uniform(900, 2600); pan = random.random()
    for j in range(2400):
        s = math.sin(2 * math.pi * f * j / 48000) * math.exp(-j / 300) * 0.4
        rain[at + j][0] += s * (1 - pan); rain[at + j][1] += s * pan
write("rain-field.wav", 48000, 2, "s24", rain)
# a glass pluck on C4, 22.05 kHz 16-bit mono, 2 s
g = []
for i in range(22050 * 2):
    t = i / 22050
    g.append([sum(math.sin(2 * math.pi * 261.63 * h * t) / h for h in (1, 2, 3, 5)) * math.exp(-t * 2.5) * 0.5])
write("glass-pluck.wav", 22050, 1, "s16", g)
# a soft pad, 44.1 kHz float stereo, 6 s
p = []
for i in range(44100 * 6):
    t = i / 44100; e = min(1, t / 1.5) * min(1, (6 - t) / 1.5)
    l = sum(math.sin(2 * math.pi * f * t) for f in (130.81, 196.0, 261.63)) * 0.15 * e
    r = sum(math.sin(2 * math.pi * f * 1.003 * t) for f in (130.81, 196.0, 261.63)) * 0.15 * e
    p.append([l, r])
write("pad-float.wav", 44100, 2, "f32", p)
open(os.path.join(d, "broken.wav"), "wb").write(b"this is not a wav file")
open(os.path.join(d, "notes.txt"), "w").write("not a wav: ignored")
print(sorted(os.listdir(d)))
