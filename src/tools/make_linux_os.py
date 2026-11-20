#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/make_linux_os.py [x86_64|aarch64] [OUTDIR]: Mutable Dreams as a program in Mutable Dreams Linux (or any
FreeDesktop Linux with SDL2): the kit (package/linux-os) and the shared files with the build for that machine -
x86_64: build/linux64 (glibc 2.34+, Ubuntu 22.04 and newer); aarch64: build/rgb20s (glibc 2.31+). A zip laid out
to run install.sh from."""
import re
import os, sys, zipfile
from pathlib import Path
HERE = Path(__file__).resolve().parent.parent
COMMON, KIT = HERE / "package/knulli/mutabledreams", HERE / "package/linux-os"
BRICK_ONLY = {"mutable-dreams", "libs", "menu-cover.png", "menu-screenshot.png", "save", "log.txt", "log.previous.txt", "brick-buttons.txt"}
version = re.search(r"(?:test build|release) ([0-9][^ \n]*)", (COMMON / "README.txt").read_text())[1]
arch = sys.argv[1] if len(sys.argv) > 1 else "x86_64"
out = Path(sys.argv[2] if len(sys.argv) > 2 else HERE.parent) / f"Mutable-Dreams-Linux-OS-{arch}-{version}.zip"
prog = HERE / ("build/linux64/mutable-dreams" if arch == "x86_64" else "build/rgb20s/mutable-dreams")
def add(z, path, arc):
    info = zipfile.ZipInfo(arc, date_time=(2026, 11, 16, 0, 0, 0)); info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = (0o100000 | (0o755 if os.access(path, os.X_OK) else 0o644)) << 16
    z.writestr(info, Path(path).read_bytes())
with zipfile.ZipFile(out, "w") as z:
    for p in sorted(KIT.iterdir()):
        if p.is_file() and not p.name.startswith("._"):
            add(z, p, f"mutable-dreams/{p.name}")
    for p in sorted(COMMON.rglob("*")):
        rel = p.relative_to(COMMON)
        if p.is_dir() or p.name.startswith("._") or p.name == ".DS_Store" or rel.parts[0] in BRICK_ONLY:
            continue
        add(z, p, f"mutable-dreams/mutabledreams/{rel.as_posix()}")
    add(z, prog, "mutable-dreams/mutabledreams/mutable-dreams")
    for pdf in ("Mutable-Dreams-Handbuch-DE.pdf", "Mutable-Dreams-Manual-EN.pdf"):   # (0.80) the manual (tools/make_manual.py)
        if (HERE.parent / "Handbuch" / pdf).exists():
            add(z, HERE.parent / "Handbuch" / pdf, f"mutable-dreams/doc/{pdf}")
print(out.name, out.stat().st_size)
