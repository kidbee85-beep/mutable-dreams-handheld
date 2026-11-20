#!/bin/bash
# GPL-3.0-only. tools/test_smartpro_launcher.sh: the Trimui Smart Pro launcher (package/knulli-smartpro/Mutable Dreams.sh) with a
# stand-in for the program and a made-up /sys and /proc (H7_ROOT): the screen's size and the stage made to fit it,
# the keys' mode, the picture's way tried in order and kept, a forced way, the CPU governor set and given back.
# Needs only bash. Prints H700_LAUNCHER_PASS at the end.
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
LAUNCHER="$HERE/package/knulli-smartpro/Mutable Dreams.sh"
ROOT="$(mktemp -d)"
FAILS=0
check() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$3], got [$2]"; FAILS=$((FAILS + 1)); fi; }

# new_case NAME SCREEN(drm|fb:W,H|none) MODES_LINE -> a card: NAME/Mutable Dreams.sh, NAME/mutabledreams/mutable-dreams
new_case() {
    local d="$ROOT/$1"
    mkdir -p "$d/mutabledreams" "$d/sys/devices/system/cpu/cpufreq/policy0"
    cp "$LAUNCHER" "$d/Mutable Dreams.sh"
    case "$2" in
        drm:*) mkdir -p "$d/sys/class/drm/card0-DSI-1"; printf '%s\n' "${2#drm:}" > "$d/sys/class/drm/card0-DSI-1/modes"
               echo connected > "$d/sys/class/drm/card0-DSI-1/status";;
        fb:*)  mkdir -p "$d/sys/class/graphics/fb0"; printf '%s\n' "${2#fb:}" > "$d/sys/class/graphics/fb0/virtual_size";;
    esac
    printf 'schedutil\n' > "$d/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
    printf 'conservative ondemand userspace powersave performance schedutil\n' > "$d/sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors"
    cat > "$d/mutabledreams/mutable-dreams" <<'FAKE'
#!/bin/bash
# FAKE: "<way>:<what>,...": hang (no frame), crash3 (exit 3 before a frame), ok<N> (a frame, then exit N).
way=brick; [ -n "${MD_SCREEN:-}" ] && way=fit
what=""
for pair in ${FAKE//,/ }; do [ "${pair%%:*}" = "$way" ] && what="${pair#*:}"; done
gov=$(cat "$H7_ROOT/sys/devices/system/cpu/cpufreq/policy0/scaling_governor")
echo "$way $what out=${MD_OUT:-none} screen=${MD_SCREEN:-none} present=${MD_PRESENT:-none} keys=${MD_KEYS:-} device=${MD_DEVICE:-} gov=$gov" >> "$TRACE"
case "$what" in
    hang) exec sleep 30;;
    crash3) exit 3;;
    ok*) echo "startup: first frame presented"; sleep 0.3; exit "${what#ok}";;
    *) exit 99;;
esac
FAKE
    chmod +x "$d/mutabledreams/mutable-dreams" "$d/Mutable Dreams.sh"
    echo "$d"
}
run() { local d="$1"; shift; TRACE="$d/trace.txt" FAKE="$1" H7_ROOT="$d" H7_FIRST_FRAME_SECONDS=1 bash "$d/Mutable Dreams.sh"; echo $?; }
trace() { tr '\n' '|' < "$1/trace.txt" | sed 's/|$//'; }
mode() { [ -f "$1/mutabledreams/video-mode.txt" ] && tr -d '\n' < "$1/mutabledreams/video-mode.txt" || echo none; }

d=$(new_case smartpro drm:1280x720); rc=$(run "$d" "fit:ok0")
check "1280x720 panel: the stage 960 x 720, the screen's own size, handheld keys, the name, performance while it runs" "$(trace "$d")" \
      "fit ok0 out=960x720 screen=1 present=none keys=handheld device=TRIMUI SMART PRO gov=performance"
