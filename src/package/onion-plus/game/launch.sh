#!/bin/sh
# Mutable Dreams for the MIYOO MINI PLUS (OnionOS). by Kid Bunka. GPL-3.0-only; sources: see SOURCE.txt
# Started by Onion's Ports (Roms/PORTS/Shortcuts/MutableDreams.port) or Apps > Mutable Dreams.
#   keytest.on   every button pressed is written to log.txt     selftest.on  plays 30 s by itself, measures, quits
#   nosplash.on  no animated start screen
#   calm.on      CALM MOTION from the start (shapes held still: also lighter for the Cortex-A7)
#   full.on      the full picture instead of the light one (decoration the A7 may not keep up with)
#   nofast.on    the CPU governor left alone (else "performance" while it runs, the old one put back after)
#   scaleone.on  the picture made smaller on one core only      burst.off  no sound made ahead between frames
#   sidefull.on  the columns beside the stage drawn every frame (1.5.8: every other frame while nothing is pressed)
#   fps15.on     15 frames a second (it asks for 20)
# Only our exact Miyoo SDL (mmiyoo, steward-fu) and its NEON helper are bundled; the firmware's libraries stay.
GAMEDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "$GAMEDIR" || exit 1
LOG="$GAMEDIR/log.txt"
[ -f "$LOG" ] && mv -f "$LOG" "$GAMEDIR/log.previous.txt"
exec > "$LOG" 2>&1
echo "== Mutable Dreams (MIYOO MINI PLUS): shell entered $(date)"
sync
mkdir -p "$GAMEDIR/save" "$GAMEDIR/samples/voices"
# Plus: the archive fetches with curl over WiFi; Onion brings curl and its TLS libraries
export PATH="/mnt/SDCARD/.tmp_update/bin:$PATH"
NETLIB=/mnt/SDCARD/.tmp_update/lib:
export LD_LIBRARY_PATH="$GAMEDIR/libs:${NETLIB}/mnt/SDCARD/miyoo/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}:/config/lib:/customer/lib:/lib"
unset LD_PRELOAD
export SDL_VIDEODRIVER=mmiyoo SDL_AUDIODRIVER=mmiyoo SDL_MMIYOO_INPUT_MODE=joystick
export SDL_MMIYOO_TEXTURE_POOL_MAX_BYTES=2097152
# the Brick's stage made 640 x 480 here (a 1.2 MB texture), a calmer picture for the Cortex-A7, the quick bar for F1 F2 FN
export MD_OUT=640x480 MD_FPS=15 MD_LIGHT=1 MD_KEYS=handheld MD_DEVICE="MIYOO MINI PLUS"
[ -f "$GAMEDIR/keytest.on" ] && set -- --keytest
[ -f "$GAMEDIR/selftest.on" ] && export MD_RUN_SECONDS=30 MD_AUTOPLAY=1
[ -f "$GAMEDIR/nosplash.on" ] && export MD_NO_SPLASH=1
[ -f "$GAMEDIR/calm.on" ] && export MD_CALM=1
[ -f "$GAMEDIR/full.on" ] && export MD_LIGHT=0
[ -f "$GAMEDIR/scaleone.on" ] && export MD_SCALE_ONE=1
[ -f "$GAMEDIR/burst.off" ] && export MD_AUDIO_BURST=0
export MD_SIDE_HALF=1                            # (1.5.8) the columns beside the stage every other frame
export MD_FPS=20                                 # (1.5.8) 42 ms a frame with 1.5.7 under Onion: 20 asked (fps15.on: 15)
[ -f "$GAMEDIR/fps15.on" ] && export MD_FPS=15
[ -f "$GAMEDIR/sidefull.on" ] && export MD_SIDE_HALF=0
# (1.5.7) the two cores at their normal top speed while Mutable Dreams runs (Kid Bunka's decision, 15.11.2026, as on
# the Brick): the governor "performance" if the kernel offers it - no overclock, the top frequency stays the system's -
# and the old governor put back when it ends. nofast.on: left as it is.
CPUF=/sys/devices/system/cpu/cpu0/cpufreq
OLDGOV=
if [ -f "$CPUF/scaling_governor" ]; then
    G=$(cat "$CPUF/scaling_governor" 2>/dev/null)
    echo "cpu: governor $G, now $(cat "$CPUF/scaling_cur_freq" 2>/dev/null) kHz, max $(cat "$CPUF/scaling_max_freq" 2>/dev/null) kHz"
    if [ ! -f "$GAMEDIR/nofast.on" ] && [ "$G" != performance ] && grep -qw performance "$CPUF/scaling_available_governors" 2>/dev/null \
        && echo performance > "$CPUF/scaling_governor" 2>/dev/null; then
        OLDGOV=$G
        echo "cpu: performance for this session, now $(cat "$CPUF/scaling_cur_freq" 2>/dev/null) kHz"
    fi
fi
: "${miyoodir:=/mnt/SDCARD/miyoo}"
export miyoodir
echo "library path: $LD_LIBRARY_PATH"
STOP=/mnt/SDCARD/.tmp_update/script/stop_audioserver.sh   # (Onion's audio server hands the sound over)
if [ -f "$STOP" ]; then sh "$STOP"; sleep 0.3; else echo "no Onion audio handover script: $STOP"; fi
chmod +x "$GAMEDIR/mutable-dreams" 2>/dev/null
sync
"$GAMEDIR/mutable-dreams" "$@" "$GAMEDIR/save" &
CHILD=$!
(   # start watchdog: only this launcher's child; the program writes "startup: first frame presented"
    N=0
    while [ "$N" -lt 25 ]; do
        kill -0 "$CHILD" 2>/dev/null || exit 0
        grep -q 'startup: first frame presented' "$LOG" && exit 0
        sleep 1; N=$((N+1))
    done
    kill -0 "$CHILD" 2>/dev/null || exit 0
    grep -q 'startup: first frame presented' "$LOG" && exit 0
    echo "start watchdog: no picture after 25 s, back to Onion"
    kill -TERM "$CHILD" 2>/dev/null; sleep 1; kill -KILL "$CHILD" 2>/dev/null
) &
WATCH=$!
trap 'kill -TERM "$CHILD" 2>/dev/null' TERM INT HUP
wait "$CHILD"
RC=$?
kill "$WATCH" 2>/dev/null
if [ -n "$OLDGOV" ]; then echo "$OLDGOV" > "$CPUF/scaling_governor" 2>/dev/null && echo "cpu: governor given back ($OLDGOV)"; fi
echo "== exit $RC"
sync
PANEL=/mnt/SDCARD/.tmp_update/bin/infoPanel
if [ "$RC" -ne 0 ] && [ -x "$PANEL" ]; then
    LD_LIBRARY_PATH=/mnt/SDCARD/miyoo/lib:/config/lib:/lib "$PANEL" --title 'Mutable Dreams' --message 'Could not start. Details: Roms/PORTS/Games/MutableDreams/log.txt' >/dev/null 2>&1
fi
exit "$RC"
