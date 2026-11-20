#!/bin/zsh
# tools/textscan.sh: every test with a build that reports texts it had to shorten (FIT) and texts that run off the
# screen (OFF), each once (build/mac-fit, -DBK_LOG_FIT). MD_SMALL=1 zsh tools/textscan.sh: as on a 640 x 480 handheld. Save dirs of its own (the archive's in a folder apart:
# fetched poems would otherwise land in out/samples). Do not run it at the same time as reg.sh or asan.sh.
cd "${0:A:h}/.."
OUT_SUFFIX=-fit EXTRA_CFLAGS="-DBK_LOG_FIT" python3 build.py headless > build/build-fit.log 2>&1 || { echo "build failed"; exit 1; }
R=out/fit; rm -rf $R; mkdir -p $R
mkdir -p out/kitloop                              # (the kitloops test saves its frames there; out/ may have been cleared)
for f in tests/*.txt; do
  n=$(basename $f .txt)
  case $n in archive-catalogue|dream-long|bench*) continue;; esac
  case $n in archive) d=out/fit-arc/save; mkdir -p out/fit-arc;; *) d=out/fit-s-$n;; esac
  MD_ARCHIVE_BASE=file://${PWD// /%20}/out/fake-archive MD_ARCHIVE_CATALOGUE=tests/archive-catalogue.tsv \
    ./build/mac-fit/brick-headless $f $d > $R/$n.log 2>&1
done
rm -rf out/fit-s-* out/fit-arc
echo "off the screen:"; cat $R/*.log | grep "^OFF" | sed 's/[0-9]* %$/N %/' | sort -u
echo "shortened:"; cat $R/*.log | grep "^FIT" | sort -u
