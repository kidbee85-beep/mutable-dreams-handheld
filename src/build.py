#!/usr/bin/env python3
"""Build Mutable Dreams for Trimui Brick from the unchanged Mutable Dreams 2.1 sources.

  python3 build.py headless        the test runner for this Mac (no window, no sound card): build/mac/brick-headless
  python3 build.py sdl             the SDL2 program for this Mac (needs SDL2: SDL2_DIR or sdl2-config)
  python3 build.py brick           the SDL2 program for the Brick (aarch64 Linux): CROSS=aarch64-linux-gnu- and
                                   SYSROOT=/path (with SDL2), e.g. inside a tg5040 / Knulli toolchain
  python3 build.py rgb20s          Powkiddy RGB20S (AmberELEC / ROCKNIX / ArkOS): aarch64, glibc 2.31, Cortex-A35
  python3 build.py linux64         a desktop PC with Mutable Dreams Linux (x86_64, glibc 2.35+, the system's SDL2)
  python3 build.py miyoo           Miyoo Mini / Mini Plus (OnionOS): 32-bit ARMv7, glibc 2.28, Cortex-A7
                                   (both with Zig 0.14.1 as the cross compiler, as TrackWerk builds for the same devices:
                                   ZIG=path, default ~/Library/Application Support/TrackWerk/Toolchains/zig-aarch64-macos-0.14.1/zig;
                                   SDL from toolchain/rgb20s-sdk (TrimUI SDK, SDL 2.26) and toolchain/miyoo-sdk (SDL 2.0.20 mmiyoo))

The 2.1 tree (FM-1/Release-2.1/Source) is only read. Generated headers and objects stay under build/.
Fonts need Pillow: the local .venv has it (python3 -m venv .venv && .venv/bin/pip install Pillow).
"""
from pathlib import Path
import os, re, ast, shutil, subprocess, sys

HERE = Path(__file__).resolve().parent
SRC = Path(os.environ.get("MD21_SOURCE", HERE.parent.parent / "FM-1/Release-2.1/Source")).resolve()
A, B = SRC / "app", SRC / "base"
target = sys.argv[1] if len(sys.argv) > 1 else "headless"
OUT = HERE / "build" / ({"headless": "mac", "sdl": "mac-sdl", "brick": "brick", "rgb20s": "rgb20s", "miyoo": "miyoo",
                         "studio": "mac", "headless-arm": "headless-arm", "linux64": "linux64"}[target]
                       + os.environ.get("OUT_SUFFIX", ""))
# the other handhelds: Zig's clang for the target and the glibc no newer than the device's (TrackWerk's profiles)
ZIG_PROFILES = {"rgb20s": ("aarch64-linux-gnu.2.31", "cortex_a35", "rgb20s-sdk"),
                "miyoo": ("arm-linux-gnueabihf.2.28", "cortex_a7", "miyoo-sdk"),
                # (0.73) a desktop PC, Mutable Dreams Linux (Ubuntu Studio 22.04+, x86_64): the system's SDL2 at run
                # time; for linking, toolchain/linux64-sdk holds SDL's headers and a stand-in libSDL2-2.0.so.0
                # with the same soname (tools/make_sdl_stub.py)
                "linux64": ("x86_64-linux-gnu.2.35", "x86_64_v2", "linux64-sdk"),
                # the test runner for 32-bit ARM, static (musl), to run in a Cortex-A7 virtual machine (tools/vm_test.py)
                "headless-arm": ("arm-linux-musleabihf", "cortex_a7", None)}
GEN = HERE / "build/gen"
OUT.mkdir(parents=True, exist_ok=True)
GEN.mkdir(parents=True, exist_ok=True)

conf = (A / "fm1.toml").read_text()
o2 = ast.literal_eval(re.search(r"^o2\s*=\s*(.+)$", conf, re.M)[1])
o2_cflags = ast.literal_eval(re.search(r"^o2_cflags\s*=\s*(.+)$", conf, re.M)[1])
fontset = re.search(r'^font_set\s*=\s*"(.+)"', conf, re.M)[1]

