#!/bin/bash
# Mutable Dreams for the TRIMUI SMART PRO (Knulli; the TG5040 family it shares with the Brick): goes into roms/ports beside the
# folder mutabledreams/. by Kid Bunka. GPL-3.0-only; sources: see mutabledreams/SOURCE.txt
# The program is the Brick's own (aarch64, the same A133P, Knulli's system SDL2). What differs is the screen and the keys:
#   - the Smart Pro's screen is 1280 x 720 (16:9), the Brick's 1024 x 768 (4:3): the stage is made 960 x 720 and drawn
#     1:1 in the middle, black beside it (the screen's size is read from the kernel: DRM, else the framebuffer);
#   - the two sticks turn the four dials (left: K1 and K2, right: K3 and K4; deflection is speed), their clicks (L3, R3)
#     are F1 (Sound Lab) and F2 (mixer); there is no FN switch: SELECT taps open the menu, L2 the quick bar (play mode in it);
#   - nobody has run it on a Smart Pro yet (1.5.10): the picture goes through SDL's renderer at the screen's own size
#     first; if no first frame comes within 20 s the program is stopped and started once more the Brick's way (a window
#     of the stage's size straight into the window surface). The way that worked is kept in mutabledreams/video-mode.txt.
# Switch files in mutabledreams/ (content does not matter):
#   keytest.on   every button pressed written to log.txt     selftest.on  plays 30 s by itself, measures, quits
#   nosplash.on  no animated start screen                    light.on     the light picture from the start
#   calm.on      CALM MOTION from the start                  nofast.on    the CPU governor left alone
#   fit.on / brick.on   only the screen-fitting way / only the Brick's way (no trying)
#   gpu.on       SDL's accelerated (GL) renderer             usbaudio.off the sound stays on the handheld
# H7_ROOT (tests only): a folder standing in for / when reading /sys and /proc.
HERE="${BASH_SOURCE[0]%/*}"
[ "$HERE" != "${BASH_SOURCE[0]}" ] || HERE=.
GAMEDIR="$HERE/mutabledreams"
cd -- "$GAMEDIR" || exit 2
GAMEDIR="$PWD"
LOG="$GAMEDIR/log.txt"
[ -f "$LOG" ] && mv -f -- "$LOG" "$GAMEDIR/log.previous.txt"
exec > "$LOG" 2>&1 || exit 2
echo "== Mutable Dreams (TRIMUI SMART PRO): shell entered"
sync                                             # (the log survives a forced power-off)
R="${H7_ROOT:-}"
mkdir -p -- "$GAMEDIR/save" "$GAMEDIR/samples/voices"
export LD_LIBRARY_PATH="$GAMEDIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [ -f "$GAMEDIR/gamecontrollerdb.txt" ]; then
    export SDL_GAMECONTROLLERCONFIG_FILE="$GAMEDIR/gamecontrollerdb.txt"
elif [ -z "${SDL_GAMECONTROLLERCONFIG_FILE+x}" ]; then
    for DB in /tmp/gamecontrollerdb.txt "${HOME:-/userdata/system}/.config/gamecontrollerdb.txt"; do
        [ -f "$DB" ] && { export SDL_GAMECONTROLLERCONFIG_FILE="$DB"; break; }
    done
fi

# --- the screen: its size, and the 4:3 stage that fits it ---
SW=""; SH=""
for M in "$R"/sys/class/drm/card*-*/modes; do      # (the connected panel's first mode, e.g. 640x480)
    [ -r "$M" ] || continue
    S="${M%/modes}/status"
    [ -r "$S" ] && [ "$(cat "$S" 2>/dev/null)" != connected ] && continue
    L=$(head -n 1 "$M" 2>/dev/null)
    case "$L" in *x*) SW="${L%%x*}"; SH="${L#*x}"; SH="${SH%%[!0-9]*}"; break;; esac
done
if [ -z "$SW" ] && [ -r "$R/sys/class/graphics/fb0/virtual_size" ]; then
    V=$(cat "$R/sys/class/graphics/fb0/virtual_size" 2>/dev/null)   # "640,960": width, height of both pages
    SW="${V%%,*}"; SH="${V#*,}"
    [ -n "$SW" ] && [ -n "$SH" ] && [ "$SH" -gt "$SW" ] 2>/dev/null && SH=$((SH / 2))   # (taller than wide: two pages of a landscape or square panel)
fi
case "$SW$SH" in *[!0-9]*|"") SW=1280; SH=720; echo "screen: size not found, taking 1280 x 720";; esac
OW=$SW; OH=$((SW * 3 / 4))                       # the largest 4:3 picture on it
[ "$OH" -gt "$SH" ] && { OH=$SH; OW=$((SH * 4 / 3)); }
OW=$((OW / 8 * 8)); OH=$((OH / 8 * 8))
echo "screen: ${SW} x ${SH}, the stage at ${OW} x ${OH}"
export MD_KEYS=handheld MD_DEVICE="TRIMUI SMART PRO"
FITOUT=""
[ "$OW" -ne 1024 ] || [ "$OH" -ne 768 ] && FITOUT="${OW}x${OH}"

ARGS=()
[ -f "$GAMEDIR/keytest.on" ] && ARGS+=(--keytest)
[ -f "$GAMEDIR/selftest.on" ] && export MD_RUN_SECONDS=30 MD_AUTOPLAY=1
[ -f "$GAMEDIR/nosplash.on" ] && export MD_NO_SPLASH=1
[ -f "$GAMEDIR/gpu.on" ] && export MD_VIDEO=gpu
[ -f "$GAMEDIR/light.on" ] && export MD_LIGHT=1
[ -f "$GAMEDIR/calm.on" ] && export MD_CALM=1
[ -f "$GAMEDIR/usbaudio.off" ] && export MD_USB_AUDIO=0

