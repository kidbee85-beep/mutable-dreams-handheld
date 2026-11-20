#!/usr/bin/env python3
"""The Brick's build sysroot: Debian 12 (bookworm) arm64 libc, libstdc++, kernel headers and SDL2, unpacked into
toolchain/sysroot. Every .deb is checked against the SHA-256 in Debian's package index (fetched over HTTPS).
glibc 2.36 is older than Knulli's, so what links here runs there; libstdc++ and libgcc are linked statically, SDL2
dynamically (Knulli's own libSDL2-2.0.so.0).

  python3 tools/make_sysroot.py
"""
from pathlib import Path
import hashlib, io, lzma, os, subprocess, sys, tarfile, urllib.request

HERE = Path(__file__).resolve().parents[1]
TC = HERE / "toolchain"
DL = TC / "downloads/debian-arm64"
ROOT = TC / "sysroot"
MIRROR = "https://deb.debian.org/debian"
PACKAGES = ["libc6", "libc6-dev", "linux-libc-dev", "libgcc-12-dev", "libgcc-s1", "libstdc++-12-dev", "libstdc++6",
            "libsdl2-2.0-0", "libsdl2-dev"]
DL.mkdir(parents=True, exist_ok=True)


def fetch(url, dest):
    if not dest.exists():
        with urllib.request.urlopen(url) as r:
            dest.write_bytes(r.read())
    return dest


index = fetch(f"{MIRROR}/dists/bookworm/main/binary-arm64/Packages.xz", DL / "Packages.xz")
info, cur = {}, {}
for line in lzma.decompress(index.read_bytes()).decode("utf-8", "replace").splitlines() + [""]:
    if not line:
        if cur.get("Package"):
            info.setdefault(cur["Package"], cur)
        cur = {}
    elif ": " in line and not line.startswith(" "):
        k, v = line.split(": ", 1)
        cur[k] = v

ROOT.mkdir(parents=True, exist_ok=True)
for name in PACKAGES:
    p = info.get(name)
    if not p:
        sys.exit(f"{name}: not in the index")
    deb = fetch(f"{MIRROR}/{p['Filename']}", DL / Path(p["Filename"]).name)
    if hashlib.sha256(deb.read_bytes()).hexdigest() != p["SHA256"]:
        deb.unlink()
        sys.exit(f"{name}: SHA-256 mismatch, file removed")
    # a .deb is an ar archive: data.tar.* holds the files
    data = deb.read_bytes()
    assert data[:8] == b"!<arch>\n"
    at = 8
    while at < len(data):
        hdr = data[at:at + 60]
        fname = hdr[:16].decode().strip().rstrip("/")
        size = int(hdr[48:58].decode().strip())
        body = data[at + 60:at + 60 + size]
        if fname.startswith("data.tar"):
            mode = "r:xz" if fname.endswith(".xz") else "r:zst" if fname.endswith(".zst") else "r:*"
            with tarfile.open(fileobj=io.BytesIO(body), mode=mode) as t:
                for m in t.getmembers():
                    if m.name.startswith(("./usr/share/doc", "./usr/share/man", "./usr/share/lintian")):
                        continue
                    t.extract(m, ROOT, set_attrs=False)
        at += 60 + size + (size & 1)
    print(f"{name} {p['Version']}")

print(ROOT)
