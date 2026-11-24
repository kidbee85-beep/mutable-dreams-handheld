#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/make_samples.py [OUTDIR]: the sample pack that ships with Mutable Dreams (1.6).

Ten short sounds, made by arithmetic alone (sines, filtered noise, envelopes; a fixed random seed): no recording, no
third-party material, so there is nothing to clear: they are made by Kid Bunka's tool and given away under CC0 (public
domain dedication) as well as the project's GPL. 44.1 kHz, mono, 16 bit; every file is a few seconds (the pack is about
4.5 MB). They fill the SAMPLES module of the Sound Lab, which is empty without files of your own.

Default OUTDIR: package/knulli/mutabledreams/samples (next to this script's folder).
"""
import math, os, random, struct, sys, wave

SR = 44100


def write(path, data, gain=0.9):
    peak = max(1e-9, max(abs(x) for x in data))
    k = gain / peak
    with wave.open(path, "wb") as w:
        w.setnchannels(1), w.setsampwidth(2), w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, x * k)) * 32767)) for x in data))


def env(t, a, d):
    return (min(1.0, t / a) if a > 0 else 1.0) * math.exp(-t / d)


def lowpass(data, fc):
    a = 1.0 - math.exp(-2.0 * math.pi * fc / SR)
    y, out = 0.0, []
    for x in data:
        y += a * (x - y)
        out.append(y)
    return out


def highpass(data, fc):
    lp = lowpass(data, fc)
    return [x - l for x, l in zip(data, lp)]


def noise(rng, n):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def kalimba(f=261.63, secs=3.0):
    n = int(SR * secs)
    parts = [(1.0, 1.0, 1.4), (5.4, 0.35, 0.5), (12.5, 0.12, 0.2), (2.0, 0.2, 0.9)]
    return [sum(a * math.sin(2 * math.pi * f * r * i / SR) * env(i / SR, 0.002, d * 1.3) for r, a, d in parts) for i in range(n)]


def glass_bell(f=261.63, secs=4.5):
    n = int(SR * secs)
    out = []
    for i in range(n):
        t = i / SR
        idx = 2.4 * math.exp(-t / 0.8)
        mod = math.sin(2 * math.pi * f * 3.5 * t) * idx
        out.append(math.sin(2 * math.pi * f * t + mod) * env(t, 0.004, 1.6))
    return out


def soft_pad(f=130.81, secs=8.0):
    n = int(SR * secs)
    out = []
    for i in range(n):
        t = i / SR
        s = 0.0
        for k, det in enumerate((-0.004, 0.0, 0.0035)):
            for h, a in ((1, 1.0), (2, 0.5), (3, 0.25), (5, 0.1)):
                s += a * math.sin(2 * math.pi * f * h * (1 + det) * t + k)
        swell = 0.55 + 0.45 * math.sin(2 * math.pi * t / secs)        # one slow breath over the loop
        out.append(s * swell * min(1.0, t / 0.8) * min(1.0, (secs - t) / 0.8))
    return lowpass(out, 1400)


def rain_loop(secs=8.0):
    rng = random.Random(11)
    n = int(SR * secs)
    bed = lowpass(highpass(noise(rng, n), 900), 7000)
    out = [x * 0.5 for x in bed]
    for _ in range(int(secs * 90)):                                   # single drops
        i = rng.randrange(n - 800)
        a = rng.uniform(0.2, 1.0)
        for j in range(500):
            out[i + j] += a * rng.uniform(-1, 1) * math.exp(-j / 70.0) * 0.5
    return out


def wind_loop(secs=8.0):
    rng = random.Random(23)
    n = int(SR * secs)
    out = lowpass(noise(rng, n), 600)
    return [x * (0.45 + 0.55 * math.sin(2 * math.pi * (i / SR) / secs * 2.0 + 0.7) ** 2) for i, x in enumerate(out)]


def water_drops(secs=8.0):
    rng = random.Random(5)
    n = int(SR * secs)
    out = [0.0] * n
    t = 0.2
    while t < secs - 0.6:
        f0 = rng.uniform(500, 1400)
        i0, a = int(t * SR), rng.uniform(0.4, 1.0)
        for j in range(int(0.25 * SR)):
            tt = j / SR
            out[i0 + j] += a * math.sin(2 * math.pi * (f0 * (1 + 2.2 * tt) * tt)) * math.exp(-tt / 0.06)
        t += rng.uniform(0.35, 1.3)
    return out


def soft_kick(secs=0.7):
    n = int(SR * secs)
    out, ph = [], 0.0
    for i in range(n):
        t = i / SR
        ph += 2 * math.pi * (46 + 90 * math.exp(-t / 0.05)) / SR
        out.append(math.sin(ph) * env(t, 0.001, 0.18))
    return out


def shaker(secs=0.3):
    rng = random.Random(3)
    n = int(SR * secs)
    x = highpass(noise(rng, n), 5000)
    return [v * (min(1.0, (i / SR) / 0.02)) * math.exp(-(i / SR) / 0.07) for i, v in enumerate(x)]


def crackle_loop(secs=6.0):
    rng = random.Random(41)
    n = int(SR * secs)
    out = [rng.uniform(-1, 1) * 0.02 for _ in range(n)]
    for _ in range(int(secs * 28)):
        i = rng.randrange(n - 60)
        a = rng.uniform(0.2, 1.0) * rng.choice((-1, 1))
        for j in range(40):
            out[i + j] += a * math.exp(-j / 6.0)
    return highpass(out, 300)


def sub_drone(f=65.41, secs=8.0):
    n = int(SR * secs)
    out = []
    for i in range(n):
        t = i / SR
        s = math.sin(2 * math.pi * f * t) + 0.35 * math.sin(2 * math.pi * f * 2 * t) + 0.12 * math.sin(2 * math.pi * f * 3.01 * t)
        out.append(s * (0.8 + 0.2 * math.sin(2 * math.pi * t * 0.25)))
    return out


PACK = [
    ("kalimba-c4.wav", kalimba), ("glass-bell-c4.wav", glass_bell), ("soft-pad-c3.wav", soft_pad),
    ("rain-loop.wav", rain_loop), ("wind-loop.wav", wind_loop), ("water-drops.wav", water_drops),
    ("soft-kick.wav", soft_kick), ("shaker.wav", shaker), ("vinyl-crackle-loop.wav", crackle_loop),
    ("sub-drone-c2.wav", sub_drone),
]

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "package", "knulli", "mutabledreams", "samples")
    os.makedirs(out, exist_ok=True)
    for name, fn in PACK:
        write(os.path.join(out, name), fn())
        print("wrote", name, os.path.getsize(os.path.join(out, name)) // 1024, "KB")
