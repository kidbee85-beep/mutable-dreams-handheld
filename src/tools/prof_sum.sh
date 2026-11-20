#!/bin/zsh
# tools/prof_sum.sh SCRIPT: where a frame's time goes, summed by the function whose yield ended each stretch
# (needs: OUT_SUFFIX=-prof EXTRA_CFLAGS=-g python3 build.py headless)
cd "${0:A:h}/.."
B=build/mac-prof/brick-headless
MD_PROFILE=1000 MD_PROFILE_SUM=1 MD_PROFILE_AT=1 $B $1 out/prof-sum-$$ > out/prof-sum-$$.log 2>&1
base=$(grep PROFILE-BASE out/prof-sum-$$.log | awk '{print $2}')
m=$(nm $B | grep " _main$" | awk '{print $1}')
L=$(python3 -c "print(hex($base-0x$m+0x100000000))")
tot=$(grep PROFILE-SUM out/prof-sum-$$.log | awk '{s+=$3} END {print s}')
grep PROFILE-SUM out/prof-sum-$$.log | sort -k3 -rn | head -${2:-25} | while read x a ms; do
  f=$([ "$a" = 0x1 ] && echo "(end of frame)" || atos -o $B -l $L $a 2>/dev/null | sed 's/ (in brick-headless)//')
  printf "%5.1f %%  %s\n" $(( ms * 100.0 / tot )) "$f"
done
rm -rf out/prof-sum-$$ out/prof-sum-$$.log
