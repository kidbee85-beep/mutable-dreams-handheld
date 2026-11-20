#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/install_handheld.py RELEASE_DIR [VOLUME]: Mutable Dreams onto a handheld's card (Miyoo Mini Plus / Mini with
Onion, RGB20S with AmberELEC), over an older install.

Which card it is comes from what is on it: Onion's Roms/PORTS with our launch.sh (its first lines say MIYOO MINI PLUS
or MIYOO MINI), or AmberELEC's ports/mutabledreams. The card's log.txt is kept first (Geraete-Logs beside the release),
then the release's zip for that handheld is unpacked onto the card - only the program's own files; save/, samples,
keys.txt and the user's switch files are not in the zip and stay as they are - and every file is checked against the
zip. Nothing is deleted. The Brick (Knulli) has its own tool: install_card.py. Without VOLUME every mounted card is
looked at and the one found is taken; more than one, or none: it stops."""
import hashlib, re, shutil, sys, time, zipfile
from pathlib import Path

rel = Path(sys.argv[1]).resolve()
skip = {"Macintosh HD", "KidBunka-Entwicklung", "M-Flasher", "PortableSSD"}
vols = [Path(sys.argv[2])] if len(sys.argv) > 2 else [v for v in Path("/Volumes").iterdir() if v.name not in skip]


def kind(v):
    ls = v / "Roms/PORTS/Games/MutableDreams/launch.sh"
    if ls.exists():
        head = ls.read_text(errors="ignore")[:300]
        if "MIYOO MINI PLUS" in head:
            return "Miyoo-Mini-Plus-Onion", ls.parent
        if "MIYOO MINI" in head:
            return "Miyoo-Mini-Onion", ls.parent
    if (v / "ports/mutabledreams").is_dir():
        return "RGB20S-AmberELEC", v / "ports/mutabledreams"
    return None, None


found = [(v, *kind(v)) for v in vols if v.is_dir()]
found = [f for f in found if f[1]]
if len(found) != 1:
    sys.exit("no handheld card with Mutable Dreams found" if not found else f"more than one card: {[str(f[0]) for f in found]}")
vol, name, app = found[0]
zips = sorted(rel.glob(f"Mutable-Dreams-{name}-*.zip"))
if len(zips) != 1:
    sys.exit(f"no single {name} zip in {rel}")
zp = zips[0]
version = re.search(r"-(\d+\.\d+\.\d+)\.zip$", zp.name)[1]
old = (app / "README.txt").read_text(errors="ignore").splitlines()[0] if (app / "README.txt").exists() else "?"
print(f"card {vol}: {name}, now: {old.strip()}")
log = app / "log.txt"
if log.exists():                                 # (the card's last run, kept before anything changes)
    dst = rel.parent / "Geraete-Logs" / f"{name.split('-Onion')[0].split('-Amber')[0]}-vor-{version}-{time.strftime('%Y%m%d-%H%M')}-log.txt"
    shutil.copy(log, dst)
    print(f"log kept: {dst.name}")
z = zipfile.ZipFile(zp)
files = [i for i in z.infolist() if not i.is_dir()]
for i in files:
    d = vol / i.filename
    d.parent.mkdir(parents=True, exist_ok=True)
    d.write_bytes(z.read(i))
bad = [i.filename for i in files if hashlib.sha256((vol / i.filename).read_bytes()).digest() != hashlib.sha256(z.read(i)).digest()]
apple = [p for p in app.rglob("._*")]
new = (app / "README.txt").read_text(errors="ignore").splitlines()[0]
print(f"written {len(files)} files from {zp.name}, mismatches {len(bad)}, ._ files {len(apple)}")
print(f"now: {new.strip()}")
print("kept:", ", ".join(sorted(p.name for p in app.iterdir() if p.name in ("save", "samples", "keys.txt") or p.suffix == ".on")) or "-")
if bad:
    sys.exit(f"MISMATCH: {bad[:5]}")
