#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutable Dreams for Android: the APKs, built offline with the tools TrackWerk uses on this Mac.

usage: build_android.py [debug|release]      (default: both)

Copies android/ to ~/Library/Caches/MutableDreams/android (Gradle writes thousands of files; not into iCloud), runs
prepare_android.py, then Gradle 8.14.3 --offline with JDK 21, the Android SDK (NDK 28.2) and a clone of TrackWerk's
Gradle cache. debug: signed with the development key, for trying it out. release: unsigned - Kid Bunka signs it
with his own key (apksigner); no key or password is ever part of this build. APKs land in build/android/."""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
TOOLS = Path.home() / "Library/Application Support/TrackWerk/Toolchains"
SDK = Path(os.environ.get("ANDROID_HOME", TOOLS / "android-sdk"))
JAVA = Path(os.environ.get("JAVA_HOME", TOOLS / "jdk-21.0.13+9/Contents/Home"))
GRADLE = Path.home() / ".gradle/wrapper/dists/gradle-8.14.3-bin/cv11ve7ro1n3o1j4so8xd9n66/gradle-8.14.3/bin/gradle"
WORK = Path.home() / "Library/Caches/MutableDreams/android"
TW_CACHE = Path.home() / "Library/Caches/TrackWerk/claude-android/gradle-home"
# (1.5.8) the debug key lives outside the caches: on 20.11.2026 a cleanup deleted the cache that held it, and with it the
# key of every debug APK up to 1.5.8 (certificate ad23ca94...). Kid Bunka chose the new one (76906e3a...), kept here.
# Never in Caches again; without it Gradle quietly makes another, and the next APK no longer installs as an update.
KEYHOME = Path.home() / "Library/Application Support/MutableDreams/android-home"
OUT = HERE / "build/android"


def main():
    kinds = sys.argv[1:] or ["debug", "release"]
    assert GRADLE.is_file() and (JAVA / "bin/java").is_file() and SDK.is_dir(), "tools missing"
    subprocess.run([sys.executable, HERE / "tools/prepare_android.py"], check=True, stdout=subprocess.DEVNULL)
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "project"
    if src.exists():
        shutil.rmtree(src)
    shutil.copytree(HERE / "android", src, ignore=shutil.ignore_patterns(".gradle", "build", ".cxx"))
    home = WORK / "gradle-home"
    online = os.environ.get("MD_GRADLE_ONLINE") == "1"   # (1.6.2) only when asked for: the dependencies fetched from Google's and Maven Central's repositories
    if not home.exists():                        # an APFS clone of TrackWerk's dependency cache; theirs is only read
        if TW_CACHE.exists():
            subprocess.run(["cp", "-c", "-R", str(TW_CACHE), str(home)], check=True)
        elif online:
            home.mkdir(parents=True)
        else:
            sys.exit(f"no dependency cache ({TW_CACHE} is gone): run again with MD_GRADLE_ONLINE=1 to fetch the dependencies once")
    if "debug" in kinds:
        assert (KEYHOME / "debug.keystore").is_file(), f"the debug key is missing: {KEYHOME}/debug.keystore (see above)"
    env = dict(os.environ, ANDROID_HOME=str(SDK), ANDROID_SDK_ROOT=str(SDK), JAVA_HOME=str(JAVA),
               GRADLE_USER_HOME=str(home), ANDROID_USER_HOME=str(KEYHOME), CMAKE_BUILD_PARALLEL_LEVEL="4")
    (src / "local.properties").write_text(f"sdk.dir={SDK}\n")
    tasks = [":app:assembleDebug" if k == "debug" else ":app:assembleRelease" for k in kinds]
    OUT.mkdir(parents=True, exist_ok=True)
    log = OUT / "build.log"
    with log.open("w") as stream:
        r = subprocess.run([str(GRADLE), *([] if online else ["--offline"]), "--no-daemon", "--console=plain", "--max-workers=4", "--project-dir",
                            str(src), f"-PmdPort={HERE}", *tasks], stdout=stream, stderr=subprocess.STDOUT, env=env)
    if r.returncode:
        print(f"Gradle failed, see {log}")
        sys.exit(1)
    version = re.search(r"(?:test build|release) ([0-9][^ \n]*)", (HERE / "package/knulli/mutabledreams/README.txt").read_text())[1]
    for k in kinds:
        name = "app-debug.apk" if k == "debug" else "app-release-unsigned.apk"
        apk = src / "app/build/outputs/apk" / k / name
        dst = OUT / f"Mutable-Dreams-Android-{version}-{'debug' if k == 'debug' else 'release-unsigned'}.apk"
        shutil.copy2(apk, dst)
        print(dst)


if __name__ == "__main__":
    main()
