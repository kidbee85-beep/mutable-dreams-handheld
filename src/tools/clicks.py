"""Clicks where a change happens: the high-frequency energy (first difference) in 5 ms windows near given times,
against the median window of the whole file.   python3 tools/clicks.py FILE T1 [T2 ...]"""
import wave, struct, sys, math
w = wave.open(sys.argv[1]); n = w.getnframes(); s = struct.unpack(f"<{n*2}h", w.readframes(n))
m = [(s[2*i] + s[2*i+1]) / 65536 for i in range(n)]
d = [m[i] - m[i-1] for i in range(1, n)]
win = 220
e = [math.sqrt(sum(x*x for x in d[i:i+win]) / win) + 1e-12 for i in range(0, len(d) - win, win)]
med = sorted(e)[len(e) // 2]
out = []
for t in map(float, sys.argv[2:]):
    i = int(t * 44100 / win)
    peak = max(e[max(0, i - 4):i + 8])
    out.append(f"{t:.2f} s: {20*math.log10(peak/med):+.1f} dB")
print(sys.argv[1].split("/")[-1], "HF energy over the median:", ", ".join(out), f"(loudest anywhere {20*math.log10(max(e)/med):+.1f} dB)")
