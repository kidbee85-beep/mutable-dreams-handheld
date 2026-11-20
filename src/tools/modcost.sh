#!/bin/zsh
# tools/modcost.sh: millions of instructions per second of sound, the plain dream and each Sound Lab module alone
# (its dearest settings), by the CPU's counter: a 20 s benchsound minus the same script without it
cd "${0:A:h}/.."
B=${1:-build/mac/brick-headless}
mods=(
 "dream only:"
 "TEXTURES:lab TEXTURES.FOREST 60|lab TEXTURES.STARDUST 60"
 "CLOUD:lab CLOUD.MIX 50"
 "PARTICLES spectral:lab PARTICLES.MIX 50|lab PARTICLES.MODE 3"
 "PARTICLES granular:lab PARTICLES.MIX 50"
 "SEEDS:lab SEEDS.MIX 50|lab SEEDS.MEDIA 0"
 "SHIMMER:lab SHIMMER.MIX 50|lab SHIMMER.SHIMMER 60"
 "CROSSMOD:lab CROSSMOD.MIX 40|lab CROSSMOD.ALGORITHM 90"
 "POLY 8 voices:lab POLY.LEVEL 50|lab POLY.VOICES 8|lab POLY.MODEL 2|lab POLY.FOLLOW_TRACK 1|lab POLY.FOLLOW_GATE 3000"
 "SYNTH:lab SYNTH.LEVEL 60"
 "COLOUR:lab COLOUR.ENSEMBLE 40|lab COLOUR.TAPE_DRIVE 20|lab COLOUR.WOW_+_FLUTTER 20"
 "MASTER chain:lab MASTER.SOUND 3"
)
for m in "${mods[@]}"; do
  n=${m%%:*}; cmds=${m#*:}
  { echo "wait 1500"; [ -n "$cmds" ] && echo "$cmds" | tr '|' '\n'; echo "tap START"; echo "wait 2000"; } > out/mc-0.txt
  cp out/mc-0.txt out/mc-1.txt; echo "benchsound 20000" >> out/mc-1.txt
  rm -rf out/mc-x; a=$( { /usr/bin/time -l $B out/mc-1.txt out/mc-x > /dev/null; } 2>&1 | grep "instructions retired" | awk '{print $1}')
  rm -rf out/mc-x; b=$( { /usr/bin/time -l $B out/mc-0.txt out/mc-x > /dev/null; } 2>&1 | grep "instructions retired" | awk '{print $1}')
  printf "%-20s %5d M/s\n" "$n" $(( (a-b)/20/1000000 ))
done
rm -rf out/mc-x out/mc-0.txt out/mc-1.txt