py = HERE / ".venv/bin/python"
py = str(py) if py.exists() else sys.executable
if not (GEN / "felucca_font.h").exists():
    subprocess.run([py, B / "tools/gen_font.py", GEN / "felucca_font.h"], check=True,
                   env={**os.environ, "X0X_FONTSET": fontset}, stdout=subprocess.DEVNULL)
if not (GEN / "ann_words.h").exists():
    subprocess.run([py, B / "tools/gen_words.py", A / "words.tsv", HERE / "build/words", GEN / "ann_words.h"],
                   check=True, stdout=subprocess.DEVNULL)

if not (GEN / "splash_font.h").exists():
    subprocess.run([py, HERE / "tools/gen_splash_font.py", B, GEN / "splash_font.h"], check=True, stdout=subprocess.DEVNULL)
if not (GEN / "stage_font.h").exists() or (GEN / "stage_font.h").stat().st_mtime < (HERE / "tools/gen_stage_font.py").stat().st_mtime:
    subprocess.run([py, HERE / "tools/gen_stage_font.py", B, GEN / "stage_font.h"], check=True, stdout=subprocess.DEVNULL)
if not (GEN / "grids_map.h").exists():
    subprocess.run([py, HERE / "tools/gen_grids.py", HERE / "third_party/mi/grids/resources.cc", GEN / "grids_map.h"], check=True, stdout=subprocess.DEVNULL)
subprocess.run([py, HERE / "tools/gen_itinera.py", A, GEN / "itinera_map.h"], check=True, stdout=subprocess.DEVNULL)
cross = os.environ.get("CROSS", "")
cc = os.environ.get("CC", cross + ("gcc" if cross else "clang"))
cxx = os.environ.get("CXX", cross + ("g++" if cross else "clang++"))
TC = HERE / "toolchain"
if target in ZIG_PROFILES:
    ztarget, zcpu, zsdk = ZIG_PROFILES[target]
    zig = os.environ.get("ZIG", str(Path.home() / "Library/Application Support/TrackWerk/Toolchains/zig-aarch64-macos-0.14.1/zig"))
    for name, tool in (("cc", "cc"), ("c++", "c++")):   # (one-word compilers for the calls below: small wrapper scripts)
        w = OUT / f"zig-{name}"
        w.write_text(f'#!/bin/sh\nexec "{zig}" {tool} -target {ztarget} -mcpu={zcpu} "$@"\n')
        w.chmod(0o755)
    cc, cxx = str(OUT / "zig-cc"), str(OUT / "zig-c++")
# The Brick without a GCC cross toolchain: this Mac's clang targets aarch64 Linux, lld links, and toolchain/sysroot
# holds Debian 12 arm64's libc, libstdc++ and SDL2 (tools/make_sysroot.py). glibc 2.36: runs on Knulli's newer one.
brick_clang = target == "brick" and not cross and (TC / "sysroot").exists()
if brick_clang:
    # The Codex folder may sit in ~/Documents or (with iCloud Desktop & Documents) in iCloud Drive: find it next to this port.
    lld_rel = "2026-10-18/erstelle-eine-machbarkeitsstudie-f-r-eine/work/toolchain/wasi-sdk-34.0-arm64-macos/bin/ld.lld"
    lld_cands = [d / lld_rel for d in HERE.resolve().parents if d.name == "Codex"] + [Path.home() / "Documents/Codex" / lld_rel]
    # (1.5.6) that old toolchain folder may be gone (it was, after a clean-up): the Android NDK's lld links the same
    lld_cands = [c for c in lld_cands if c.exists()] + sorted(Path.home().glob(
        "Library/Application Support/TrackWerk/Toolchains/android-sdk/ndk/*/toolchains/llvm/prebuilt/darwin-x86_64/bin/ld.lld"))
    lld = os.environ.get("LLD", str(next((c for c in lld_cands if c.exists()), Path("ld.lld"))))
    os.environ.setdefault("SYSROOT", str(TC / "sysroot"))
