#!/bin/zsh
# tools/asan.sh: every test under AddressSanitizer and UndefinedBehaviorSanitizer (build/mac-asan); prints the tests
# that did not pass and every report except the known harmless ones (shy_fft, dx_units: third_party/mi/PATCHES.md).
# Each test gets save dirs like tools/reg.sh (archive in a folder of its own: its fetched poems would otherwise land in
# out/samples and change what voices.txt finds). Do not run it at the same time as tools/reg.sh.
cd "${0:A:h}/.."
OUT_SUFFIX=-asan EXTRA_CFLAGS="-fsanitize=address,undefined -fsanitize-recover=undefined -fno-omit-frame-pointer -g" \
    python3 build.py headless > build/build-asan.log 2>&1 || { echo "build failed: build/build-asan.log"; exit 1; }
R=out/asan; mkdir -p $R; rm -f $R/*.log(N)
mkdir -p out/kitloop                              # (the kitloops test saves its frames there; out/ may have been cleared)
for f in tests/*.txt; do
  n=$(basename $f .txt)
  [ $n = dream-long ] && continue
  case $n in
    archive) d=out/asan-arc/save; mkdir -p out/asan-arc;;
    labbiome|labbiome2) d=out/asan-lb;;
    *) d=out/asan-s-$n;;
  esac
  MD_ARCHIVE_BASE=file://${PWD// /%20}/out/fake-archive MD_ARCHIVE_CATALOGUE=tests/archive-catalogue.tsv ASAN_OPTIONS=detect_leaks=0 \
    perl -e 'alarm 1200; exec @ARGV' ./build/mac-asan/brick-headless $f $d > $R/$n.log 2>&1
done
rm -rf out/asan-s-* out/asan-lb out/asan-arc
echo "not passed:"; grep -L "all passed" $R/*.log
echo "reports:"; grep -h "runtime error\|ERROR: AddressSanitizer" $R/*.log | sed 's/.*port\///' | grep -v "shy_fft\|dx_units" | sort | uniq -c