check "1280x720: status 0" "$rc" 0
check "1280x720: the fitting way kept" "$(mode "$d")" fit
check "the governor given back afterwards" "$(cat "$d/sys/devices/system/cpu/cpufreq/policy0/scaling_governor")" schedutil
check "the log names the screen" "$(grep -c 'screen: 1280 x 720, the stage at 960 x 720' "$d/mutabledreams/log.txt")" 1

d=$(new_case nothing none); run "$d" "fit:ok0" > /dev/null
check "no size found: 1280 x 720 taken, the stage 960 x 720" "$(trace "$d" | sed 's/.*out=\([0-9a-z]*\).*/\1/')" 960x720
d=$(new_case fbonly fb:1280,1440); run "$d" "fit:ok0" > /dev/null
check "no DRM, framebuffer 1280,1440 (two pages): 960 x 720" "$(trace "$d" | sed 's/.*out=\([0-9a-z]*\).*/\1/')" 960x720
d=$(new_case brickpanel drm:1024x768); run "$d" "fit:ok0" > /dev/null
check "a 1024x768 panel: the stage as drawn, no scaling" "$(trace "$d" | sed 's/.*out=\([0-9a-z]*\).*/\1/')" none
d=$(new_case wide drm:1920x1080); run "$d" "fit:ok0" > /dev/null
check "1920x1080: the stage 1440 x 1080" "$(trace "$d" | sed 's/.*out=\([0-9a-z]*\).*/\1/')" 1440x1080

d=$(new_case fallback drm:1280x720); rc=$(run "$d" "fit:hang,brick:ok0")
check "no picture the fitting way: the Brick's way next, status 0" "$rc" 0
check "both ways tried in order; the Brick's way without scaling or fitting, into the window" "$(trace "$d" | sed 's/ keys=[^|]*//g')" \
      "fit hang out=960x720 screen=1 present=none|brick ok0 out=none screen=none present=window"
check "the Brick's way kept" "$(mode "$d")" brick
rc=$(run "$d" "fit:ok3,brick:ok7")
check "second start: the kept way first, its status passed on" "$rc" 7
check "second start: only the Brick's way" "$(tail -n 1 "$d/trace.txt" | cut -d' ' -f1,2)" "brick ok7"

d=$(new_case crash drm:1280x720); rc=$(run "$d" "fit:crash3,brick:ok0")
check "the fitting way crashes before a picture: the Brick's way, status 0" "$rc" 0
d=$(new_case none-work drm:1280x720); rc=$(run "$d" "fit:hang,brick:hang")
check "no way works: status 124, nothing kept" "$rc/$(mode "$d")" "124/none"
check "no way works: governor given back" "$(cat "$d/sys/devices/system/cpu/cpufreq/policy0/scaling_governor")" schedutil

d=$(new_case forced-brick drm:1280x720); : > "$d/mutabledreams/brick.on"; run "$d" "fit:ok0,brick:ok0" > /dev/null
check "brick.on: only the Brick's way, nothing kept" "$(trace "$d" | cut -d' ' -f1)/$(mode "$d")" "brick/none"
d=$(new_case forced-fit drm:1280x720); : > "$d/mutabledreams/fit.on"; run "$d" "fit:hang,brick:ok0" > /dev/null
check "fit.on: only the fitting way (it hangs: no fallback)" "$(trace "$d" | cut -d' ' -f1)" fit
d=$(new_case nofast drm:1280x720); : > "$d/mutabledreams/nofast.on"; run "$d" "fit:ok0" > /dev/null
check "nofast.on: the governor left alone" "$(trace "$d" | sed 's/.*gov=//')" schedutil

rm -rf "$ROOT"
echo
[ "$FAILS" -eq 0 ] && { echo "SMARTPRO_LAUNCHER_PASS"; exit 0; }
echo "SMARTPRO_LAUNCHER_FAIL: $FAILS"; exit 1
