#!/bin/zsh
# tools/prof_at.sh TEST [MS]: where the stage holds the core longer than MS (Mac time) without letting the sound in.
# Needs the profiling build: OUT_SUFFIX=-prof EXTRA_CFLAGS=-g python3 build.py headless
cd "${0:A:h}/.."
B=build/mac-prof/brick-headless
MD_PROFILE=${2:-1.0} MD_PROFILE_AT=1 $B tests/$1.txt out/prof-at-$$ > out/prof-at-$$.log 2>&1
base=$(grep PROFILE-BASE out/prof-at-$$.log | awk '{print $2}')
m=$(nm $B | grep " _main$" | awk '{print $1}')
L=$(python3 -c "print(hex($base-0x$m+0x100000000))")
grep PROFILE-AT out/prof-at-$$.log | awk '{print $5, $7}' | sort | uniq -c | sort -rn | head -${3:-8} | while read n a b; do
  fa=$(atos -o $B -l $L $a 2>/dev/null); fb=$([ "$b" = end ] && echo end || atos -o $B -l $L $b 2>/dev/null)
  mx=$(grep "from $a to $b" out/prof-at-$$.log | awk '{print $2}' | sort -n | tail -1)
  echo "$n x  max $mx ms  FROM $fa  TO $fb"
done
grep "PROFILE max" out/prof-at-$$.log
rm -rf out/prof-at-$$ out/prof-at-$$.log
