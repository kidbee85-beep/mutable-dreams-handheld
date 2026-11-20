#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Mutable Dreams by Kid Bunka: put it into a Linux system as a program of its own (FreeDesktop: menu entry, icon,
# command). For a system image (run inside the image's root) or for one machine.
#
#   sh install.sh [PREFIX] [APPNAME]        PREFIX default /usr/local, APPNAME default "Dream Garden"
#   (in Mutable Dreams Linux the program is called by a name of its own; the command stays mutable-dreams)
#   DESTDIR=/path/to/rootfs sh install.sh /usr    (into an OS image being built)
#
# Program and its read-only files go to PREFIX/lib/mutable-dreams; the command PREFIX/bin/mutable-dreams; the menu
# entry PREFIX/share/applications/mutable-dreams.desktop; the icon PREFIX/share/icons/hicolor/256x256/apps.
# Each user's own dreams live in ${XDG_DATA_HOME:-~/.local/share}/mutable-dreams (made on the first start; an
# update renews the program files there but never touches save/ or samples/).
set -eu
PREFIX=${1:-/usr/local}
APPNAME=${2:-Dream Garden}
APPID=org.kidbunka.mutabledreams.dreamgarden          # (the system's naming: org.kidbunka.mutabledreams.<name>)
HERE=$(cd "$(dirname "$0")" && pwd)
D=${DESTDIR:-}
SRC="$HERE/mutabledreams"
[ -x "$SRC/mutable-dreams" ] || { echo "no program in $SRC (copy the build for this machine there: see README)"; exit 1; }
mkdir -p "$D$PREFIX/lib/mutable-dreams" "$D$PREFIX/bin" "$D$PREFIX/share/applications" \
         "$D$PREFIX/share/icons/hicolor/256x256/apps" "$D$PREFIX/share/metainfo"
( cd "$SRC" && find . -type f ! -name '._*' ! -path './save/*' ! -name 'log*.txt' | while read -r f; do
    mkdir -p "$D$PREFIX/lib/mutable-dreams/$(dirname "$f")"
    cp "$f" "$D$PREFIX/lib/mutable-dreams/$f"
    if [ -x "$f" ]; then chmod 755 "$D$PREFIX/lib/mutable-dreams/$f"; else chmod 644 "$D$PREFIX/lib/mutable-dreams/$f"; fi
  done )
if [ -d "$HERE/doc" ]; then                       # (0.80) the manual, German and English (the menu entry's "Handbuch")
    mkdir -p "$D$PREFIX/share/doc/mutable-dreams"
    cp "$HERE"/doc/*.pdf "$D$PREFIX/share/doc/mutable-dreams/"
    chmod 644 "$D$PREFIX"/share/doc/mutable-dreams/*.pdf
fi
sed "s|@PREFIX@|$PREFIX|g" "$HERE/mutable-dreams-launcher" > "$D$PREFIX/bin/mutable-dreams"
chmod 755 "$D$PREFIX/bin/mutable-dreams"
sed -e "s|@PREFIX@|$PREFIX|g" -e "s|@APPNAME@|$APPNAME|g" "$HERE/mutable-dreams.desktop" > "$D$PREFIX/share/applications/$APPID.desktop"
cp "$HERE/mutable-dreams.png" "$D$PREFIX/share/icons/hicolor/256x256/apps/$APPID.png"
sed "s|@APPNAME@|$APPNAME|g" "$HERE/mutable-dreams.metainfo.xml" > "$D$PREFIX/share/metainfo/$APPID.metainfo.xml"
echo "installed \"$APPNAME\" into $D$PREFIX (command: mutable-dreams)"
