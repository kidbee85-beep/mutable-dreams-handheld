#!/bin/bash
# Mutable Dreams for Trimui Brick (Knulli): goes into roms/ports beside the folder mutabledreams/.
# by Kid Bunka. GPL-3.0-only; sources: see mutabledreams/SOURCE.txt
#   mutabledreams/keytest.on   present: every button is written to log.txt (for brick-buttons.txt)
#   mutabledreams/selftest.on  present: plays 30 s by itself, quits and writes its measurements to log.txt
#   mutabledreams/nosplash.on  present: starts without the animated start screen
#   mutabledreams/gpu.on       present: tries the accelerated (GL) renderer instead of the software one
#   mutabledreams/nofast.on    present: the CPU governor is left alone (else "performance" while it runs)
#   mutabledreams/profile.on   present: every 30 s log.txt says where the stage's time goes (tools/symbolize_profile.py)
#   mutabledreams/nosurface.on present: the picture through SDL's renderer again (since 0.79 it goes straight into
#                              the window surface, measured faster on the Brick; surface.on is no longer needed)
#   mutabledreams/light.on     present: the light picture from the start (else it comes by itself when slow)
#   mutabledreams/calm.on      present: CALM MOTION from the start (shapes still, nothing drifting)
#   mutabledreams/usbaudio.off present: the sound stays on the Brick even with a USB-C sound card plugged in (since
#                              0.80 a class-compliant USB card or DAC takes the sound, and gives it back when pulled out)
# Standalone: no PortMaster. Knulli's own controller mapping is kept (as TrackWerk does on the same card).
HERE="${BASH_SOURCE[0]%/*}"
[ "$HERE" != "${BASH_SOURCE[0]}" ] || HERE=.
GAMEDIR="$HERE/mutabledreams"
cd -- "$GAMEDIR" || exit 2
GAMEDIR="$PWD"
LOG="$GAMEDIR/log.txt"
[ -f "$LOG" ] && mv -f -- "$LOG" "$GAMEDIR/log.previous.txt"
exec > "$LOG" 2>&1 || exit 2
echo "== Mutable Dreams: shell entered"
sync                                             # (the log survives a forced power-off)
mkdir -p -- "$GAMEDIR/save" "$GAMEDIR/samples/voices"
export LD_LIBRARY_PATH="$GAMEDIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [ -f "$GAMEDIR/gamecontrollerdb.txt" ]; then
    export SDL_GAMECONTROLLERCONFIG_FILE="$GAMEDIR/gamecontrollerdb.txt"
elif [ -z "${SDL_GAMECONTROLLERCONFIG_FILE+x}" ]; then
    for DB in /tmp/gamecontrollerdb.txt "${HOME:-/userdata/system}/.config/gamecontrollerdb.txt"; do
        [ -f "$DB" ] && { export SDL_GAMECONTROLLERCONFIG_FILE="$DB"; break; }
    done
fi
ARGS=()
[ -f "$GAMEDIR/keytest.on" ] && ARGS+=(--keytest)
[ -f "$GAMEDIR/selftest.on" ] && export MD_RUN_SECONDS=30 MD_AUTOPLAY=1
[ -f "$GAMEDIR/nosplash.on" ] && export MD_NO_SPLASH=1
[ -f "$GAMEDIR/gpu.on" ] && export MD_VIDEO=gpu
[ -f "$GAMEDIR/profile.on" ] && export MD_PROFILE_DRAW=1
[ -f "$GAMEDIR/nosurface.on" ] || export MD_PRESENT=window   # (0.79: the standard - measured on the Brick, 11.8 ms instead of 16.5 to show a frame)
[ -f "$GAMEDIR/light.on" ] && export MD_LIGHT=1
[ -f "$GAMEDIR/calm.on" ] && export MD_CALM=1
[ -f "$GAMEDIR/usbaudio.off" ] && export MD_USB_AUDIO=0
echo "== $(date) $(uname -a)"
ldd --version 2>&1 | head -1
cat /proc/asound/cards 2>/dev/null
# The cores at their normal top speed while Mutable Dreams runs (Kid Bunka's decision, 15.11.2026): each cpufreq
# policy's governor set to "performance" if the kernel offers it - no overclock, the maximum frequency stays what
# Knulli allows - and the old governor put back on any exit. mutabledreams/nofast.on present: left as it is.
GOVS=()
restore_cpu() {
    for G in "${GOVS[@]}"; do echo "${G#*=}" > "${G%%=*}" 2>/dev/null; done
    [ ${#GOVS[@]} -gt 0 ] && echo "cpu: governor given back"
    GOVS=()
}
trap restore_cpu EXIT
for C in /sys/devices/system/cpu/cpufreq/policy* /sys/devices/system/cpu/cpu0/cpufreq; do
    [ -f "$C/scaling_governor" ] || continue
    OLD=$(cat "$C/scaling_governor" 2>/dev/null)
    echo "cpu $C: governor $OLD, now $(cat $C/scaling_cur_freq 2>/dev/null) kHz, max $(cat $C/scaling_max_freq 2>/dev/null) kHz"
    if [ ! -f "$GAMEDIR/nofast.on" ] && [ "$OLD" != performance ] && grep -qw performance "$C/scaling_available_governors" 2>/dev/null; then
        if echo performance > "$C/scaling_governor" 2>/dev/null; then
            GOVS+=("$C/scaling_governor=$OLD")
            echo "cpu $C: performance for this session, now $(cat $C/scaling_cur_freq 2>/dev/null) kHz"
        fi
    fi
done
echo "cpus online: $(cat /sys/devices/system/cpu/online 2>/dev/null)"
echo "controller mapping: ${SDL_GAMECONTROLLERCONFIG_FILE:-none}"
chmod +x -- "$GAMEDIR/mutable-dreams" 2>/dev/null
sync
"$GAMEDIR/mutable-dreams" "${ARGS[@]}" "$GAMEDIR/save"
echo "== exit $?"
sync
