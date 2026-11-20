#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/lang_set.py FILE: set (or with an empty German, remove) pairs in lang/de.txt. FILE has lines "EN<TAB>DE"."""
import sys
from pathlib import Path
P = Path(__file__).resolve().parent.parent / "package/knulli/mutabledreams/lang/de.txt"
lines = P.read_text(encoding="utf-8").split("\n")
head = [l for l in lines if l.startswith("#") or not l.strip()][:4]
pairs, order, en = {}, [], None
for l in lines:
    if l.startswith("en "): en = l[3:]
    elif l.startswith("de ") and en is not None:
        if en not in pairs: order.append(en)
        pairs[en] = l[3:]; en = None
for l in Path(sys.argv[1]).read_text(encoding="utf-8").split("\n"):
    if "\t" not in l: continue
    e, d = l.split("\t", 1)
    if not d:
        pairs.pop(e, None); continue
    if e not in pairs: order.append(e)
    pairs[e] = d
out = head[:3] + [""]
for e in order:
    if e in pairs: out += [f"en {e}", f"de {pairs[e]}"]
P.write_text("\n".join(out) + "\n", encoding="utf-8")
print(sum(1 for e in order if e in pairs), "pairs")
