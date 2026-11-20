#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/make_sdl_stub.py: toolchain/linux64-sdk for the desktop build - SDL2's headers (from rgb20s-sdk, SDL 2.26)
and a stand-in libSDL2-2.0.so.0 for x86_64 that has every SDL function the program calls and nothing in them. It
is only linked against (its soname goes into the program); at run time the system's real SDL2 is loaded."""
import os, re, shutil, subprocess
from pathlib import Path
HERE = Path(__file__).resolve().parent.parent
SDK = HERE / "toolchain/linux64-sdk"
ZIG = os.environ.get("ZIG", str(Path.home() / "Library/Application Support/TrackWerk/Toolchains/zig-aarch64-macos-0.14.1/zig"))
src = HERE / "toolchain/rgb20s-sdk/include"
if not (SDK / "include").exists():
    shutil.copytree(src, SDK / "include")
names = set()
for f in (HERE / "brick_sdl.c",):                  # every SDL_ call the front end makes
    names |= set(re.findall(r"\b(SDL_[A-Za-z0-9_]+)\s*\(", f.read_text()))
names = {n for n in names if not n.isupper() and not n.startswith("SDL_HINT")}   # (no macros)
nm = Path.home() / "Library/Application Support/TrackWerk/Toolchains/android-sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm"
arm = HERE / "build/rgb20s/mutable-dreams"         # (and what the ARM build really links: macros such as SDL_zero)
if nm.exists() and arm.exists():
    for l in subprocess.check_output([str(nm), "-D", "-u", str(arm)], text=True).splitlines():
        w = l.split()[-1].split("@")[0]
        if w.startswith("SDL_"):
            names.add(w)
names = sorted(names)
(SDK / "lib").mkdir(parents=True, exist_ok=True)
stub = SDK / "lib/sdl_stub.c"
stub.write_text("/* stand-in for linking only (tools/make_sdl_stub.py) */\n" + "".join(f"void {n}(void) {{}}\n" for n in names))
subprocess.run([ZIG, "cc", "-target", "x86_64-linux-gnu.2.35", "-shared", "-fPIC", "-Wl,-soname,libSDL2-2.0.so.0",
                "-o", str(SDK / "lib/libSDL2-2.0.so.0"), str(stub)], check=True)
print(len(names), "functions in", SDK / "lib/libSDL2-2.0.so.0")
