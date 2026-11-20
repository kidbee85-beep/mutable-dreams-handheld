#!/bin/bash
# Mutable Dreams for the Powkiddy RGB20S (AmberELEC; also ROCKNIX and ArkOS on RK3326 handhelds): goes into
# roms/ports beside the folder mutabledreams/. by Kid Bunka. GPL-3.0-only; sources: see mutabledreams/SOURCE.txt
#   mutabledreams/keytest.on   present: every button is written to log.txt
#   mutabledreams/selftest.on  present: plays 30 s by itself, quits and writes its measurements to log.txt
#   mutabledreams/nosplash.on  present: starts without the animated start screen
#   mutabledreams/calm.on      present: CALM MOTION from the start (shapes still, nothing drifting)
#   mutabledreams/light.on     present: the light picture from the start (else it comes by itself when slow)
#   mutabledreams/gpu.on       present: the accelerated (GL) renderer instead of the software one
#   mutabledreams/surface.on   present: the picture straight into the window's pixels (as on the Brick since 0.79) -
#                              try this if the picture still flickers with 1.5.5
#   mutabledreams/thread.on    present: the stage drawn on a thread of its own (1.5.1: off here - the picture flickered
#                              on the RGB20S with it; 1.0 drew on the main thread and was steady)
#   mutabledreams/cache.on     present: the side columns' and views' grounds kept between frames (1.3); off here since
#                              1.5.5 - the picture flickered beside the menus with it
# The RGB20S has no MENU, F1, F2 or FN key: L2 (tapped) opens the quick bar with all of it.
HERE="${BASH_SOURCE[0]%/*}"
[ "$HERE" != "${BASH_SOURCE[0]}" ] || HERE=.
GAMEDIR="$HERE/mutabledreams"
cd -- "$GAMEDIR" || exit 2
GAMEDIR="$PWD"
LOG="$GAMEDIR/log.txt"
[ -f "$LOG" ] && mv -f -- "$LOG" "$GAMEDIR/log.previous.txt"
exec > "$LOG" 2>&1 || exit 2
echo "== Mutable Dreams (RGB20S): shell entered"
sync
mkdir -p -- "$GAMEDIR/save" "$GAMEDIR/samples/voices"
export LD_LIBRARY_PATH="$GAMEDIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# AmberELEC's SDL draws through DRM and plays through ALSA (as TrackWerk on the same device); a value already set wins
[ -z "${SDL_VIDEODRIVER+x}" ] && export SDL_VIDEODRIVER=kmsdrm
[ -z "${SDL_AUDIODRIVER+x}" ] && export SDL_AUDIODRIVER=alsa
export MD_KEYS=handheld MD_DEVICE="POWKIDDY RGB20S" MD_OUT=640x480   # (the stage made 640 x 480 here: smoother letters)
if [ -z "${SDL_GAMECONTROLLERCONFIG_FILE+x}" ]; then
    for DB in /storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt /usr/config/SDL-GameControllerDB/gamecontrollerdb.txt \
              /tmp/gamecontrollerdb.txt; do
        [ -f "$DB" ] && { export SDL_GAMECONTROLLERCONFIG_FILE="$DB"; break; }
    done
fi
# the RG351MP family's internal pad (the RGB20S reports it): its buttons by name, as TrackWerk maps them
if [ -z "${SDL_GAMECONTROLLERCONFIG+x}" ] && [ -r "$GAMEDIR/controllers.txt" ]; then
    SDL_GAMECONTROLLERCONFIG="$(< "$GAMEDIR/controllers.txt")"
    export SDL_GAMECONTROLLERCONFIG
fi
ARGS=()
[ -f "$GAMEDIR/keytest.on" ] && ARGS+=(--keytest)
[ -f "$GAMEDIR/selftest.on" ] && export MD_RUN_SECONDS=30 MD_AUTOPLAY=1
[ -f "$GAMEDIR/nosplash.on" ] && export MD_NO_SPLASH=1
[ -f "$GAMEDIR/calm.on" ] && export MD_CALM=1
[ -f "$GAMEDIR/light.on" ] && export MD_LIGHT=1
[ -f "$GAMEDIR/gpu.on" ] && export MD_VIDEO=gpu
[ -f "$GAMEDIR/surface.on" ] && export MD_PRESENT=window
[ -f "$GAMEDIR/thread.on" ] || export MD_DRAW_THREAD=0
[ -f "$GAMEDIR/cache.on" ] || export MD_GROUND_CACHE=0   # (1.5.5: every ground drawn each frame, as in 1.0)
echo "== $(date) $(uname -a)"
ldd --version 2>&1 | head -1
cat /proc/asound/cards 2>/dev/null
echo "video ${SDL_VIDEODRIVER} audio ${SDL_AUDIODRIVER} controller mapping: ${SDL_GAMECONTROLLERCONFIG_FILE:-none}"
chmod +x -- "$GAMEDIR/mutable-dreams" 2>/dev/null
sync
"$GAMEDIR/mutable-dreams" "${ARGS[@]}" "$GAMEDIR/save" &
CHILD=$!
(   # start watchdog: only this launcher's child; the program writes "startup: first frame presented"
    for N in $(seq 1 25); do
        kill -0 "$CHILD" 2>/dev/null || exit 0
        grep -q 'startup: first frame presented' "$LOG" && exit 0
        sleep 1
    done
    kill -0 "$CHILD" 2>/dev/null || exit 0
    grep -q 'startup: first frame presented' "$LOG" && exit 0
    echo "start watchdog: no picture after 25 s, back to the menu"
    kill -TERM "$CHILD" 2>/dev/null; sleep 1; kill -KILL "$CHILD" 2>/dev/null
) &
WATCH=$!
trap 'kill -TERM "$CHILD" 2>/dev/null' TERM INT HUP
wait "$CHILD"
RC=$?
kill "$WATCH" 2>/dev/null
echo "== exit $RC"
sync
