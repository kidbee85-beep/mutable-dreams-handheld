#!/bin/zsh
# tools/textlist.sh: every text the stage draws in all tests (digits as #), once each, sorted -> out/texts.txt.
# (0.80: the list the German table lang/de.txt is made from; MD_HANDHELD=1 and MD_KEYBOARD=1 runs add their hints.)
# Do not run it at the same time as reg.sh, asan.sh or textscan.sh.
cd "${0:A:h}/.."
OUT_SUFFIX=-txt EXTRA_CFLAGS="-DBK_LOG_TEXT" python3 build.py headless > build/build-txt.log 2>&1 || { echo "build failed"; exit 1; }
R=out/txt; rm -rf $R; mkdir -p $R
for f in tests/*.txt; do
  n=$(basename $f .txt)
  case $n in archive-catalogue|dream-long|bench*) continue;; esac
  for mode in plain kb; do
    case $n in archive) d=out/txt-arc/save; mkdir -p out/txt-arc;; *) d=out/txt-s-$n;; esac
    if [ $mode = kb ]; then export MD_KEYBOARD=1; else unset MD_KEYBOARD; fi
    MD_ARCHIVE_BASE=file://${PWD// /%20}/out/fake-archive MD_ARCHIVE_CATALOGUE=tests/archive-catalogue.tsv \
      ./build/mac-txt/brick-headless $f $d > $R/$n-$mode.log 2>&1
    rm -rf out/txt-s-$n out/txt-arc
  done
done
cat $R/*.log | grep "^TEXT " | sed 's/^TEXT //' | sort -u > out/texts.txt
wc -l out/texts.txt
