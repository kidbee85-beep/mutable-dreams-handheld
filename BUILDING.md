# Building Mutable Dreams for handhelds

The port is built from the **unchanged Mutable Dreams 2.1 sources** plus the port layer in `src/`. Nothing in the 2.1 tree is modified; generated headers and objects land in `src/build/`.

## 1. Get the engine sources

Download `Mutable-Dreams-2.1-Source.zip` from the [v2.1 release](https://github.com/kidbee85-beep/mutable-dreams/releases/tag/v2.1) of `kidbee85-beep/mutable-dreams` (checksums in `SHA256-2.1.txt` there), unpack it, and point the build at the folder that contains `app/` and `base/`:

```sh
export MD21_SOURCE=/path/to/Mutable-Dreams-2.1-Source
```

## 2. Build (from `src/`)

```sh
cd src
python3 build.py headless      # the test runner for this computer: build/mac/brick-headless (no window, no sound)
python3 build.py sdl           # the SDL2 program for this computer (needs SDL2)
python3 build.py brick         # Trimui Brick: aarch64 Linux (Debian 12 arm64 sysroot: tools/make_sysroot.py; clang + lld)
python3 build.py rgb20s        # RGB20S: aarch64, glibc 2.31, Cortex-A35 (Zig 0.14.1 as the cross compiler: ZIG=path)
python3 build.py miyoo         # Miyoo Mini / Mini Plus / Flip: 32-bit ARMv7, glibc 2.28, Cortex-A7 (Zig)
python3 build.py linux64       # Linux PC: x86_64, glibc 2.35+, the system's SDL2
```

The fonts and a few tables are generated with Pillow (`python3 -m venv .venv && .venv/bin/pip install Pillow`). `build.py`'s header lists every environment variable (`CROSS`, `SYSROOT`, `ZIG`, `SDL2_DIR`, …). The cross-compilation SDKs (`toolchain/`) are not part of this repository.

## 3. Test

```sh
tools/reg.sh .                       # all tests in tests/ with the headless runner: "passed N failed 0"
tools/asan.sh                        # the same under AddressSanitizer and UndefinedBehaviorSanitizer
MD_SMALL=1 zsh tools/textscan.sh     # texts that were shortened or run off the screen (640x480 mode)
bash tools/test_h700_launcher.sh     # the H700 launcher with a stand-in program: H700_LAUNCHER_PASS
python3 tools/flicker_scan.py FILM.raw   # flicker check on frames written by the runner's "film" command
```

Do not run two of the first three at the same time: they share output folders. The tests' pictures and sounds go to `out/`.

## 4. Package

```sh
python3 tools/make_packages.py OUTDIR            # RGB20S, Miyoo Mini Plus, Miyoo Mini, H700 (needs the builds above)
python3 tools/make_linux_os.py x86_64            # Linux PC package ("Dream Garden")
python3 tools/build_android.py                   # Android debug and unsigned release APKs
python3 tools/make_manual.py OUTDIR              # the manual (HTML and PDF, German and English; PDF needs Google Chrome)
python3 tools/manual_shots.py en OUTDIR/bilder-en   # the manual's pictures, taken by the headless build
```

The Brick package is `src/package/knulli/` plus `build/brick/mutable-dreams`.

### Android signing

`tools/build_android.py` signs the debug APK with a debug key kept in `~/Library/Application Support/MutableDreams/android-home/debug.keystore` (change `KEYHOME` in the script). It builds the release APK **unsigned**: sign it with your own key (`apksigner sign …`). No key or password belongs in this repository.

## 5. The website

```sh
python3 site-src/build.py      # writes docs/index.html and docs/de/index.html
```

The pictures in `docs/assets/img/` are frames from the program itself (`tools/manual_shots.py`, converted to WebP); the animated loops come from the `kitloops` test; the hero's living garden is the Visual Kit (`docs/assets/js/kit.js`).