# --- what this device is (for a report; every command optional) ---
echo "== $(date) $(uname -a)"
for F in "$R"/proc/device-tree/model "$R"/sys/firmware/devicetree/base/model; do
    [ -r "$F" ] && { echo "device: $(tr -d '\0' < "$F")"; break; }
done
ldd --version 2>&1 | head -1
cat "$R"/proc/asound/cards 2>/dev/null
[ -r "$R/proc/bus/input/devices" ] && grep -E '^N: Name' "$R/proc/bus/input/devices" | sed 's/^/input: /'
echo "controller mapping: ${SDL_GAMECONTROLLERCONFIG_FILE:-none}"

# --- the cores at their normal top speed while it runs (Kid Bunka, 15.11.2026: no overclock, the old one back) ---
GOVS=()
restore_cpu() {
    for G in "${GOVS[@]}"; do echo "${G#*=}" > "${G%%=*}" 2>/dev/null; done
    [ ${#GOVS[@]} -gt 0 ] && echo "cpu: governor given back"
    GOVS=()
}
for C in "$R"/sys/devices/system/cpu/cpufreq/policy* "$R"/sys/devices/system/cpu/cpu0/cpufreq; do
    [ -f "$C/scaling_governor" ] || continue
    OLD=$(cat "$C/scaling_governor" 2>/dev/null)
    echo "cpu $C: governor $OLD, now $(cat "$C/scaling_cur_freq" 2>/dev/null) kHz, max $(cat "$C/scaling_max_freq" 2>/dev/null) kHz"
    if [ ! -f "$GAMEDIR/nofast.on" ] && [ "$OLD" != performance ] && grep -qw performance "$C/scaling_available_governors" 2>/dev/null; then
        if echo performance > "$C/scaling_governor" 2>/dev/null; then
            GOVS+=("$C/scaling_governor=$OLD")
            echo "cpu $C: performance for this session"
        fi
    fi
done

# --- the picture's way: tried in order, the one that worked first next time ---
MODEFILE="$GAMEDIR/video-mode.txt"
if [ -f "$GAMEDIR/fit.on" ]; then MODES=(fit); FORCED=1
elif [ -f "$GAMEDIR/brick.on" ]; then MODES=(brick); FORCED=1
else
    FORCED=0
    LAST=""
    [ -f "$MODEFILE" ] && [ ! -L "$MODEFILE" ] && IFS= read -r -n 16 LAST < "$MODEFILE"
    case "$LAST" in brick) MODES=(brick fit);; *) MODES=(fit brick);; esac
fi
WAIT="${H7_FIRST_FRAME_SECONDS:-20}"
CHILD=""
stop_child() {
    [ -n "$CHILD" ] || return
    kill -TERM "$CHILD" 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10; do kill -0 "$CHILD" 2>/dev/null || break; sleep 0.5; done
    kill -KILL "$CHILD" 2>/dev/null
    wait "$CHILD" 2>/dev/null
    CHILD=""
}
on_exit() { stop_child; restore_cpu; }
trap on_exit EXIT
trap 'stop_child; exit 143' TERM INT HUP
chmod +x -- "$GAMEDIR/mutable-dreams" 2>/dev/null
RC=124
for MODE in "${MODES[@]}"; do
    if [ "$MODE" = fit ]; then                   # the screen's own size, the stage in proportion in the middle
        export MD_SCREEN=1; unset MD_PRESENT
        [ -n "$FITOUT" ] && export MD_OUT="$FITOUT" || unset MD_OUT
    else                                         # the Brick's way: the stage's own size, straight into the window
        unset MD_SCREEN MD_OUT; export MD_PRESENT=window
    fi
    echo "video: trying the picture's way: $MODE"
    MARK=$(grep -c 'startup: first frame presented' "$LOG" 2>/dev/null)
    sync
    "$GAMEDIR/mutable-dreams" "${ARGS[@]}" "$GAMEDIR/save" &
    CHILD=$!
    N=0; SEEN=0
    while kill -0 "$CHILD" 2>/dev/null; do       # (a frame on the screen, or the program gone, or the time up)
        if [ "$(grep -c 'startup: first frame presented' "$LOG" 2>/dev/null)" -gt "$MARK" ]; then SEEN=1; break; fi
        [ "$N" -ge $((WAIT * 4)) ] && break
        sleep 0.25; N=$((N + 1))
    done
    if [ "$SEEN" = 0 ] && [ "$(grep -c 'startup: first frame presented' "$LOG" 2>/dev/null)" -gt "$MARK" ]; then SEEN=1; fi
    if [ "$SEEN" = 1 ]; then
        [ "$FORCED" = 0 ] && printf '%s\n' "$MODE" > "$MODEFILE"
        wait "$CHILD"; RC=$?; CHILD=""
        break
    fi
    if kill -0 "$CHILD" 2>/dev/null; then
        echo "video: no picture after ${WAIT} s through $MODE: stopped"
        stop_child; RC=124
    else
        wait "$CHILD"; RC=$?; CHILD=""
        echo "video: the program ended (status $RC) before a picture through $MODE"
    fi
done
echo "== exit $RC"
sync
exit "$RC"
