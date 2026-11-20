#!/usr/bin/env python3
"""tools/symbolize_profile.py LOG.txt [BINARY]: the 'draw profile' lines of a device's log.txt with function names.
BINARY: the very program that ran (default build/brick/mutable-dreams, not stripped)."""
import re, subprocess, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent.parent
binary = sys.argv[2] if len(sys.argv) > 2 else str(HERE / "build/brick/mutable-dreams")
tools = Path.home() / "Library/Application Support/TrackWerk/Toolchains/android-sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin"
nm = subprocess.check_output([str(tools / "llvm-nm"), binary], text=True)
main = int(next(l.split()[0] for l in nm.splitlines() if l.endswith(" T main")), 16)
for line in open(sys.argv[1], errors="replace"):
    if not line.startswith("draw profile"):
        continue
    out = []
    for pct, off in re.findall(r"([\d.]+)%@([+-]\d+)", line):
        addr = main + int(off)
        name = "end of frame" if int(off) == 1 - 0 else subprocess.check_output(
            [str(tools / "llvm-symbolizer"), "--obj=" + binary, "--functions=linkage", hex(addr)], text=True).splitlines()[0]
        out.append(f"{pct:>5}%  {name}")
    print("\n".join(out), "\n")
