#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutable Dreams: the test packages for the other handhelds, from the same files as the Brick's.

usage: make_packages.py OUTDIR [rgb20s] [miyoo-plus] [miyoo-mini]      (default: all built ones)

The shared content (texts, worlds, archive catalogue, test voices, licences) comes from package/knulli/mutabledreams;
each handheld adds its program (build/<target>/mutable-dreams), its launcher and its own files from package/<name>/.
A zip per handheld, laid out as it goes onto the card, no ._ files, entries in a fixed order with a fixed date."""
import re
import shutil
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
COMMON = HERE / "package/knulli/mutabledreams"
BRICK_ONLY = {"mutable-dreams", "libs", "menu-cover.png", "menu-screenshot.png", "save", "log.txt", "log.previous.txt", "brick-buttons.txt"}
version = re.search(r"(?:test build|release) ([0-9][^ \n]*)", (COMMON / "README.txt").read_text())[1]

# name: (build dir, package dir, zip name, the folder they go into on the card, the app folder's name there)
PROFILES = {
    "rgb20s": ("rgb20s", "amberelec", "RGB20S-AmberELEC", "ports", "mutabledreams"),
    "miyoo-plus": ("miyoo", "onion-plus", "Miyoo-Mini-Plus-Onion", "", "Roms/PORTS/Games/MutableDreams"),
    "miyoo-mini": ("miyoo", "onion-mini", "Miyoo-Mini-Onion", "", "Roms/PORTS/Games/MutableDreams"),
    # (1.5.9) the Anbernic H700 handhelds with Knulli: the Brick's own program, their own launcher (screen, keys)
    "h700": ("brick", "knulli-h700", "Anbernic-H700-Knulli", "", "mutabledreams"),
}


def add_tree(z, src, arc):
    for p in sorted(src.rglob("*")):
        if p.is_dir() or p.name.startswith("._") or p.name == ".DS_Store":
            continue
        rel = p.relative_to(src)
        if rel.parts[0] in BRICK_ONLY:
            continue
        add_file(z, p, f"{arc}/{rel.as_posix()}")


def add_file(z, p, arc, mode=None):
    info = zipfile.ZipInfo(arc, date_time=(2026, 11, 14, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    m = mode if mode is not None else (0o755 if (p.stat().st_mode & 0o111) else 0o644)
    info.external_attr = (0o100000 | m) << 16
    z.writestr(info, p.read_bytes())


def build(name, outdir):
    bdir, pdir, zname, top, app = PROFILES[name]
    exe = HERE / "build" / bdir / "mutable-dreams"
    own = HERE / "package" / pdir
    if not exe.exists() or not own.exists():
        print(f"skipped {name}: {exe if not exe.exists() else own} missing")
        return None
    out = Path(outdir) / f"Mutable-Dreams-{zname}-{version}.zip"
    pre = f"{top}/" if top else ""
    with zipfile.ZipFile(out, "w") as z:
        for p in sorted(own.rglob("*")):              # the handheld's own files: launcher, mapping, notes, libs
            if p.is_dir() or p.name.startswith("._") or p.name == ".DS_Store":
                continue
            rel = p.relative_to(own).as_posix()
            if rel.startswith("game/"):              # files that belong inside the program folder
                add_file(z, p, f"{pre}{app}/{rel[5:]}")
            else:
                add_file(z, p, f"{pre}{rel}")
        add_tree(z, COMMON, f"{pre}{app}")
        add_file(z, exe, f"{pre}{app}/mutable-dreams", 0o755)
    print(out)
    return out


if __name__ == "__main__":
    outdir = sys.argv[1] if len(sys.argv) > 1 else str(HERE.parent)
    names = sys.argv[2:] or list(PROFILES)
    for n in names:
        build(n, outdir)
