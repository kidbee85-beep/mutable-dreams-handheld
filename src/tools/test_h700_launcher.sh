#!/bin/bash
# GPL-3.0-only. tools/test_h700_launcher.sh: the Anbernic H700 launcher (package/knulli-h700/Mutable Dreams.sh) with a
# stand-in for the program and a made-up /sys and /proc (H7_ROOT): the screen's size and the stage made to fit it,
# the keys' mode, the picture's way tried in order and kept, a forced way, the CPU governor set and given back.
# Needs only bash. Prints H700_LAUNCHER_PASS at the end.
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
LAUNCHER="$HERE/package/knulli-h700/Mutable Dreams.sh"
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
way=surface; [ -z "${MD_PRESENT:-}" ] && way=renderer
what=""
for pair in ${FAKE//,/ }; do [ "${pair%%:*}" = "$way" ] && what="${pair#*:}"; done
gov=$(cat "$H7_ROOT/sys/devices/system/cpu/cpufreq/policy0/scaling_governor")
echo "$way $what out=${MD_OUT:-none} keys=${MD_KEYS:-} device=${MD_DEVICE:-} gov=$gov" >> "$TRACE"
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

d=$(new_case rg35xx drm:640x480); rc=$(run "$d" "surface:ok0")
check "640x480 panel: the stage at 640 x 480, handheld keys, the H700's name, performance while it runs" "$(trace "$d")" \
      "surface ok0 out=640x480 keys=handheld device=ANBERNIC H700 gov=performance"
check "640x480: status 0" "$rc" 0
check "640x480: the surface way kept" "$(mode "$d")" surface
check "the governor given back afterwards" "$(cat "$d/sys/devices/system/cpu/cpufreq/policy0/scaling_governor")" schedutil
check "the log names the screen" "$(grep -c 'screen: 640 x 480, the stage at 640 x 480' "$d/mutabledreams/log.txt")" 1

d=$(new_case cube drm:720x720); run "$d" "surface:ok0" > /dev/null
check "720x720 (CubeXX): a 4:3 stage of 720 x 536" "$(trace "$d" | sed 's/.*out=\([0-9x]*\).*/\1/')" 720x536
d=$(new_case rg34 drm:720x480); run "$d" "surface:ok0" > /dev/null
check "720x480 (RG34XX): a 4:3 stage of 640 x 480" "$(trace "$d" | sed 's/.*out=\([0-9x]*\).*/\1/')" 640x480
d=$(new_case fbonly fb:640,960); run "$d" "surface:ok0" > /dev/null
check "no DRM, framebuffer 640,960 (two pages): 640 x 480" "$(trace "$d" | sed 's/.*out=\([0-9x]*\).*/\1/')" 640x480
d=$(new_case nothing none); run "$d" "surface:ok0" > /dev/null
check "no size found: 640 x 480" "$(trace "$d" | sed 's/.*out=\([0-9x]*\).*/\1/')" 640x480
d=$(new_case big drm:1024x768); run "$d" "surface:ok0" > /dev/null
check "1024x768: the stage as drawn, no scaling" "$(trace "$d" | sed 's/.*out=\([0-9a-z]*\).*/\1/')" none

d=$(new_case fallback drm:640x480); rc=$(run "$d" "surface:hang,renderer:ok0")
check "no picture through the surface: the renderer next, status 0" "$rc" 0
check "both ways tried in order" "$(trace "$d" | sed 's/ out=[^|]*//g')" "surface hang|renderer ok0"
check "the renderer kept" "$(mode "$d")" renderer
rc=$(run "$d" "surface:hang,renderer:ok7")
check "second start: the kept way first, its status passed on" "$rc" 7
check "second start: only the renderer" "$(tail -n 1 "$d/trace.txt" | cut -d' ' -f1,2)" "renderer ok7"

d=$(new_case crash drm:640x480); rc=$(run "$d" "surface:crash3,renderer:ok0")
check "the surface way crashes before a picture: the renderer, status 0" "$rc" 0
d=$(new_case none-work drm:640x480); rc=$(run "$d" "surface:hang,renderer:hang")
check "no way works: status 124, nothing kept" "$rc/$(mode "$d")" "124/none"
check "no way works: governor given back" "$(cat "$d/sys/devices/system/cpu/cpufreq/policy0/scaling_governor")" schedutil

d=$(new_case forced drm:640x480); : > "$d/mutabledreams/nosurface.on"; rc=$(run "$d" "surface:ok0,renderer:ok0")
check "nosurface.on: only the renderer, nothing kept" "$(trace "$d" | cut -d' ' -f1)/$(mode "$d")" "renderer/none"
d=$(new_case nofast drm:640x480); : > "$d/mutabledreams/nofast.on"; run "$d" "surface:ok0" > /dev/null
check "nofast.on: the governor left alone" "$(trace "$d" | sed 's/.*gov=//')" schedutil

rm -rf "$ROOT"
echo
[ "$FAILS" -eq 0 ] && { echo "H700_LAUNCHER_PASS"; exit 0; }
echo "H700_LAUNCHER_FAIL: $FAILS"; exit 1
