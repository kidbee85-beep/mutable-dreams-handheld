#!/usr/bin/env python3
"""tools/sample_callers.py SAMPLE.txt FUNC...: from macOS `sample` output, who calls FUNC, with sample counts"""
import re, sys, collections
lines = open(sys.argv[1]).read().split('Sort by top of stack')[0].splitlines()
pat = re.compile(r'^([\s+!:|]*)(\d+)\s+(\S+)')
for want in sys.argv[2:]:
    tally = collections.Counter(); stack = []
    for l in lines:
        m = pat.match(l)
        if not m:
            continue
        depth, n, fn = len(m.group(1)), int(m.group(2)), m.group(3)
        while stack and stack[-1][0] >= depth:
            stack.pop()
        if fn == want and stack:
            tally[stack[-1][1]] += n
        stack.append((depth, fn))
    print(want, ':', ', '.join(f'{f} {n}' for f, n in tally.most_common(8)))
