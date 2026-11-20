#!/bin/zsh
# tools/spancost.sh: which functions lay how many see-through (blended) and opaque pixels through bk_span, on four
# places (garden, FM-1 page, Lab, mixer), 10 s each. A measuring build without inlining (build/mac-span), so the
# caller one level up from bk_rrect / bk_rect is named. Totals are for the whole run (about 400 frames).
cd "${0:A:h}/.."
MD_CORE_OPT="-fno-inline" EXTRA_CFLAGS="-DMD_SPANSTAT -DMD_SPANSTAT_RA=__builtin_return_address(1)" OUT_SUFFIX=-span \
  python3 build.py headless > build/build-span.log 2>&1 || { echo "build failed: build/build-span.log"; exit 1; }
for t in "garden:" "page:tap X" "lab:tap F1" "mixer:tap F2"; do
  n=${t%%:*}; k=${t#*:}
  printf "wait 2500\ntap START\n$k\nwait 1000\nbenchdraw 10000\n" > out/sp-$n.txt
  rm -rf out/sp-x; ./build/mac-span/brick-headless out/sp-$n.txt out/sp-x 2> out/sp-$n.err > /dev/null
done
python3 tools/span_callers.py build/mac-span/brick-headless out/sp-garden.err out/sp-page.err out/sp-lab.err out/sp-mixer.err
rm -rf out/sp-x out/sp-*.txt out/sp-*.err
