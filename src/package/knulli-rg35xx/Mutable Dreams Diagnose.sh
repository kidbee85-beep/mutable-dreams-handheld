#!/bin/bash
# Mutable Dreams on the ANBERNIC RG35XX (Knulli): the diagnosis. Starts nothing; writes mutabledreams/diagnose.txt, what this
# handheld is and whether the program has what it needs. Run it from Ports once, then send diagnose.txt (and log.txt after a
# try of Mutable Dreams) back. by Kid Bunka. GPL-3.0-only. H7_ROOT (tests only): a folder standing in for / .
HERE="${BASH_SOURCE[0]%/*}"
[ "$HERE" != "${BASH_SOURCE[0]}" ] || HERE=.
GAMEDIR="$HERE/mutabledreams"
cd -- "$GAMEDIR" || exit 2
GAMEDIR="$PWD"
OUT="$GAMEDIR/diagnose.txt"
R="${H7_ROOT:-}"
exec > "$OUT" 2>&1 || exit 2
echo "== Mutable Dreams: diagnosis ($(date))"
echo "== system"
uname -a
for F in "$R"/proc/device-tree/model "$R"/sys/firmware/devicetree/base/model; do
    [ -r "$F" ] && { echo "device: $(tr -d '\0' < "$F")"; break; }
done
[ -f "$R/etc/os-release" ] && cat "$R/etc/os-release"
ldd --version 2>&1 | head -1
echo "== memory and card"
grep -E 'MemTotal|MemAvailable' "$R"/proc/meminfo 2>/dev/null
df -h "$GAMEDIR" 2>/dev/null
echo "== the program"
ls -l "$GAMEDIR/mutable-dreams" 2>&1
file "$GAMEDIR/mutable-dreams" 2>&1 | head -1
if command -v ldd >/dev/null 2>&1; then
    ldd "$GAMEDIR/mutable-dreams" 2>&1
    ldd "$GAMEDIR/mutable-dreams" 2>&1 | grep -qE 'not found|cannot execute|not a dynamic' && echo "PROBLEM: the program lacks something above"
else
    echo "ldd not available"
fi
echo "== SDL2"
for L in /usr/lib/libSDL2-2.0.so.0 /usr/lib64/libSDL2-2.0.so.0 /lib/libSDL2-2.0.so.0 /usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0; do
    [ -e "$R$L" ] && echo "$L -> $(readlink -f "$R$L" 2>/dev/null)"
done
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-unset} SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-unset}"
echo "== screen"
for M in "$R"/sys/class/drm/card*-*/modes; do
    [ -r "$M" ] || continue
    echo "$M: $(cat "${M%/modes}/status" 2>/dev/null): $(head -n 2 "$M" 2>/dev/null | tr '\n' ' ')"
done
[ -r "$R/sys/class/graphics/fb0/virtual_size" ] && echo "fb0: $(cat "$R/sys/class/graphics/fb0/virtual_size")"
echo "== sound cards"
cat "$R"/proc/asound/cards 2>/dev/null
echo "== input devices"
[ -r "$R/proc/bus/input/devices" ] && grep -E '^N: Name|^H: Handlers' "$R/proc/bus/input/devices"
echo "controller database: ${SDL_GAMECONTROLLERCONFIG_FILE:-unset}"
for DB in /tmp/gamecontrollerdb.txt "${HOME:-/userdata/system}/.config/gamecontrollerdb.txt" "$GAMEDIR/gamecontrollerdb.txt"; do
    [ -f "$R$DB" ] && echo "found: $DB ($(wc -l < "$R$DB") lines)"
done
echo "== cpu"
for C in "$R"/sys/devices/system/cpu/cpufreq/policy*; do
    [ -f "$C/scaling_governor" ] && echo "$C: $(cat "$C/scaling_governor") $(cat "$C/scaling_cur_freq" 2>/dev/null) kHz (max $(cat "$C/scaling_max_freq" 2>/dev/null))"
done
echo "== the folder"
ls -l "$GAMEDIR" | head -30
echo "== end"
sync
exit 0
