#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutable Dreams for Android: what the Gradle build (android/) needs from this port, made here first.

usage: prepare_android.py

- android/app/src/main/cpp/sources.cmake: the engine, Mutable Instruments and port sources with the very lists and
  flags build.py uses (the 2.1 engine with fm1.toml's o2_cflags), as absolute paths
- android/app/src/main/assets/md/: the package's shared files (worlds, archive catalogue, texts), which the app
  copies into its folder on the device on its first start (MutableDreamsActivity)
- the generated headers (build/gen) must exist: python3 build.py headless makes them"""
import ast
import re
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
SRC = (HERE.parent.parent / "FM-1/Release-2.1/Source").resolve()
A, B, MI, GEN = SRC / "app", SRC / "base", HERE / "third_party/mi", HERE / "build/gen"
APP = HERE / "android/app/src/main"
assert (GEN / "stage_font.h").exists(), "build/gen is missing: run python3 build.py headless first"

conf = (A / "fm1.toml").read_text()
o2 = ast.literal_eval(re.search(r"^o2\s*=\s*(.+)$", conf, re.M)[1])
o2_cflags = ast.literal_eval(re.search(r"^o2_cflags\s*=\s*(.+)$", conf, re.M)[1])
mi_src = ([*sorted((MI / "clouds/dsp").glob("*.cc")), *sorted((MI / "clouds/dsp/pvoc").glob("*.cc")), MI / "clouds/resources.cc",
           *sorted((MI / "warps/dsp").glob("*.cc")), MI / "warps/resources.cc", MI / "marbles/resources.cc",
           *sorted(f for f in (MI / "beads/dsp").glob("*.cc") if f.name != "synth.cc"), MI / "beads/resources.cc",
           *sorted((MI / "plaits/dsp").rglob("*.cc")), MI / "plaits/resources.cc",
           MI / "stmlib/dsp/units.cc", MI / "stmlib/dsp/atan.cc", MI / "stmlib/utils/random.cc",
           *sorted((MI / "rings/dsp").glob("*.cc")), MI / "rings/resources.cc", *sorted((MI / "elements/dsp").glob("*.cc")),
           MI / "elements/resources.cc", HERE / "brick_mi_rings.cpp", HERE / "brick_mi_elements.cpp", HERE / "brick_mi_clouds.cpp",
           HERE / "brick_mi_warps.cpp", HERE / "brick_mi_marbles.cpp", HERE / "brick_mi_beads.cpp", HERE / "brick_mi_plaits.cpp",
           MI / "tides2/poly_slope_generator.cc", MI / "tides2/resources.cc", HERE / "brick_mi_tides.cpp"])
mi_src = [f for f in mi_src if f.exists()]


def cm(paths):
    return "\n".join(f'    "{p}"' for p in paths)


inc = [HERE, A, B / "host", B / "device", B / "include", B / "lib", GEN, A / "third_party", B / "web/emu"]
text = f"""# Made by tools/prepare_android.py from build.py's lists: do not edit, run that again.
set(MD_ENGINE_SOURCES
{cm(A / s for s in o2)}
)
set(MD_ENGINE_FLAGS {" ".join(o2_cflags)} -std=c++11 -fno-exceptions -fno-rtti)
set(MD_ENGINE_COMPAT "{A / 'compat'}")
set(MD_MI_SOURCES
{cm(mi_src)}
)
set(MD_MI_INCLUDES "{MI}" "{HERE}")
set(MD_CORE_SOURCES "{HERE / 'brick_core.c'}" "{HERE / 'brick_sdl.c'}")
set(MD_INCLUDES
{cm(inc)}
)
set(MD_SDL_DIR "{HERE / 'third_party/SDL2'}")
"""
out = APP / "cpp/sources.cmake"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(text)
print(out)

assets = APP / "assets/md"
if assets.exists():
    shutil.rmtree(assets)
common = HERE / "package/knulli/mutabledreams"
skip = {"mutable-dreams", "libs", "menu-cover.png", "menu-screenshot.png", "brick-buttons.txt", "save", "log.txt",
        "log.previous.txt"}
for p in sorted(common.rglob("*")):
    rel = p.relative_to(common)
    if p.is_dir() or rel.parts[0] in skip or p.name.startswith("._") or p.name == ".DS_Store":
        continue
    (assets / rel).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(p, assets / rel)
note = HERE / "package/android/ANDROID.txt"
if note.exists():
    shutil.copy2(note, assets / "ANDROID.txt")
(assets / "files.txt").write_text("\n".join(sorted(str(p.relative_to(assets)) for p in assets.rglob("*") if p.is_file())) + "\n")
print(assets)
