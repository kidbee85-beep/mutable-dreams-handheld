"""What a Sound Lab setting adds: the same 20 s twice from the same start, neutral and with the settings, and the
level of the difference.   python3 tools/labdiff.py NAME 'PARTICLES.MIX=100' ['PARTICLES.MODE=3' ...]"""
import struct, os, shutil, subprocess, sys, wave, math
def run(tag, sets):
    d = f"out/diff-{tag}"; shutil.rmtree(d, ignore_errors=True)
    lines = ["wait 2000"] + [f"lab {k.replace(' ', '_')} {v}" for k, v in (x.rsplit("=", 1) for x in sets)] + ["tap START", "wav out/diff.wav", "wait 20000", "wavstop"]
    open("tests/diff.txt", "w").write("\n".join(lines) + "\n")
    r = subprocess.run(["./build/mac/brick-headless", "tests/diff.txt", d], capture_output=True, text=True)
    if "FAIL" in r.stdout: print(r.stdout); sys.exit(1)
    w = wave.open("out/diff.wav"); n = w.getnframes()
    return struct.unpack(f"<{n*2}h", w.readframes(n))
a = run(sys.argv[1] + "-0", []); b = run(sys.argv[1] + "-1", sys.argv[2:])
n = min(len(a), len(b)); rms = lambda s: math.sqrt(sum(x * x for x in s) / len(s)) / 32768 + 1e-12
d = [b[i] - a[i] for i in range(n)]
dc = sum(b) / len(b) / 32768
print(f"{sys.argv[1]}: dry {20*math.log10(rms(a[:n])):.1f} dB, with {20*math.log10(rms(b[:n])):.1f} dB, added {20*math.log10(rms(d)):.1f} dB, peak {20*math.log10(max(abs(x) for x in b)/32768+1e-12):.1f} dB, dc {dc:+.4f}")
