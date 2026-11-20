#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/span_callers.py BINARY ERR...: the SPAN lines of an MD_SPANSTAT build, summed per calling function (see spancost.sh)"""
import bisect, collections, subprocess, sys
b, files = sys.argv[1], sys.argv[2:]
syms = []
for l in subprocess.check_output(["nm", "-n", b], text=True).splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in "tT":
        syms.append((int(p[0], 16), p[2].lstrip("_")))
addrs = [a for a, _ in syms]
main = dict((n, a) for a, n in syms)["main"]
for f in files:
    agg = collections.defaultdict(lambda: [0, 0, 0])
    for l in open(f):
        if l.startswith("SPAN "):
            _, off, op, tr, c = l.split()
            a = main + int(off)
            n = syms[bisect.bisect_right(addrs, a) - 1][1]
            agg[n][0] += int(op); agg[n][1] += int(tr); agg[n][2] += int(c)
    tot_tr = sum(v[1] for v in agg.values()); tot_op = sum(v[0] for v in agg.values())
    print(f"== {f}: opaque {tot_op/1e6:.1f} M px, translucent {tot_tr/1e6:.1f} M px")
    for n, v in sorted(agg.items(), key=lambda kv: -kv[1][1])[:10]:
        print(f"   {n:32s} translucent {v[1]/1e6:7.2f} M  opaque {v[0]/1e6:7.2f} M  spans {v[2]}")
