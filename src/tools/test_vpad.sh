#!/bin/zsh
# tools/test_vpad.sh: the controller path of the SDL front end with a virtual game controller (Mac only, SDL 2.0.14+): the
# sticks' clicks (F1, F2), SELECT and MENU opening the menu, a stick turning the first dial. Prints "vpad: PASS" at the end.
# With ASAN=1 under AddressSanitizer and UndefinedBehaviorSanitizer. The screen as on the Trimui Smart Pro (a 1280 x 720 test
# window, the stage 960 x 720 in the middle).
cd "${0:A:h}/.."
if [ "$ASAN" = 1 ]; then
  OUT_SUFFIX=-asan EXTRA_CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" python3 build.py sdl > /dev/null 2>&1 || { echo "build failed"; exit 1; }
  BIN=build/mac-sdl-asan/mutable-dreams
else
  python3 build.py sdl > /dev/null 2>&1 || { echo "build failed"; exit 1; }
  BIN=build/mac-sdl/mutable-dreams
fi
mkdir -p out/vpad; : > out/vpad/a.raw
ASAN_OPTIONS=detect_leaks=0 SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out/vpad/a.raw \
  MD_VPAD_TEST=1 MD_SCREEN=1280x720 MD_OUT=960x720 MD_KEYS=handheld MD_AUTOPLAY=1 MD_NO_SPLASH=1 MD_RUN_SECONDS=90 $BIN out/vpad/save > out/vpad/log.txt 2>&1
grep -E '^vpad:' out/vpad/log.txt
echo "sanitizer reports: $(grep -c 'ERROR: AddressSanitizer\|runtime error' out/vpad/log.txt)"
