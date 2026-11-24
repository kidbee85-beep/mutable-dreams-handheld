#!/bin/zsh
# tools/reg.sh PORT_DIR: every test in tests/ with build/mac/brick-headless; "passed N failed M" at the end
cd "$1"
R=out/reg42-$(date +%s); mkdir -p $R
mkdir -p out/kitloop                              # (the kitloops test saves its frames there; out/ may have been cleared)
pass=0; fail=0
for f in tests/*.txt; do
  n=$(basename $f .txt)
  [ $n = archive-catalogue ] && continue
  case $n in
    archive) mkdir -p out/arcreg$$; MD_ARCHIVE_BASE=file://${PWD// /%20}/out/fake-archive MD_ARCHIVE_CATALOGUE=tests/archive-catalogue.tsv ./build/mac/brick-headless $f out/arcreg$$/save > $R/$n.log 2>&1;;
    touchviews) MD_ARCHIVE_BASE=file://${PWD// /%20}/out/fake-archive MD_ARCHIVE_CATALOGUE=tests/archive-catalogue.tsv ./build/mac/brick-headless $f out/reg42s-$n-$$ > $R/$n.log 2>&1;;
    labbiome|labbiome2) ./build/mac/brick-headless $f out/reg42-lb$$ > $R/$n.log 2>&1;;
    *) ./build/mac/brick-headless $f out/reg42s-$n-$$ > $R/$n.log 2>&1;;
  esac
  if tail -1 $R/$n.log | grep -q "all passed"; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL $n: $(grep -m3 FAIL $R/$n.log | tr '\n' ' ')"; fi
done
echo "passed $pass failed $fail (logs $R)"
find out -maxdepth 1 \( -name 'reg42s-*' -o -name 'reg42-lb*' -o -name 'arcreg*' \) -exec rm -rf {} +
