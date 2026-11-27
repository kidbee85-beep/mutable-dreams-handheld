#!/bin/zsh
# tools/fastscan.sh: tools/textscan.sh on all processor cores at once (each test with a save folder of its own)
cd "${0:A:h}/.."
OUT_SUFFIX=-fit EXTRA_CFLAGS="-DBK_LOG_FIT" python3 build.py headless > build/build-fit.log 2>&1 || { echo "build failed"; exit 1; }
R=out/fit; rm -rf $R; mkdir -p $R out/kitloop out/fit-arc
ls tests/*.txt | grep -Ev '/(archive-catalogue|dream-long|bench)[^/]*$' | xargs -n 1 -P $(sysctl -n hw.ncpu) zsh tools/fastscan_one.sh
rm -rf out/fit-s-* out/fit-arc
echo "off the screen:"; cat $R/*.log | grep "^OFF" | sed 's/[0-9]* %$/N %/' | sort -u
echo "shortened: $(cat $R/*.log | grep '^FIT' | sort -u | wc -l)"; cat $R/*.log | grep "^FIT" | sort -u > out/fit-lines.txt
