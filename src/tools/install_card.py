#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutable Dreams for Trimui Brick: put the package onto a Knulli card (mounted on the Mac).
usage: install_card.py PORTS_DIR   (e.g. /Volumes/SHARE/roms/ports)

Copies 'Mutable Dreams.sh' and mutabledreams/ (the program, texts, worlds, catalogue, launcher pictures) over an
older install, never touching mutabledreams/save/, samples the user added, or log files. Adds (or updates) the
entry in gamelist.xml with picture and description, after a dated backup of gamelist.xml beside it. No ._ files."""
import re
import os
import shutil
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
PKG = HERE / "package/knulli"
ports = Path(sys.argv[1])
assert (ports / "gamelist.xml").exists() or ports.is_dir(), ports
version = re.search(r"(?:test build|release) ([0-9][^ \n]*)", (PKG / "mutabledreams/README.txt").read_text())[1]

KEEP = {"save", "log.txt", "log.previous.txt"}
src, dst = PKG / "mutabledreams", ports / "mutabledreams"
for root, dirs, files in os.walk(src):
    rel = Path(root).relative_to(src)
    if rel.parts and rel.parts[0] in KEEP:
        continue
    (dst / rel).mkdir(parents=True, exist_ok=True)
    for f in files:
        if f.startswith("._") or f == ".DS_Store" or (not rel.parts and f in KEEP):
            continue
        s, d = Path(root) / f, dst / rel / f
        if rel.parts[:1] == ("samples",) and d.exists():
            continue                             # (what is already in samples stays as it is)
        shutil.copyfile(s, d)
shutil.copyfile(PKG / "Mutable Dreams.sh", ports / "Mutable Dreams.sh")

DESC = (f"Mutable Dreams {version} by Kid Bunka - a generative garden of sound for the Trimui Brick, grown from "
        "Mutable Dreams 2.1 (FM-1). Six tracks, 272 Biomes, a Sound Lab with Mutable Instruments' Plaits, Rings, "
        "Elements, Clouds, Beads, Warps, Marbles and Grids, twelve SYNTH instruments, two FX racks, spoken voices "
        "from the archive, 128 worlds, a playground of ten toys and games, a dream tale, dream journeys and a sleep timer.\n"
        "START play - F1 Sound Lab - F2 mixer - MENU where to - hold SELECT: help.")
gl = ports / "gamelist.xml"
if gl.exists():
    shutil.copyfile(gl, ports / f"gamelist.xml.vor-mutable-dreams-{time.strftime('%Y%m%d-%H%M%S')}")
    tree = ET.parse(gl)
    root = tree.getroot()
else:
    root = ET.Element("gameList")
    tree = ET.ElementTree(root)
game = next((g for g in root.findall("game") if (g.findtext("path") or "") == "./Mutable Dreams.sh"), None)
if game is None:
    game = ET.SubElement(root, "game")
    ET.SubElement(game, "path").text = "./Mutable Dreams.sh"
for tag, text in (("name", f"Mutable Dreams {version}"), ("desc", DESC),
                  ("image", "./mutabledreams/menu-cover.png"), ("thumbnail", "./mutabledreams/menu-cover.png"), ("developer", "Kid Bunka"),
                  ("publisher", "Kid Bunka"), ("genre", "Music")):
    e = game.find(tag)
    if e is None:
        e = ET.SubElement(game, tag)
    e.text = text
ET.indent(tree, "\t")
tree.write(gl, encoding="utf-8", xml_declaration=True)
for p in ports.rglob("._*"):
    if "mutabledreams" in p.parts or p.name == "._Mutable Dreams.sh" or p.name == "._gamelist.xml":
        p.unlink()
os.sync()
print(f"installed Mutable Dreams {version} into {ports}")
