#!/bin/zsh
# tools/drawcost.sh [BINARY]: instructions per stage frame on four places (garden, FM-1 page, Lab, mixer), by the
# CPU's own counter (load-independent): a 20 s run with drawing minus the same without, over 606 frames
cd "${0:A:h}/.."
B=${1:-build/mac/brick-headless}
for t in "garden:" "page:tap X" "lab:tap F1" "mixer:tap F2"; do
  n=${t%%:*}; k=${t#*:}
  printf "wait 2500\ntap START\n$k\nwait 1000\nbenchdraw 20000\n" > out/dc-d.txt
  printf "wait 2500\ntap START\n$k\nwait 1000\nbenchsound 20000\n" > out/dc-s.txt
  a=$( { /usr/bin/time -l $B out/dc-d.txt out/dc-x > /dev/null; } 2>&1 | grep "instructions retired" | awk '{print $1}')
  b=$( { /usr/bin/time -l $B out/dc-s.txt out/dc-x > /dev/null; } 2>&1 | grep "instructions retired" | awk '{print $1}')
  echo "$n $(( (a-b)/606/1000 )) k"
done
rm -rf out/dc-x out/dc-d.txt out/dc-s.txt