inc = [f"-I{p}" for p in (HERE, A, B / "host", B / "device", B / "include", B / "lib", GEN, A / "third_party", B / "web/emu")]
flags = ["-O2", "-ffp-contract=off", "-DPLAT_HOST", "-DPLAT_WEB", "-DTEST", "-DFM1B_MIRROR=1", "-DFM1B_UAC_CH=8",
         "-ffunction-sections", "-fdata-sections", *inc]
flags += os.environ.get("EXTRA_CFLAGS", "").split()
flags += [f"-ffile-prefix-map={os.path.realpath(HERE)}=md", f"-ffile-prefix-map={HERE}=md"]   # (no private paths in a published program)
link = []
if target in ("headless", "sdl", "studio") and sys.platform == "darwin":
    sdk = os.environ.get("SDKROOT", "/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk")
    if Path(sdk).exists():
        flags += ["-isysroot", sdk]
    link += ["-Wl,-dead_strip"]
else:
    link += ["-Wl,--gc-sections"]
if target == "brick":
    if brick_clang:
        flags += ["--target=aarch64-linux-gnu"]
        link += [f"-fuse-ld={lld}", "-static-libstdc++", "-static-libgcc", "-Wl,--allow-shlib-undefined",
                 "-Wl,-rpath,$ORIGIN/libs"]
    if os.environ.get("SYSROOT"):
        flags += [f"--sysroot={os.environ['SYSROOT']}"]
    flags += ["-mcpu=cortex-a53"]
if target == "headless-arm":
    link += ["-static"]
elif target in ZIG_PROFILES:
    link += ["-Wl,--allow-shlib-undefined", "-Wl,-rpath,$ORIGIN/libs", "-s"]

sdl_cflags, sdl_libs = [], []
if target in ("sdl", "brick", *(k for k, v in ZIG_PROFILES.items() if v[2])):
    d = os.environ.get("SDL2_DIR")
    fw = TC / "mac/SDL2.framework"
    if target == "sdl" and sys.platform == "darwin" and not d and fw.exists():
        sdl_cflags = [f"-F{fw.parent}", f"-I{fw / 'Headers'}"]
        sdl_libs = [f"-F{fw.parent}", "-framework", "SDL2", f"-Wl,-rpath,{fw.parent}"]
    elif target in ZIG_PROFILES:
        sdk = TC / ZIG_PROFILES[target][2]
        sdl_cflags = [f"-I{sdk / 'include/SDL2'}", "-D_REENTRANT"]
        sdl_libs = [str(sdk / "lib/libSDL2-2.0.so.0"), "-lpthread", "-ldl"]   # (that very file: its soname is what the device has)
    elif target == "brick" and brick_clang:
        sdl_cflags = [f"-I{TC / 'sysroot/usr/include/SDL2'}", "-D_REENTRANT"]
        sdl_libs = ["-lSDL2"]
    elif d:
        sdl_cflags, sdl_libs = [f"-I{d}/include", f"-I{d}/include/SDL2"], [f"-L{d}/lib", "-lSDL2"]
    elif shutil.which(cross + "sdl2-config") or shutil.which("sdl2-config"):
        tool = shutil.which(cross + "sdl2-config") or "sdl2-config"
        sdl_cflags = subprocess.check_output([tool, "--cflags"], text=True).split()
        sdl_libs = subprocess.check_output([tool, "--libs"], text=True).split()
    else:
        raise SystemExit("SDL2 not found: set SDL2_DIR (with include/ and lib/) or put sdl2-config on PATH")

objs = []
for s in o2:   # the engine, exactly as the firmware builds it: -O3 without fast-math (fm1.toml o2_cflags)
    o = OUT / (s.replace("/", "_").replace(".", "_") + ".o")
    src = A / s
    if not o.exists() or o.stat().st_mtime < src.stat().st_mtime:
        subprocess.run([cxx, *flags, *o2_cflags, f"-I{A / 'compat'}", "-std=c++11", "-fno-exceptions", "-fno-rtti",
                        "-c", str(src), "-o", str(o)], check=True)
    objs.append(str(o))
