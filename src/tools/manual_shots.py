#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/manual_shots.py LANG OUTDIR: the pictures for the manual (Handbuch), taken by the headless build in LANG
(en or de): every place, the Sound Lab's modules, the playground's ten toys, the guide, the help, a tale card. Saved
as JPEG (sips) under OUTDIR/<name>.jpg. Needs build/mac/brick-headless."""
import os, subprocess, sys, tempfile, shutil
from pathlib import Path
HERE = Path(__file__).resolve().parent.parent
lang, out = sys.argv[1], Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
tmp = Path(tempfile.mkdtemp())
S = []
def shot(name, wait=600):
    S.append(f"wait {wait}")
    S.append(f"shot {tmp}/{name}.png")
cur = [0]                                        # (the menu keeps its choice between visits)
def menu(i):                                     # the jump menu's i-th stone (0 Sound Lab .. 12 Playground)
    S.append("tap MENU"); S.append("wait 300")
    S.extend(["tap DOWN"] * (i - cur[0]) if i >= cur[0] else ["tap UP"] * (cur[0] - i))
    cur[0] = i
    S.append("tap A"); S.append("wait 700")
def home():
    S.append("press B"); S.append("wait 900"); S.append("release B"); S.append("wait 500")
S += ["splash", "wait 1600"]; shot("00-start", 400)
S += ["wait 6000"]
shot("01-garden", 300)
S += ["tap START", "wait 4000"]; shot("02-garden-playing")
S += ["press R2", "wait 700"]; shot("03-garden-r2", 100); S += ["release R2"]
S += ["tap L2", "wait 300", "tap RIGHT", "tap RIGHT"]; shot("04-quickbar", 500); S += ["tap B"]
S += ["tap MENU", "wait 300", "tap DOWN", "tap DOWN"]; shot("05-menu", 500); S += ["tap B", "wait 300"]
cur[0] = 2
S += ["press FN", "wait 400", "tap A", "tap RIGHT", "tap UP"]; shot("06-playmode", 300); S += ["release FN", "wait 300"]
S += ["tap F1", "wait 900"]; shot("10-lab")
for k in range(1, 12):
    S += ["tap R1", "wait 500"]
    if k in (2, 3, 4, 5, 7, 11):
        shot(f"11-lab-{k:02d}", 400)
S += ["tap F1", "wait 500"]
names = {1: "20-mixer", 2: "21-atlas", 3: "22-sequence", 4: "24-dreammap", 5: "25-modulation", 6: "26-voice",
         7: "27-space", 9: "28-archive", 10: "29-worlds"}
for i, n in names.items():
    menu(i)
    shot(n)
    if i == 3:
        S += ["tap X", "wait 500"]; shot("23-sequence-edit"); S += ["tap X", "wait 300"]
    if i == 4:
        S += ["tap Y", "wait 1500"]; shot("24-dreammap-flight"); S += ["tap Y", "wait 300"]
    if i == 5:
        S += ["tap Y", "wait 900"]; shot("25-modulation-pond"); S += ["tap Y", "wait 300"]
    home()
menu(12)
S += ["wait 7000"]                               # (the tale's first card rises and fades)
for k in range(10):                              # the ten toys
    S += ["tap A", "wait 300", "tap A"]
    shot(f"3{k}-toy", 2500)
    S += ["tap R1", "wait 400"]
home()
S += ["guide 1"]
for k in range(1, 15):
    shot(f"40-guide-{k:02d}", 500)
    S += ["tap RIGHT"]
S += ["guide 0", "wait 300", "press SELECT", "wait 1200", "release SELECT"]
shot("50-help", 400)
S += ["tap B", "wait 300", "options 1", "tap DOWN", "tap DOWN"]; shot("60-options", 600); S += ["options 0", "press SELECT", "wait 1200", "release SELECT"]
for k in range(1, 5):
    S += ["tap R1"]; shot(f"51-help-{k}", 400)
S += ["tap B", "wait 300"]
test = tmp / "shots.txt"
test.write_text("\n".join(S) + "\n")
env = dict(os.environ, MD_LANG=lang, MD_ARCHIVE_CATALOGUE=str(HERE / "package/knulli/mutabledreams/archive/catalogue.tsv"),   # (the real catalogue: the test one says "TEST READER")
          
           MD_ARCHIVE_BASE="file://" + str(HERE / "out/fake-archive").replace(" ", "%20"))
save = tmp / "save"
shutil.copytree(HERE / "package/knulli/mutabledreams/worlds", tmp / "worlds")   # (the program reads ../worlds/worlds.txt beside save/: without it the WORLDS picture says "No worlds yet")
r = subprocess.run([str(HERE / "build/mac/brick-headless"), str(test), str(save)], cwd=HERE, env=env, capture_output=True, text=True)
if "FAIL" in r.stdout:
    print(r.stdout[-2000:])
n = 0
for p in sorted(tmp.glob("*.png")):
    subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "82", str(p), "--out", str(out / (p.stem + ".jpg"))],
                   check=True, capture_output=True)
    n += 1
shutil.rmtree(tmp)
print(n, "pictures in", out)
