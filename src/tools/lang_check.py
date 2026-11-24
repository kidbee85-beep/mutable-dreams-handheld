#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/lang_check.py CODE [FILE]: checks a language table (package/knulli/mutabledreams/lang/CODE.txt) against de.txt's English
keys: every English text has its translation, '#' (a number) and '%d'-style fields agree, only characters the stage's faces
have (ASCII 32-126 and Latin-1 196-252 without the multiplication and division signs) are used, and a translation much
longer than its English is listed (the screen is tight). Exit 1 on a hard error. CODE: de, fr, es, ..."""
import re, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent.parent
code = sys.argv[1]
path = Path(sys.argv[2]) if len(sys.argv) > 2 else HERE / f"package/knulli/mutabledreams/lang/{code}.txt"
ref = HERE / "package/knulli/mutabledreams/lang/de.txt"
def pairs(p, c):
    out, en, order = {}, None, []
    for n, l in enumerate(p.read_text(encoding="utf-8").split("\n"), 1):
        l = l.rstrip("\r")
        if l.startswith("en "):
            en = l[3:]
        elif l.startswith(c + " ") and en is not None:
            out[en] = (l[3:], n); order.append(en); en = None
    return out, order
want, _ = pairs(ref, "de")
have, _ = pairs(path, code)
bad = 0
miss = [k for k in want if k not in have]
extra = [k for k in have if k not in want]
if miss: print(f"MISSING {len(miss)}: {miss[:5]}"); bad += 1
if extra: print(f"EXTRA {len(extra)} (not in de.txt): {extra[:5]}")
okc = lambda ch: 32 <= ord(ch) <= 126 or (196 <= ord(ch) <= 252 and ord(ch) not in (215, 247))
long_ = []
for k, (v, n) in have.items():
    if k.count("#") != v.count("#"): print(f"line {n}: '#' count differs: {k!r} -> {v!r}"); bad += 1
    if re.findall(r"%[sdiucfx]", k) != re.findall(r"%[sdiucfx]", v): print(f"line {n}: % fields differ: {k!r} -> {v!r}"); bad += 1
    for ch in v:
        if not okc(ch): print(f"line {n}: character {ch!r} (U+{ord(ch):04X}) is not in the stage's faces: {v!r}"); bad += 1; break
    if k.upper() == k and len(v) > max(14, len(k) * 1.5) : long_.append((len(v) / max(len(k), 1), k, v))
    if not v.strip(): print(f"line {n}: empty translation of {k!r}"); bad += 1
long_.sort(reverse=True)
print(f"{code}: {len(have)} of {len(want)} texts; {len(long_)} much longer than the English (first: {[(round(r, 2), k, v) for r, k, v in long_[:3]]})")
print("OK" if not bad else f"{bad} problems")
sys.exit(1 if bad else 0)
