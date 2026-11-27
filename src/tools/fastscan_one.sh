#!/bin/zsh
# one test of tools/fastscan.sh
f=$1; n=${f:t:r}
case $n in archive) d=out/fit-arc/save;; *) d=out/fit-s-$n;; esac
MD_ARCHIVE_BASE=file://${PWD// /%20}/out/fake-archive MD_ARCHIVE_CATALOGUE=tests/archive-catalogue.tsv ./build/mac-fit/brick-headless $f $d > out/fit/$n.log 2>&1