# original Mutable Instruments DSP (MIT, third_party/mi): its newer stmlib in a namespace of its own
MI = HERE / "third_party/mi"
mi_src = ([*sorted((MI / "clouds/dsp").glob("*.cc")), *sorted((MI / "clouds/dsp/pvoc").glob("*.cc")), MI / "clouds/resources.cc",
           *sorted((MI / "warps/dsp").glob("*.cc")), MI / "warps/resources.cc", MI / "marbles/resources.cc",
           *sorted(f for f in (MI / "beads/dsp").glob("*.cc") if f.name != "synth.cc"), MI / "beads/resources.cc",
           *[f for f in sorted((MI / "plaits/dsp").rglob("*.cc"))], MI / "plaits/resources.cc",
           MI / "stmlib/dsp/units.cc", MI / "stmlib/dsp/atan.cc", MI / "stmlib/utils/random.cc", *sorted((MI / "rings/dsp").glob("*.cc")), MI / "rings/resources.cc", *sorted((MI / "elements/dsp").glob("*.cc")), MI / "elements/resources.cc", HERE / "brick_mi_rings.cpp", HERE / "brick_mi_elements.cpp", HERE / "brick_mi_clouds.cpp", HERE / "brick_mi_warps.cpp", HERE / "brick_mi_marbles.cpp", HERE / "brick_mi_beads.cpp", HERE / "brick_mi_plaits.cpp", MI / "tides2/poly_slope_generator.cc", MI / "tides2/resources.cc", HERE / "brick_mi_tides.cpp"])
mi_newest = max(f.stat().st_mtime for f in MI.rglob("*") if f.suffix in (".h", ".cc"))   # a patched header too
mi_flags = [f for f in flags if not f.startswith("-I")] + [f"-I{MI}", f"-I{HERE}", "-Dstmlib=mi_stmlib", "-Dplaits=mi_plaits", "-Drings=mi_rings", "-Delements=mi_elements", "-Dtides=mi_tides", "-DTEST", os.environ.get("MD_MI_OPT", "-O2"), "-include", "stdio.h"]   # (plaits/user_data.h prints in TEST builds without including it)
for src in mi_src:
    if not src.exists():
        continue
    o = OUT / ("mi_" + str(src.relative_to(HERE)).replace("/", "_").replace(".", "_") + ".o")
    if not o.exists() or o.stat().st_mtime < max(src.stat().st_mtime, mi_newest) or src.name.startswith("brick_mi_"):
        subprocess.run([cxx, *mi_flags, "-std=c++11", "-fno-exceptions", "-fno-rtti", "-Wno-unused-parameter", "-Wno-unknown-warning-option", "-Wno-vla-cxx-extension", "-Wno-vla",
                        "-c", str(src), "-o", str(o)], check=True)
    objs.append(str(o))
core = OUT / "brick_core.o"
subprocess.run([cc, *flags, os.environ.get("MD_CORE_OPT", "-O2"), "-std=gnu99", "-Wno-unknown-attributes", "-c", str(HERE / "brick_core.c"), "-o", str(core)],
               check=True)
objs.append(str(core))
main = {"headless": "brick_headless.c", "headless-arm": "brick_headless.c", "studio": "brick_studio.c"}.get(target, "brick_sdl.c")
mo = OUT / (Path(main).stem + ".o")
subprocess.run([cc, *flags, *sdl_cflags, "-std=gnu99", "-c", str(HERE / main), "-o", str(mo)], check=True)
objs.append(str(mo))
exe = OUT / {"headless": "brick-headless", "headless-arm": "brick-headless", "studio": "brick-studio"}.get(target, "mutable-dreams")
subprocess.run([cxx, *flags, *objs, *link, *sdl_libs, "-lm", "-o", str(exe)], check=True)
print(exe)
