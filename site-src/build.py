#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""site-src/build.py: writes the project website (docs/index.html in English, docs/de/index.html in German) in the look of
Mutable Dreams: the night garden's colours, Nunito and Barlow Semi Condensed, buttons as small pebbles, leaves for hints.
Every picture is a real frame from the program (docs/assets/img); the animated loops come from its renderer; the hero's
living garden is the Visual Kit (docs/assets/js/kit.js, no libraries, no internet).
Run:  python3 site-src/build.py   (from the repository root)"""
import html, os, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
VERSION = "1.5.10"
OLD = "1.5.9"                                   # the packages that did not change stay in the 1.5.9 release
REPO = "kidbee85-beep/mutable-dreams-handheld"
REL = f"https://github.com/{REPO}/releases/download/v{VERSION}/"
REL_OLD = f"https://github.com/{REPO}/releases/download/v{OLD}/"


def rel(name):                                  # where a file is: the packages of 1.5.9 that did not change, else the newest release
    return (REL_OLD if f"-{OLD}." in name else REL) + name
SITE = "https://kidbee85-beep.github.io/mutable-dreams-handheld/"
FM1 = "https://github.com/kidbee85-beep/mutable-dreams"
SIZES = {}                                      # file name -> bytes, from release-files.txt if present
rf = ROOT / "site-src/release-files.txt"
if rf.exists():
    for line in rf.read_text().splitlines():
        if "\t" in line:
            n, b = line.split("\t")
            SIZES[n] = int(b)


def mb(name):
    b = SIZES.get(name)
    return f" · {b / 1048576:.1f} MB" if b else ""


def k(s):                                       # a button as a pebble
    return f"<kbd>{s}</kbd>"


# ------------------------------------------------------------------ the words, in both languages
T = {
 "en": {
  "lang": "en", "dir": "", "other": ("de/", "Deutsch"), "root": "",
  "title": f"Mutable Dreams {VERSION} for handhelds – a garden of sound",
  "desc": "Mutable Dreams turns a handheld into a dream garden of sound: six tracks, 272 Biomes, 96 worlds, a Sound Lab with Mutable Instruments modules and ten musical toys. For Trimui Brick and Smart Pro, Miyoo Mini, RGB20S, H700, Android and Linux. Free, GPL-3.0.",
  "nav": [("garden", "The garden"), ("features", "Features"), ("devices", "Devices"), ("controls", "Controls"), ("ideas", "Ideas"), ("download", "Download")],
  "kicker": "For handhelds · by Kid Bunka",
  "sub": "A garden of sound that makes music by itself – and answers you.",
  "cta1": "Download", "cta2": "Manual (PDF)", "cta3": "Source on GitHub",
  "meta": f"Version {VERSION} · GPL-3.0 · English and German",
  "stats": [("6", "tracks, six plants"), ("272", "Biomes"), ("96", "worlds"), ("18", "Sound Lab modules"), ("10", "musical toys"), ("24", "Plaits models")],
  "pillars_h": "Tend it, and it answers",
  "pillars_p": "Mutable Dreams is a generative instrument. It plays by itself, the way a garden grows by itself. You do not need to read music or program anything: turn a dial, choose another Biome or play a note, and the music changes in a way you can hear and see.",
  "pillars": [
    ("Garden", "Six plants are the six voices of the music; four dials are its weather.", "02-garden-playing"),
    ("Sound Lab", "Synths, grain clouds, reverb, echo and resonators after Mutable Instruments.", "10-lab"),
    ("Worlds", "96 ready-made scenes with their own tempo, sounds and a line of story.", "29-worlds"),
    ("Playground", "Bouncing balls, planets, rain and pendulums that make music with you.", "31-toy"),
  ],
  "loops_h": "Alive, not a picture",
  "loops_p": "These loops are recorded from the program itself: the garden, the space effects and the dream map.",
  "loops": [("garden", "The garden: plants bloom on each note, the pond ripples, fireflies follow the beat."),
            ("space", "Space: echo, grain memory drawn as tape, reels whose speed is the pitch."),
            ("dream", "The dream map: a journey through Biomes, stations along a route of light.")],
  "feat_h": "Everything it can do",
  "feat_p": "A tour of every part, with pictures from the program. The manual (PDF) goes deeper.",
  "devices_h": "Made for the buttons in your hands",
  "devices_p": "One program, tuned for each handheld: a lighter picture on the Miyoo, a stage that fits every screen, touch on Android.",
  "dev": [
    ("Trimui Brick", "Knulli · 1024×768", "Brick", "ok", "Tested on Brick with earlier builds: 28 fps was measured on 1.5.0. Exact-1.5.9 hardware confirmation is still pending."),
    ("Miyoo Mini Plus", "Onion OS · 640×480", "Plus", "ok", "Earlier device builds ran at about 15 fps. 1.5.9 requests 20 fps; measured drawing work per frame is 22–30% lower. Exact-build device confirmation is pending."),
    ("Miyoo Mini · Mini Flip", "Onion OS · 640×480", "Mini", "ok", "The Flip is recognised and speaker audio was confirmed on an earlier build. USB-C adapter audio remains unverified."),
    ("Powkiddy RGB20S", "AmberELEC · 640×480", "RGB20S", "ok", "1.5.9 confirmed on the device. Earlier 1.5.7 measurements: 20 fps and one audio underrun in 60 seconds."),
    ("Trimui Smart Pro", "Knulli · 1280×720 · two sticks", "SmartPro", "test", "Community test (new in 1.5.10): the garden sits in the middle of the 16:9 screen; the sticks turn the dials. Not yet run on a real Smart Pro."),
    ("Anbernic H700", "Knulli · RG35XX, RG40XX, CubeXX, RG34XX", "H700", "test", "Community test: the launcher fits the stage to the screen. Not yet run on a real H700."),
    ("Android", "Phones, tablets, gamepads", "Android", "ok", "1.5.9 confirmed on a device. Touch buttons beside the stage, several fingers at once; or play with a gamepad."),
    ("Linux PC", "x86_64 and aarch64", "Linux", "new", "Packages built for x86_64 and aarch64, with a “Dream Garden” start-menu entry. No real-PC run confirmed in this review; feedback welcome."),
  ],
  "badge": {"ok": "Runs on the device", "test": "Community test", "new": "Feedback welcome"},
  "controls_h": "The buttons",
  "controls_p": "Mutable Dreams is made for the Brick's buttons. Two rules matter most:",
  "rules": [f"{k('B')} always goes one step back, the way you came.", f"Holding {k('B')} takes you home to the garden from anywhere."],
  "hand_h": "No MENU key? One tap on SELECT.",
  "hand_p": f"On the Miyoo handhelds, the RGB20S, the H700 and Android gamepads, tap {k('SELECT')} for the menu with every place, the options and <strong>Quit</strong> (press {k('UP')} once, then {k('A')}). Hold {k('SELECT')} for the help. Tap {k('L2')} for the quick bar, which also switches play mode on and off. The hint line at the bottom names {k('SELECT')} MENU right after the place's name.",
  "sp_cap": "The Trimui Smart Pro's 16:9 screen: the garden (4:3) sits 1:1 in the middle, black beside it.",
  "sp_h": "Trimui Smart Pro: the sticks",
  "sp_p": f"The two sticks turn the four dials: the left stick {k('K1')} and {k('K2')}, the right stick {k('K3')} and {k('K4')}. The further a stick is pushed, the faster the dial turns; a stick at rest does nothing. Clicking the sticks ({k('L3')}, {k('R3')}) is {k('F1')} (Sound Lab) and {k('F2')} (mixer). There is no FN switch: play mode is in the quick bar ({k('L2')}).",
  "touch_h": "Touch on Android",
  "touch_p": f"The Brick's buttons lie beside the stage: D-pad, {k('L1')}, {k('L2')} and {k('SELECT')} on the left; {k('A')} {k('B')} {k('X')} {k('Y')}, {k('R1')}, {k('R2')}, {k('START')} and {k('MENU')} on the right. Several fingers at once and sliding work, so held buttons and combinations behave as on the device. The back key is {k('B')}. Press a gamepad button and the touch buttons step aside for a larger stage.",
  "map_cap": "Hold SELECT and the help appears: a drawn Brick shows every button and what it does. L1/R1 turn through five pages.",
  "table_h": ["Button", "In the garden", "With R2 held"],
  "table": [("UP DOWN", "choose one of the four dials", "the track's sound model"), ("LEFT RIGHT", "turn the dial (L2 held: fine)", "another Biome"),
            ("L1 R1", "choose a track", "octave"), ("A", "select, mute a track", "LOCK: keep the Biome"), ("B", "back; hold: home", "FREEZE everything"),
            ("X", "edit pages", "envelope"), ("Y", "effect pages", "global"), ("START", "play / stop", "record"),
            ("SELECT", "tap: where am I? · hold: help", "hold 1 s: save everything"), ("MENU", "where to? the menu", "dream map"),
            ("F1 · F2", "Sound Lab · mixer", "· Biome atlas"), ("L2", "tap: quick bar", ""), ("FN", "switch: play mode, the buttons play notes", "")],
  "ideas_h": "Ideas for making music",
  "ideas_p": "Seven small recipes, from falling asleep to playing live.",
  "ideas": [
    ("A quiet evening", "29-worlds", [f"Open <strong>Worlds</strong> and enter <em>Kelp Cathedral</em> from <em>Deep Waters</em>.", "Turn MIST up and FIREFLIES down a little.", f"Press {k('Y')} to wander. Every few minutes the garden glides into the next world."]),
    ("A rhythm of your own", "35-toy", [f"On the playground, go to the <strong>Wheel</strong> and set three rings, for example 3 in 8, 5 in 16 and 4 in 16.", f"Choose the track with {k('X')}, then keep the pattern with {k('R2')} + {k('X')}.", "Open <strong>GRIDS</strong> in the Sound Lab to add kick, snare and hi-hat."]),
    ("A melody that stays", "34-toy", ["Catch four or five notes with the <strong>Firefly</strong>.", f"Listen for a few bars, then keep them as a sequence with {k('R2')} + {k('X')}.", f"Refine it in the <strong>Sequence</strong> with {k('X')}, for example with chance and gate."]),
    ("Voices in the garden", "28-archive", ["Fetch a poem in the <strong>Archive</strong> (needs WiFi).", f"Send it to VOICES with {k('A')}, and choose FRAGMENTS or GRAINS.", "Add a little SHIMMER, and the words float through the garden."]),
    ("Falling asleep", "60-options", ["Enter one of the <em>Lullabies</em> in Worlds: it sets the sleep timer itself. Or choose 15, 30, 60 or 90 minutes in the options.", "Turn GLOW down.", "In the last minute the music fades out, and the screen rests dark until a button wakes it."]),
    ("Playing live", "06-playmode", [f"Switch on play mode: the {k('FN')} switch on the Brick, or PLAY MODE in the quick bar ({k('L2')}).", f"The D-pad and {k('A')} {k('B')} {k('X')} {k('Y')} now play the Biome's scale, so nothing can sound wrong; {k('L1')}/{k('R1')} change the track.", f"Let a sequence run with {k('START')} and play over it; hold {k('R2')} for chords."]),
    ("On the go", "05-menu", [f"Tap {k('SELECT')} for the menu and pick a place; on a phone, use the touch buttons.", f"Tap {k('L2')} for the quick bar: PLAY MODE, LIGHT PICTURE and DEUTSCH / ENGLISH are at its end.", f"To stop, tap {k('SELECT')}, press {k('UP')} once to reach <strong>Quit</strong>, then {k('A')}. The Sound Lab and your settings are kept."]),
  ],
  "dl_h": "Download", "dl_p": f"Version {VERSION} · the files are in the GitHub releases {VERSION} (new) and {OLD} (unchanged packages). Checksums are in SHA256SUMS.txt of each release.",
  "dl_cols": ["For", "File"], "dl_all": "All files and checksums on GitHub",
  "inst_h": "Installing", "inst_p": "You can copy a new version over an older one: your saved Biomes, settings, tale progress and samples stay as they are.",
  "inst": [
    ("Trimui Brick (Knulli)", f"Unpack the zip. Copy <b>Mutable Dreams.sh</b> and the folder <b>mutabledreams</b> into <b>roms/ports</b> on the SD card. Update the game list in the Knulli menu and start <strong>Mutable Dreams</strong> from <em>Ports</em>."),
    ("Trimui Smart Pro (Knulli) – community test", "Copy <b>Mutable Dreams.sh</b> and <b>mutabledreams</b> into <b>roms/ports</b>. The screen is 16:9: the garden sits in the middle with black beside it. The sticks turn the four dials (the further, the faster), clicking them is F1 and F2, <b>SELECT</b> opens the menu. <b>SMARTPRO.txt</b> lists switch files; if something is wrong, please send <b>mutabledreams/log.txt</b>."),
    ("Miyoo Mini Plus, Mini, Mini Flip (Onion OS)", "Unpack the zip for your device and copy its folders <b>App</b> and <b>Roms</b> onto the SD card, merging them with the ones there. Start <strong>Mutable Dreams</strong> under <em>Ports</em> or <em>Apps</em>. <b>MIYOO.txt</b> beside the program lists switch files (for example <b>fps15.on</b>)."),
    ("Powkiddy RGB20S (AmberELEC)", "Unpack the zip into <b>roms/ports</b>, update the game list and start <strong>Mutable Dreams</strong> under <em>Ports</em>."),
    ("Anbernic H700 (Knulli) – community test", "Copy <b>Mutable Dreams.sh</b> and <b>mutabledreams</b> into <b>roms/ports</b>. The launcher reads the screen size and fits the stage to it. If something is wrong, please send <b>mutabledreams/log.txt</b>."),
    ("Android", "Install the APK. The app keeps its files (saves, your samples, worlds) in <b>Android/data/com.kidbunka.mutabledreams/files</b>. With a gamepad, play as on a handheld; without, by touch."),
    ("Linux PC", "The Linux package installs the program as <strong>Dream Garden</strong>, with a start-menu icon. Without a game controller, every hint names the keyboard keys instead."),
  ],
  "faq_h": "Questions and help",
  "faq": [
    ("There is no sound.", f"Press {k('START')}: RESTING means the music is stopped. Check the device's volume keys, and if a USB sound card or headphone adapter is plugged in, whether the sound goes there."),
    ("I am lost.", f"Hold {k('B')}, and you are home. A short tap on {k('SELECT')} tells you where you are."),
    ("The picture stutters.", "Switch on LIGHT PICTURE in the quick bar. It also comes on by itself when the stage stays slow. The sound always comes before the picture."),
    ("How do I begin the tale again?", "Delete <b>mutabledreams/save/tale.txt</b>."),
    ("I want everything back as new.", "Rename the folder <b>mutabledreams/save</b>. On the next start everything is new, and your old state stays in the renamed folder."),
    ("Can I use my own sounds?", "Yes: WAV files in <b>mutabledreams/samples</b> (SAMPLES module) and voices as Ogg or WAV in <b>samples/voices</b> (VOICES module)."),
    ("Does it work with MIDI?", "A class-compliant USB keyboard or controller plays POLY when POLY is on; otherwise MIDI channels 1–6 play the six tracks. MIDI only goes in."),
  ],
  "credits_h": "Thanks and licences",
  "credits": f"Mutable Dreams for handhelds is by Kid Bunka. It grows from <a href=\"{FM1}\">Mutable Dreams 2.1 (FM-1)</a> and is licensed under the GPL-3.0; the sources of this port are in this repository.",
  "credits_li": ["Mutable Instruments modules by Émilie Gillet: Plaits, Rings, Elements, Clouds, Beads, Marbles, Grids, Tides and stmlib (MIT and GPL licences).",
                 "The fonts Barlow and Nunito (SIL Open Font License).", "The archive's voices: LibriVox via archive.org, public domain or freely licensed, credited beside each file.",
                 "Independent from Mutable Instruments, M-VAVE, Trimui, Anbernic, Miyoo and Powkiddy.", "Special thanks to Yimiblú for listening closely and asking for more."],
  "foot": f"Mutable Dreams {VERSION} · by Kid Bunka · all pictures come from the program itself",
  "src": "Source", "manual": "Manual", "release": "Release",
  "top": "Back to top",
 },
 "de": {
  "lang": "de", "dir": "de/", "other": ("../", "English"), "root": "../",
  "title": f"Mutable Dreams {VERSION} für Handhelds – ein Garten aus Klang",
  "desc": "Mutable Dreams macht den Handheld zu einem Traumgarten aus Klang: sechs Spuren, 272 Biome, 96 Welten, ein Klanglabor mit Modulen nach Mutable Instruments und zehn musikalische Spielzeuge. Für Trimui Brick und Smart Pro, Miyoo Mini, RGB20S, H700, Android und Linux. Frei, GPL-3.0.",
  "nav": [("garden", "Der Garten"), ("features", "Funktionen"), ("devices", "Geräte"), ("controls", "Tasten"), ("ideas", "Ideen"), ("download", "Download")],
  "kicker": "Für Handhelds · von Kid Bunka",
  "sub": "Ein Garten aus Klang, der von selbst Musik macht – und dir antwortet.",
  "cta1": "Download", "cta2": "Handbuch (PDF)", "cta3": "Quellen auf GitHub",
  "meta": f"Version {VERSION} · GPL-3.0 · Deutsch und Englisch",
  "stats": [("6", "Spuren, sechs Pflanzen"), ("272", "Biome"), ("96", "Welten"), ("18", "Module im Klanglabor"), ("10", "musikalische Spielzeuge"), ("24", "Plaits-Modelle")],
  "pillars_h": "Pflege ihn, und er antwortet",
  "pillars_p": "Mutable Dreams ist ein generatives Instrument. Es spielt von selbst, so wie ein Garten von selbst wächst. Du musst keine Noten lesen und nichts programmieren: Dreh an einem Regler, wähle ein anderes Biom oder spiel eine Note, und die Musik verändert sich hörbar und sichtbar.",
  "pillars": [
    ("Garten", "Sechs Pflanzen sind die sechs Stimmen der Musik, vier Regler ihr Wetter.", "02-garden-playing"),
    ("Klanglabor", "Synths, Körnerwolken, Hall, Echo und Resonatoren nach Mutable Instruments.", "10-lab"),
    ("Welten", "96 fertige Szenen mit eigenem Tempo, eigenen Klängen und einer Zeile Geschichte.", "29-worlds"),
    ("Spielwiese", "Hüpfende Bälle, Planeten, Regen und Pendel, die mit dir Musik machen.", "31-toy"),
  ],
  "loops_h": "Lebendig, kein Standbild",
  "loops_p": "Diese Schleifen stammen aus dem Programm selbst: der Garten, die Raum-Effekte und die Traumkarte.",
  "loops": [("garden", "Der Garten: Pflanzen blühen bei jeder Note, der Teich kräuselt sich, Glühwürmchen folgen dem Takt."),
            ("space", "Raum: Echo, Körnerspeicher als Band gezeichnet, Spulen, deren Tempo die Tonhöhe ist."),
            ("dream", "Die Traumkarte: eine Reise durch die Biome, Stationen entlang einer Lichtroute.")],
  "feat_h": "Alles, was es kann",
  "feat_p": "Ein Rundgang durch jeden Teil, mit Bildern aus dem Programm. Das Handbuch (PDF) geht tiefer.",
  "devices_h": "Gemacht für die Tasten in deiner Hand",
  "devices_p": "Ein Programm, auf jedes Gerät abgestimmt: ein leichteres Bild auf dem Miyoo, eine Bühne, die auf jeden Bildschirm passt, Touch auf Android.",
  "dev": [
    ("Trimui Brick", "Knulli · 1024×768", "Brick", "ok", "Auf dem Brick mit früheren Builds getestet: 28 fps wurden mit 1.5.0 gemessen. Eine Gerätebestätigung genau für 1.5.9 steht noch aus."),
    ("Miyoo Mini Plus", "Onion OS · 640×480", "Plus", "ok", "Frühere Geräte-Builds liefen mit etwa 15 fps. 1.5.9 fordert 20 fps an; gemessen sind 22–30% weniger Zeichenarbeit pro Bild. Eine Gerätebestätigung dieses Builds steht noch aus."),
    ("Miyoo Mini · Mini Flip", "Onion OS · 640×480", "Mini", "ok", "Der Flip wird erkannt, Lautsprecherton wurde mit einem früheren Build bestätigt. USB-C-Adapterton bleibt ungeprüft."),
    ("Powkiddy RGB20S", "AmberELEC · 640×480", "RGB20S", "ok", "1.5.9 am Gerät bestätigt. Frühere Messung mit 1.5.7: 20 fps und ein Audio-Aussetzer in 60 Sekunden."),
    ("Trimui Smart Pro", "Knulli · 1280×720 · zwei Sticks", "SmartPro", "test", "Community-Test (neu in 1.5.10): Der Garten sitzt in der Mitte des 16:9-Bildschirms; die Sticks drehen die Regler. Auf einem echten Smart Pro noch nicht gelaufen."),
    ("Anbernic H700", "Knulli · RG35XX, RG40XX, CubeXX, RG34XX", "H700", "test", "Community-Test: Der Starter passt die Bühne dem Bildschirm an. Auf einem echten H700 noch nicht gelaufen."),
    ("Android", "Handys, Tablets, Gamepads", "Android", "ok", "1.5.9 am Gerät bestätigt. Touch-Tasten neben der Bühne, mehrere Finger gleichzeitig; oder mit Gamepad spielen."),
    ("Linux-PC", "x86_64 und aarch64", "Linux", "new", "Pakete für x86_64 und aarch64 mit „Dream Garden“ im Startmenü sind gebaut. Kein echter PC-Lauf in dieser Prüfung bestätigt; Rückmeldungen willkommen."),
  ],
  "badge": {"ok": "Läuft am Gerät", "test": "Community-Test", "new": "Rückmeldung willkommen"},
  "controls_h": "Die Tasten",
  "controls_p": "Mutable Dreams ist für die Tasten des Brick gemacht. Zwei Regeln sind die wichtigsten:",
  "rules": [f"{k('B')} geht immer einen Schritt zurück, auf dem Weg, den du gekommen bist.", f"{k('B')} gehalten bringt dich von überall nach Hause in den Garten."],
  "hand_h": "Keine MENU-Taste? Ein Tipp auf SELECT.",
  "hand_p": f"Auf den Miyoo-Geräten, dem RGB20S, dem H700 und bei Android-Gamepads öffnet ein Tipp auf {k('SELECT')} das Menü mit allen Orten, den Optionen und <strong>Beenden</strong> (einmal {k('UP')}, dann {k('A')}). {k('SELECT')} gehalten zeigt die Hilfe. Ein Tipp auf {k('L2')} öffnet die Schnellleiste, die auch den Spielmodus ein- und ausschaltet. Die Hinweiszeile unten nennt {k('SELECT')} MENÜ gleich nach dem Namen des Ortes.",
  "sp_cap": "Der 16:9-Bildschirm des Trimui Smart Pro: Der Garten (4:3) sitzt 1:1 in der Mitte, daneben bleibt es schwarz.",
  "sp_h": "Trimui Smart Pro: die Sticks",
  "sp_p": f"Die beiden Sticks drehen die vier Regler: der linke Stick {k('K1')} und {k('K2')}, der rechte {k('K3')} und {k('K4')}. Je weiter ein Stick gedrückt wird, desto schneller dreht der Regler; ein ruhender Stick tut nichts. Ein Klick auf die Sticks ({k('L3')}, {k('R3')}) ist {k('F1')} (Klanglabor) und {k('F2')} (Mixer). Es gibt keinen FN-Schalter: Der Spielmodus liegt in der Schnellleiste ({k('L2')}).",
  "touch_h": "Touch auf Android",
  "touch_p": f"Die Tasten des Brick liegen neben der Bühne: links Steuerkreuz, {k('L1')}, {k('L2')} und {k('SELECT')}; rechts {k('A')} {k('B')} {k('X')} {k('Y')}, {k('R1')}, {k('R2')}, {k('START')} und {k('MENU')}. Mehrere Finger gleichzeitig und Wischen gehen, gehaltene Tasten und Kombinationen verhalten sich also wie am Gerät. Die Zurück-Taste ist {k('B')}. Drückst du eine Gamepad-Taste, treten die Touch-Tasten für eine größere Bühne zur Seite.",
  "map_cap": "SELECT gehalten: Die Hilfe erscheint. Ein gezeichneter Brick zeigt jede Taste mit ihrer Bedeutung. L1/R1 blättern durch fünf Seiten.",
  "table_h": ["Taste", "Im Garten", "Mit R2 gehalten"],
  "table": [("UP DOWN", "einen der vier Regler wählen", "das Klangmodell der Spur"), ("LEFT RIGHT", "Regler drehen (L2 gehalten: fein)", "anderes Biom"),
            ("L1 R1", "Spur wählen", "Oktave"), ("A", "auswählen, Spur stummschalten", "LOCK: Biom behalten"), ("B", "zurück; gehalten: nach Hause", "alles EINFRIEREN"),
            ("X", "Bearbeiten-Seiten", "Hüllkurve"), ("Y", "Effekt-Seiten", "global"), ("START", "spielen / stoppen", "aufnehmen"),
            ("SELECT", "tippen: wo bin ich? · halten: Hilfe", "1 s halten: alles sichern"), ("MENU", "wohin? das Menü", "Traumkarte"),
            ("F1 · F2", "Klanglabor · Mixer", "· Biom-Atlas"), ("L2", "tippen: Schnellleiste", ""), ("FN", "Schalter: Spielmodus, die Tasten spielen Töne", "")],
  "ideas_h": "Ideen zum Musikmachen",
  "ideas_p": "Sieben kleine Rezepte, vom Einschlafen bis zum Live-Spielen.",
  "ideas": [
    ("Ein ruhiger Abend", "29-worlds", [f"Öffne die <strong>Welten</strong> und betritt <em>Kelp Cathedral</em> aus <em>Deep Waters</em>.", "Dreh NEBEL hoch und LICHTER etwas herunter.", f"Schalte mit {k('Y')} das Wandern ein. Alle paar Minuten gleitet der Garten in die nächste Welt."]),
    ("Ein eigener Rhythmus", "35-toy", [f"Geh auf der Spielwiese zum <strong>Rad</strong> und stell drei Ringe ein, zum Beispiel 3 in 8, 5 in 16 und 4 in 16.", f"Wähle mit {k('X')} die Spur und behalte das Muster mit {k('R2')} + {k('X')}.", "Öffne im Klanglabor <strong>GRIDS</strong> für Kick, Snare und Hi-Hat dazu."]),
    ("Eine Melodie, die bleibt", "34-toy", ["Fang mit dem <strong>Glühwurm</strong> vier oder fünf Noten.", f"Hör ein paar Takte zu und behalte sie mit {k('R2')} + {k('X')} als Sequenz.", f"Verfeinere sie in der <strong>Sequenz</strong> mit {k('X')}, zum Beispiel mit Chance und Länge."]),
    ("Stimmen im Garten", "28-archive", ["Hol im <strong>Archiv</strong> ein Gedicht (braucht WLAN).", f"Schick es mit {k('A')} in VOICES und wähle FRAGMENTS oder GRAINS.", "Etwas SHIMMER dazu, und die Worte schweben durch den Garten."]),
    ("Einschlafen", "60-options", ["Betritt in den Welten eines der <em>Lullabies</em>: Es stellt den Schlaf-Timer selbst ein. Oder wähle in den Optionen 15, 30, 60 oder 90 Minuten.", "Dreh GLÜHEN herunter.", "In der letzten Minute blendet die Musik aus, und der Bildschirm ruht dunkel, bis eine Taste ihn weckt."]),
    ("Live spielen", "06-playmode", [f"Schalte den Spielmodus ein: am Brick mit dem {k('FN')}-Schalter, sonst mit SPIELMODUS in der Schnellleiste ({k('L2')}).", f"Steuerkreuz und {k('A')} {k('B')} {k('X')} {k('Y')} spielen jetzt die Tonleiter des Bioms, es kann also nichts falsch klingen; {k('L1')}/{k('R1')} wechseln die Spur.", f"Lass mit {k('START')} eine Sequenz laufen und spiel darüber; {k('R2')} gehalten spielt Akkorde."]),
    ("Unterwegs", "05-menu", [f"Ein Tipp auf {k('SELECT')} öffnet das Menü, such dir einen Ort aus; auf dem Handy nimmst du die Touch-Tasten.", f"Ein Tipp auf {k('L2')} öffnet die Schnellleiste: SPIELMODUS, LEICHTES BILD und DEUTSCH / ENGLISH liegen an ihrem Ende.", f"Zum Aufhören {k('SELECT')} tippen, einmal {k('UP')} zu <strong>Beenden</strong>, dann {k('A')}. Klanglabor und Einstellungen bleiben erhalten."]),
  ],
  "dl_h": "Download", "dl_p": f"Version {VERSION} · die Dateien liegen in den GitHub-Releases {VERSION} (neu) und {OLD} (unveränderte Pakete). Die Prüfsummen stehen in der SHA256SUMS.txt jedes Releases.",
  "dl_cols": ["Für", "Datei"], "dl_all": "Alle Dateien und Prüfsummen auf GitHub",
  "inst_h": "Installieren", "inst_p": "Eine ältere Version darfst du einfach überschreiben: Deine gespeicherten Biome, Einstellungen, die Traumgeschichte und deine Samples bleiben erhalten.",
  "inst": [
    ("Trimui Brick (Knulli)", "Entpacke das Zip. Kopiere <b>Mutable Dreams.sh</b> und den Ordner <b>mutabledreams</b> auf der SD-Karte nach <b>roms/ports</b>. Aktualisiere im Knulli-Menü die Spieleliste und starte <strong>Mutable Dreams</strong> unter <em>Ports</em>."),
    ("Trimui Smart Pro (Knulli) – Community-Test", "Kopiere <b>Mutable Dreams.sh</b> und <b>mutabledreams</b> nach <b>roms/ports</b>. Der Bildschirm ist 16:9: Der Garten sitzt in der Mitte, daneben bleibt es schwarz. Die Sticks drehen die vier Regler (je weiter, desto schneller), ein Klick darauf ist F1 und F2, <b>SELECT</b> öffnet das Menü. <b>SMARTPRO.txt</b> nennt die Schalterdateien; wenn etwas nicht stimmt, schick bitte <b>mutabledreams/log.txt</b>."),
    ("Miyoo Mini Plus, Mini, Mini Flip (Onion OS)", "Entpacke das Zip für dein Gerät und kopiere seine Ordner <b>App</b> und <b>Roms</b> auf die SD-Karte, zusammengeführt mit den vorhandenen. Gestartet wird <strong>Mutable Dreams</strong> unter <em>Ports</em> oder <em>Apps</em>. <b>MIYOO.txt</b> neben dem Programm nennt Schalterdateien (zum Beispiel <b>fps15.on</b>)."),
    ("Powkiddy RGB20S (AmberELEC)", "Entpacke das Zip nach <b>roms/ports</b>, aktualisiere die Spieleliste und starte <strong>Mutable Dreams</strong> unter <em>Ports</em>."),
    ("Anbernic H700 (Knulli) – Community-Test", "Kopiere <b>Mutable Dreams.sh</b> und <b>mutabledreams</b> nach <b>roms/ports</b>. Der Starter liest die Bildschirmgröße und passt die Bühne an. Wenn etwas nicht stimmt, schick bitte <b>mutabledreams/log.txt</b>."),
    ("Android", "Installiere die APK. Die App legt ihre Dateien (Spielstände, deine Samples, Welten) in <b>Android/data/com.kidbunka.mutabledreams/files</b> ab. Mit Gamepad spielst du wie auf einem Handheld, ohne per Touch."),
    ("Linux-PC", "Das Linux-Paket installiert das Programm als <strong>Dream Garden</strong> mit Symbol im Startmenü. Ohne Gamecontroller nennt es in allen Hinweisen die Tasten der Tastatur."),
  ],
  "faq_h": "Fragen und Hilfe",
  "faq": [
    ("Ich höre nichts.", f"Drück {k('START')}: RESTING heißt, die Musik steht. Prüfe die Lautstärketasten des Geräts, und wenn eine USB-Soundkarte oder ein Kopfhörer-Adapter steckt, ob der Ton dorthin geht."),
    ("Ich habe mich verirrt.", f"Halte {k('B')}, und du bist zu Hause. Ein kurzer Tipp auf {k('SELECT')} sagt dir, wo du bist."),
    ("Das Bild ruckelt.", "Schalte in der Schnellleiste LEICHTES BILD ein. Es kommt auch von selbst, wenn die Bühne langsam bleibt. Der Ton geht immer vor dem Bild."),
    ("Wie fange ich die Geschichte neu an?", "Lösche <b>mutabledreams/save/tale.txt</b>."),
    ("Ich will alles wie neu.", "Benenne den Ordner <b>mutabledreams/save</b> um. Beim nächsten Start ist alles neu, und dein alter Stand bleibt im umbenannten Ordner."),
    ("Kann ich eigene Klänge benutzen?", "Ja: WAV-Dateien in <b>mutabledreams/samples</b> (Modul SAMPLES) und Stimmen als Ogg oder WAV in <b>samples/voices</b> (Modul VOICES)."),
    ("Geht MIDI?", "Ein klassenkonformes USB-Keyboard oder ein Controller spielt POLY, wenn POLY an ist; sonst spielen die MIDI-Kanäle 1–6 die sechs Spuren. MIDI geht nur hinein."),
  ],
  "credits_h": "Dank und Lizenzen",
  "credits": f"Mutable Dreams für Handhelds ist von Kid Bunka. Es wächst aus <a href=\"{FM1}\">Mutable Dreams 2.1 (FM-1)</a> und steht unter der GPL-3.0; die Quellen dieses Ports liegen in diesem Repository.",
  "credits_li": ["Mutable-Instruments-Module von Émilie Gillet: Plaits, Rings, Elements, Clouds, Beads, Marbles, Grids, Tides und stmlib (MIT- und GPL-Lizenzen).",
                 "Die Schriften Barlow und Nunito (SIL Open Font License).", "Die Stimmen des Archivs: LibriVox über archive.org, gemeinfrei oder frei lizenziert, neben jeder Datei genannt.",
                 "Unabhängig von Mutable Instruments, M-VAVE, Trimui, Anbernic, Miyoo und Powkiddy.", "Besonderer Dank an Yimiblú fürs genaue Zuhören und das Verlangen nach mehr."],
  "foot": f"Mutable Dreams {VERSION} · von Kid Bunka · alle Bilder stammen aus dem Programm selbst",
  "src": "Quellen", "manual": "Handbuch", "release": "Release",
  "top": "Nach oben",
 },
}

# ---- the feature tour: (anchor, kicker, title, text, [images], extra) per language
def tour(L):
    en = L == "en"
    chips = lambda items, cols: "".join(f'<span class="chip" style="--c:{c}">{html.escape(t)}</span>' for t, c in zip(items, cols))
    dial_cols = ["#d6ff96", "#a0dceb", "#faaa78", "#c8d2f0"]
    dials = ([("FIREFLIES", "Density: how many notes are played", "fireflies in the air"), ("WIND", "Motion: how much the music changes", "the plants sway"),
              ("GLOW", "Colour: dark or bright, soft or radiant", "the light in the sky"), ("MIST", "Space: reverb and width", "mist, and the mirror in the pond")] if en else
             [("LICHTER", "Dichte: wie viele Noten gespielt werden", "Glühwürmchen in der Luft"), ("WIND", "Bewegung: wie stark sich die Musik verändert", "die Pflanzen wiegen sich"),
              ("GLÜHEN", "Farbe: dunkel oder hell, sanft oder strahlend", "das Licht am Himmel"), ("NEBEL", "Raum: Hall und Weite", "Nebel und die Spiegelung im Teich")])
    dial_html = '<div class="dials">' + "".join(
        f'<div class="dial" style="--c:{c}"><b>{n}</b><span>{a}</span><em>{b}</em></div>' for (n, a, b), c in zip(dials, dial_cols)) + "</div>"
    modules = (["TEXTURES", "SAMPLES", "VOICES", "POLY", "SYNTH", "RANDOM", "LOGIC", "GRIDS", "RACK A", "RACK B", "CLOUD", "PARTICLES", "SEEDS", "SHIMMER", "CROSSMOD", "SENDS", "COLOUR", "MASTER"]
               if en else ["TEXTURES", "SAMPLES", "VOICES", "POLY", "SYNTH", "RANDOM", "LOGIC", "GRIDS", "RACK A", "RACK B", "CLOUD", "PARTICLES", "SEEDS", "SHIMMER", "CROSSMOD", "SENDS", "FARBE", "MASTER"])
    mod_cols = ["#aecfb6", "#dcc694", "#86c2b4", "#a5bed9", "#c8b1d8", "#cca091"] * 3
    families = ["Gardens of Number", "Bells and Resonance", "Drones and Overtones", "Rain and Water", "Night Choirs", "Endless", "Voices and Poems", "Machines and Automata",
                "Plaits Orchestra", "Rhythm Gardens", "Resonant Rooms", "Sky Machines", "Forest Floor", "Deep Waters", "Glass and Air", "Lullabies"]
    toys = ([("Bounce", "Balls bounce in a box, and every bounce rings.", "30-toy"), ("Orbits", "Up to five planets circle a sun; each sings at the gate, so polyrhythms appear.", "31-toy"),
             ("Rain", "A cloud rains on the lily pads.", "32-toy"), ("Swing", "Eight pendulums drift apart into waves and come together again.", "33-toy"),
             ("Firefly", "Steer a firefly to the glowing notes; each one caught joins a looping melody.", "34-toy"), ("Wheel", "Four rhythm circles, each a track, like a drum circle.", "35-toy"),
             ("Echo", "A game: the pond sings a phrase, you sing it back. Each round adds a note.", "36-toy"), ("Seed", "A game: fly a dandelion seed on the night wind.", "37-toy"),
             ("Jelly", "A pulses a jellyfish upwards; it slowly sinks again.", "38-toy"), ("Starfall", "A game: stars fall in eight lanes; catch them in a lotus cup.", "39-toy")]
            if en else
            [("Bounce", "Bälle hüpfen in einer Kiste, und jeder Aufprall klingt.", "30-toy"), ("Orbits", "Bis zu fünf Planeten kreisen um eine Sonne; jeder singt am Tor, so entstehen Polyrhythmen.", "31-toy"),
             ("Rain", "Eine Wolke regnet auf die Seerosenblätter.", "32-toy"), ("Swing", "Acht Pendel driften zu Wellen auseinander und finden wieder zusammen.", "33-toy"),
             ("Firefly", "Steuere ein Glühwürmchen zu den leuchtenden Noten; jede gefangene kommt in eine kreisende Melodie.", "34-toy"), ("Wheel", "Vier Rhythmuskreise, jeder eine Spur, wie ein Trommelkreis.", "35-toy"),
             ("Echo", "Ein Spiel: Der Teich singt eine Phrase, du singst sie nach. Jede Runde kommt eine Note dazu.", "36-toy"), ("Seed", "Ein Spiel: Flieg mit einem Löwenzahnsamen im Nachtwind.", "37-toy"),
             ("Jelly", "A lässt eine Qualle aufsteigen; langsam sinkt sie wieder.", "38-toy"), ("Starfall", "Ein Spiel: Sterne fallen in acht Bahnen; fang sie mit einer Lotusschale.", "39-toy")])
    views = ([("20-mixer", "Mixer", "Level, pan and sends of the six tracks; each fader is a stem with a flower that opens."), ("21-atlas", "Biome atlas", "All 272 Biomes by family; Y loads a surprise."),
              ("22-sequence", "Sequence", "The steps of all tracks at a glance; X edits a track as a piano roll."), ("24-dreammap", "Dream map", "Travel through Biomes for hours; free flight over the islands."),
              ("25-modulation", "Modulation", "Two LFOs per track, living links, macros and the Biome drift."), ("26-voice", "Voice", "One track's whole sound in five pictured panels."),
              ("27-space", "Space", "Echo, grain memory as tape, reels, ducking and swing."), ("28-archive", "Archive", "Free voices to fetch over WiFi: poems from LibriVox.")]
             if en else
             [("20-mixer", "Mixer", "Pegel, Pan und Sends der sechs Spuren; jeder Fader ein Stängel mit einer Blume, die sich öffnet."), ("21-atlas", "Biom-Atlas", "Alle 272 Biome nach Familien; Y lädt eine Überraschung."),
              ("22-sequence", "Sequenz", "Die Schritte aller Spuren auf einen Blick; X bearbeitet eine Spur als Piano Roll."), ("24-dreammap", "Traumkarte", "Reise stundenlang durch Biome; freier Flug über die Inseln."),
              ("25-modulation", "Modulation", "Zwei LFOs je Spur, lebendige Bänder, Makros und die Biom-Drift."), ("26-voice", "Stimme", "Der ganze Klang einer Spur in fünf bebilderten Tafeln."),
              ("27-space", "Raum", "Echo, Körnerspeicher als Band, Spulen, Ducking und Swing."), ("28-archive", "Archiv", "Freie Stimmen zum Holen über WLAN: Gedichte von LibriVox.")])
    C = []
    C.append(("garden", "01", "The garden" if en else "Der Garten",
              ("The garden is your home. On the left stand the six tracks, each with its number, its sound model and a plant; in the middle the garden itself grows. A plant grows with its track's level, blooms on each note and sends a spark upwards. A muted track slowly falls asleep. Four dials stand on the right: UP/DOWN choose one, LEFT/RIGHT turn it."
               if en else "Der Garten ist dein Zuhause. Links stehen die sechs Spuren, jede mit Nummer, Klangmodell und einer Pflanze; in der Mitte wächst der Garten selbst. Eine Pflanze wächst mit dem Pegel ihrer Spur, blüht bei jeder Note und schickt einen Funken nach oben. Eine stummgeschaltete Spur schläft langsam ein. Rechts stehen vier Regler: UP/DOWN wählen einen, LEFT/RIGHT drehen ihn."),
              ["02-garden-playing"], dial_html + f'<p class="note">{"Start with MIST: a little more space makes almost any Biome more dreamlike. If the music feels too even, turn up WIND." if en else "Fang mit NEBEL an: etwas mehr Raum macht fast jedes Biom träumerischer. Wirkt die Musik zu gleichmäßig, dreh WIND auf."}</p>'
              + '<ul class="ticks">' + "".join(f"<li>{x}</li>" for x in (
                  ["Each note ripples the pond, and the fireflies light up on the beat.", f"FREEZE ({k('R2')} + {k('B')}) ices the pond over and lets it snow.", "In Biomes with an aurora, six ribbons flare with each note, rising higher for higher notes.",
                   "A frog jumps whenever RANDOM plays, and POLY's voices float as lanterns on the pond.", "The Biome drift slowly turns night into dawn, and the moon becomes the sun."] if en else
                  ["Jede Note kräuselt den Teich, und die Glühwürmchen leuchten im Takt.", f"EINFRIEREN ({k('R2')} + {k('B')}) überzieht den Teich mit Eis und lässt es schneien.", "In Biomen mit Polarlicht flammen sechs Bänder bei jeder Note auf, bei höheren Noten höher.",
                   "Ein Frosch springt, wann immer RANDOM spielt, und die Stimmen von POLY schweben als Laternen auf dem Teich.", "Die Biom-Drift verwandelt langsam Nacht in Morgenrot, und der Mond wird zur Sonne."]) ) + "</ul>"))
    C.append(("quick", "02", "Quick bar and menu" if en else "Schnellleiste und Menü",
              (f"A short tap on {k('L2')} opens the quick bar: every place lies there as a living stone on a vine. LEFT/RIGHT choose, A goes there, B closes. It always opens on the place you left last, so L2, A takes you back and forth between two places. At its end lie the switches PLAY MODE, LIGHT PICTURE, CALM MOTION and DEUTSCH / ENGLISH. The menu shows the places as stones along a garden path and the FM-1's pages as leaves; Quit is its last leaf."
               if en else f"Ein kurzer Tipp auf {k('L2')} öffnet die Schnellleiste: Jeder Ort liegt dort als lebendiger Stein an einer Ranke. LEFT/RIGHT wählen, A geht hin, B schließt. Sie öffnet sich immer am zuletzt verlassenen Ort, mit L2, A springst du also zwischen zwei Orten hin und her. An ihrem Ende liegen die Schalter SPIELMODUS, LEICHTES BILD, RUHIGE BEWEGUNG und DEUTSCH / ENGLISH. Das Menü zeigt die Orte als Steine an einem Gartenweg und die Seiten des FM-1 als Blätter; Beenden ist sein letztes Blatt."),
              ["04-quickbar", "05-menu"], ""))
    C.append(("play", "03", "Playing yourself: play mode" if en else "Selbst spielen: der Spielmodus",
              (f"Slide the FN switch, or choose PLAY MODE in the quick bar, and the buttons become notes. The D-pad and A B X Y play eight tones of the Biome's scale, so nothing can sound wrong. R2 adds a chord, L1/R1 choose the track. Eight stepping stones lie over the pond, one per tone, and light up as you play. If the POLY voice is on, you play it: eight voices with all 24 synthesis models of Plaits."
               if en else f"Schieb den FN-Schalter um oder wähle SPIELMODUS in der Schnellleiste, und die Tasten werden zu Tönen. Steuerkreuz und A B X Y spielen acht Töne der Tonleiter des Bioms, es kann also nichts falsch klingen. R2 fügt einen Akkord hinzu, L1/R1 wählen die Spur. Acht Trittsteine liegen über dem Teich, einer je Ton, und leuchten beim Spielen. Ist die Stimme POLY an, spielst du sie: acht Stimmen mit allen 24 Synthesemodellen von Plaits."),
              ["06-playmode"], ""))
    C.append(("lab", "04", "The Sound Lab" if en else "Das Klanglabor",
              (f"{k('F1')} opens the Sound Lab, where the sound itself can be bent, folded and set alight. Its modules stand in two rows, sources and effects, and each draws a live picture of what it is doing. With everything at neutral the sound is exactly that of Mutable Dreams 2.1; nothing can break, because A brings any setting back. X silences a module (the quickest way to hear what an effect adds), Y jumps to the next module that sounds, and L1+R1 together show all modules at once."
               if en else f"{k('F1')} öffnet das Klanglabor, in dem sich der Klang selbst biegen, falten und entzünden lässt. Seine Module stehen in zwei Reihen, Quellen und Effekte, und jedes zeichnet ein lebendiges Bild dessen, was es tut. Steht alles auf neutral, klingt es genau wie Mutable Dreams 2.1; nichts kann kaputtgehen, denn A holt jede Einstellung zurück. X macht ein Modul stumm (der schnellste Weg zu hören, was ein Effekt beiträgt), Y springt zum nächsten klingenden Modul, und L1+R1 zusammen zeigen alle Module auf einmal."),
              ["10-lab", "11-lab-03", "11-lab-07"],
              '<div class="chips">' + chips(modules, mod_cols) + "</div>" + '<ul class="ticks">' + "".join(f"<li>{x}</li>" for x in (
                  ["<b>TEXTURES</b>: beds of nature sound (forest, wind, rain, stardust) that start silent.", "<b>SYNTH</b>: twelve instruments, from just-intonation drones and Shepard tones to a glass harmonica, an aurora pad and a breathy wind flute.",
                   "<b>LOGIC</b>: music from musical mathematics: tintinnabuli, phasing, Fibonacci, cellular automata, Markov chains, L-systems.", "<b>GRIDS</b>: Mutable Instruments' topographic drum map, 25 patterns.",
                   "<b>CLOUD · PARTICLES · SEEDS</b>: grain clouds after Clouds and Beads.", "<b>MASTER</b>: a sound preset (CLEAN, GENTLE, WARM, LOUD, AIRY), EQ, saturation, compressor, width and limiter."] if en else
                  ["<b>TEXTURES</b>: Betten aus Naturklang (Wald, Wind, Regen, Sternenstaub), die stumm beginnen.", "<b>SYNTH</b>: zwölf Instrumente, von Drones in reiner Stimmung und Shepard-Tönen bis zu Glasharmonika, Polarlicht-Pad und hauchender Windflöte.",
                   "<b>LOGIC</b>: Musik aus musikalischer Mathematik: Tintinnabuli, Phasing, Fibonacci, zelluläre Automaten, Markov-Ketten, L-Systeme.", "<b>GRIDS</b>: die topografische Drum-Karte von Mutable Instruments, 25 Muster.",
                   "<b>CLOUD · PARTICLES · SEEDS</b>: Körnerwolken nach Clouds und Beads.", "<b>MASTER</b>: ein Klangvorsatz (CLEAN, GENTLE, WARM, LOUD, AIRY), EQ, Sättigung, Kompressor, Breite und Limiter."]) ) + "</ul>"))
    C.append(("views", "05", "The full views" if en else "Die Vollansichten",
              ("Beside the garden there are views that each show one thing in full. Reach them from the menu or the quick bar; each shows its buttons at the bottom." if en else
               "Neben dem Garten gibt es Ansichten, die je eine Sache ganz zeigen. Du erreichst sie über das Menü oder die Schnellleiste; jede zeigt unten ihre Tasten."),
              [], '<div class="gallery">' + "".join(f'<figure>{{IMG:{n}}}<figcaption><b>{t}</b> {d}</figcaption></figure>' for n, t, d in views) + "</div>"))
    C.append(("worlds", "06", "Worlds" if en else "Welten",
              ("A world is a whole scene: a Biome with its own tempo, dials, Sound Lab, SYNTH, LOGIC and a line of story. There are 96 worlds in 16 families. L1/R1 choose the family, UP/DOWN the world, A enters it, and Y wanders on to the family's next world every few minutes. The worlds are plain text in <b>worlds/worlds.txt</b>: change them or add your own. The Lullabies set the sleep timer themselves."
               if en else "Eine Welt ist eine ganze Szene: ein Biom mit eigenem Tempo, eigenen Reglern, Klanglabor, SYNTH, LOGIC und einer Zeile Geschichte. Es gibt 96 Welten in 16 Familien. L1/R1 wählen die Familie, UP/DOWN die Welt, A betritt sie, und Y wandert alle paar Minuten zur nächsten Welt der Familie. Die Welten sind einfacher Text in <b>worlds/worlds.txt</b>: Ändere sie oder füge eigene hinzu. Die Lullabies stellen den Schlaf-Timer selbst ein."),
              ["29-worlds"], '<div class="chips small">' + chips(families, ["#aecfb6", "#dcc694", "#86c2b4", "#a5bed9", "#c8b1d8", "#cca091"] * 3) + "</div>"
              + '<pre class="code">[MY FAMILY] Moon Garden\nbiome  = 89\nstory  = A song walks out and comes home again.\ntempo  = 60\nmacros = 30 25 40 70\nlab    = SHIMMER.MIX 30; TEXTURES.FOREST 20</pre>'))
    C.append(("toys", "07", "The playground" if en else "Die Spielwiese",
              ("Ten toys make music with you. They always play the chosen track in the Biome's scale, so they always fit. L1/R1 turn the carousel, X chooses the track, and R2 + X keeps what that track played in the last bar as its sequence, so a toy's music stays in the song."
               if en else "Zehn Spielzeuge machen mit dir Musik. Sie spielen immer die gewählte Spur in der Tonleiter des Bioms, es passt also immer. L1/R1 drehen das Karussell, X wählt die Spur, und R2 + X behält, was die Spur im letzten Takt gespielt hat, als ihre Sequenz, sodass die Musik eines Spielzeugs im Stück bleibt."),
              [], '<div class="gallery toys">' + "".join(f'<figure>{{IMG:{n}}}<figcaption><b>{t}</b> {d}</figcaption></figure>' for t, d, n in toys) + "</div>"))
    C.append(("tale", "08", "The dream tale" if en else "Die Traumgeschichte",
              ("While you play, a story in eight chapters is told. Each chapter has a few lines and a small deed that leads you somewhere new: the playground, the firefly, keeping a melody, the echo game, the planets, another Biome, the Sound Lab, all the balls at once. When a deed is done, its card rises over the stage. Progress is kept in <b>save/tale.txt</b>."
               if en else "Während du spielst, wird eine Geschichte in acht Kapiteln erzählt. Jedes Kapitel hat ein paar Zeilen und eine kleine Tat, die dich an einen neuen Ort führt: die Spielwiese, das Glühwürmchen, eine Melodie behalten, das Echo-Spiel, die Planeten, ein anderes Biom, das Klanglabor, alle Bälle auf einmal. Ist eine Tat getan, steigt ihre Karte über der Bühne auf. Der Fortschritt steht in <b>save/tale.txt</b>."),
              ["40-guide-01"], ""))
    C.append(("options", "09", "Options and settings" if en else "Optionen und Einstellungen",
              (f"The quick bar has an OPTIONS stone. You can swap A and B or X and Y, assign any part to any button, set how fast held buttons repeat, choose a sleep timer (15 to 90 minutes: the music fades out, the screen rests dark), switch language, light picture and calm motion. Mixed something up? Hold {k('START')} and {k('SELECT')} together for three seconds and every button is itself again. Plug in a USB sound card or a USB-C headphone adapter and the sound goes there; a USB keyboard plays POLY over MIDI. Switch files (<b>light.on</b>, <b>calm.on</b>, <b>nosplash.on</b> and more) are listed in the manual."
               if en else f"Die Schnellleiste hat einen Stein OPTIONEN. Du kannst A und B oder X und Y tauschen, jedem Teil eine beliebige Taste zuweisen, die Wiederholung gehaltener Tasten einstellen, einen Schlaf-Timer wählen (15 bis 90 Minuten: Die Musik blendet aus, der Bildschirm ruht dunkel), Sprache, leichtes Bild und ruhige Bewegung umschalten. Etwas durcheinandergebracht? {k('START')} und {k('SELECT')} drei Sekunden zusammen halten, und jede Taste ist wieder sie selbst. Steck eine USB-Soundkarte oder einen USB-C-Kopfhörer-Adapter an, und der Ton geht dorthin; ein USB-Keyboard spielt POLY über MIDI. Schalterdateien (<b>light.on</b>, <b>calm.on</b>, <b>nosplash.on</b> und mehr) stehen im Handbuch."),
              ["60-options"], ""))
    return C


def hero_loop(L, name, cap):
    r = T[L]["root"] + f"assets/img/loops/{L}/"
    return (f'<figure class="loop"><picture><source media="(prefers-reduced-motion: reduce)" srcset="{r}{name}-poster.webp">'
            f'<img src="{r}{name}-loop.webp" width="768" height="576" loading="lazy" alt="{html.escape(cap)}"></picture>'
            f'<figcaption>{cap}</figcaption></figure>')


def img(L, name, alt="", cls="", eager=False):
    r = T[L]["root"] + f"assets/img/{L}/{name}.webp"
    lazy = "" if eager else 'loading="lazy"'
    return (f'<a class="shot {cls}" href="{r}" target="_blank" rel="noopener"><img src="{r}" width="1024" height="768" alt="{html.escape(alt or name)}" {lazy}></a>')


CSS = """
@font-face { font-family: 'Nunito'; src: url('%(root)sassets/fonts/Nunito-Variable.ttf'); font-weight: 200 1000; font-display: swap; }
@font-face { font-family: 'Barlow'; src: url('%(root)sassets/fonts/BarlowSemiCondensed-Bold.ttf'); font-weight: 700; font-display: swap; }
@font-face { font-family: 'Barlow'; src: url('%(root)sassets/fonts/BarlowSemiCondensed-SemiBold.ttf'); font-weight: 600; font-display: swap; }
:root { --night:#04070e; --deep:#0a1220; --panel:#101b28; --ink:#ebf0e8; --dim:#a9b4bd; --faint:#6f7d88; --moss:#aecfb6; --sand:#dcc694;
        --teal:#86c2b4; --sky:#a5beda; --lilac:#c8b1d8; --rose:#cca091; --amber:#face8c; --leaf:#38664a; --w:1120px; }
* { box-sizing:border-box; } html { scroll-behavior:smooth; background:var(--night); scroll-padding-top:72px; }
body { margin:0; color:var(--ink); font:400 17px/1.65 'Nunito',system-ui,sans-serif;
       background:radial-gradient(1200px 700px at 12%% -8%%,#12233a 0%%,transparent 60%%),radial-gradient(900px 600px at 100%% 18%%,#1c1630 0%%,transparent 60%%),var(--night); }
img { max-width:100%%; height:auto; } a { color:var(--teal); }
h1,h2,h3 { font-family:'Nunito',sans-serif; font-weight:800; line-height:1.15; margin:0; }
.wrap { max-width:var(--w); margin:0 auto; padding:0 24px; }
header.top { position:sticky; top:0; z-index:20; background:#04070ecc; backdrop-filter:blur(10px); border-bottom:1px solid #ffffff12; }
header.top .wrap { display:flex; align-items:center; gap:18px; height:58px; }
.brand { font:700 18px 'Barlow'; letter-spacing:.14em; text-transform:uppercase; color:var(--ink); text-decoration:none; white-space:nowrap; display:flex; align-items:center; gap:10px; }
.brand svg { width:22px; height:22px; }
nav.main { display:flex; gap:4px; overflow-x:auto; scrollbar-width:none; flex:1; }
nav.main a { color:var(--dim); text-decoration:none; font:600 15px 'Barlow'; letter-spacing:.06em; text-transform:uppercase; padding:8px 11px; border-radius:999px; white-space:nowrap; }
nav.main a:hover { color:var(--ink); background:#ffffff0d; }
.lang { font:700 13px 'Barlow'; letter-spacing:.1em; text-transform:uppercase; text-decoration:none; color:#0e141c; background:var(--sand); padding:6px 12px; border-radius:999px; white-space:nowrap; }
.hero { position:relative; overflow:hidden; border-bottom:1px solid #ffffff10; }
.hero canvas { position:absolute; inset:0; width:100%%; height:100%%; display:block; opacity:.85; }
.hero::after { content:''; position:absolute; inset:0; background:linear-gradient(180deg,#04070e66 0%%,#04070eb8 55%%,var(--night) 100%%); }
.hero .wrap { position:relative; z-index:2; display:grid; grid-template-columns:1.05fr 1fr; gap:42px; align-items:center; padding-top:70px; padding-bottom:70px; }
.kicker { font:700 15px 'Barlow'; letter-spacing:.3em; color:var(--sand); text-transform:uppercase; }
.hero h1 { font-size:clamp(46px,7vw,84px); margin:10px 0 6px; letter-spacing:-.01em;
           background:linear-gradient(90deg,#fff,var(--moss) 40%%,var(--sky) 70%%,var(--lilac)); -webkit-background-clip:text; background-clip:text; color:transparent; }
.hero .sub { font-size:clamp(19px,2.3vw,24px); color:var(--dim); margin:6px 0 26px; max-width:30em; }
.cta { display:flex; flex-wrap:wrap; gap:12px; }
.btn { display:inline-block; font:700 16px 'Barlow'; letter-spacing:.08em; text-transform:uppercase; text-decoration:none; padding:13px 22px; border-radius:999px; border:1px solid #ffffff26; color:var(--ink); background:#ffffff0a; }
.btn.main { background:linear-gradient(135deg,var(--moss),#5c9a72); color:#0e141c; border:0; box-shadow:0 8px 28px #5c9a7255; }
.btn:hover { transform:translateY(-1px); filter:brightness(1.08); }
.meta { margin-top:18px; color:var(--faint); font:600 14px 'Barlow'; letter-spacing:.05em; }
.device { position:relative; border-radius:30px; padding:14px 14px 14px; background:linear-gradient(160deg,#1b2432,#0b111a); box-shadow:0 40px 90px #000c,0 0 0 1px #ffffff14,inset 0 1px 0 #ffffff1c; transform:rotate(1.2deg); }
.device .screen { border-radius:14px; overflow:hidden; background:#000; aspect-ratio:4/3; box-shadow:inset 0 0 0 1px #ffffff10; }
.device .screen img { width:100%%; height:100%%; object-fit:cover; display:block; }
.device .dots { position:absolute; right:18px; bottom:-26px; display:flex; gap:8px; }
.device .dots i { width:13px; height:13px; border-radius:50%%; display:block; opacity:.9; }
section { padding:78px 0 6px; }
h2 { font-size:clamp(28px,4vw,40px); position:relative; padding-top:20px; }
h2::before { content:''; position:absolute; top:0; left:0; width:120px; height:4px; border-radius:2px; background:linear-gradient(90deg,var(--moss),var(--sand),var(--teal),var(--sky),var(--lilac),var(--rose)); }
h2 .n { color:var(--amber); font-family:'Barlow'; font-weight:700; margin-right:10px; }
.lead { color:var(--dim); font-size:19px; max-width:46em; margin:10px 0 0; }
strong,b { color:#fff; } b { font-weight:700; }
.stats { display:grid; grid-template-columns:repeat(6,1fr); gap:12px; margin:34px 0 0; }
.stat { background:#ffffff08; border:1px solid #ffffff12; border-radius:18px; padding:16px 14px; text-align:center; }
.stat b { display:block; font:800 38px/1 'Nunito'; background:linear-gradient(135deg,var(--moss),var(--sky)); -webkit-background-clip:text; background-clip:text; color:transparent; }
.stat span { color:var(--dim); font:600 14px 'Barlow'; letter-spacing:.06em; text-transform:uppercase; }
.cards { display:grid; grid-template-columns:repeat(4,1fr); gap:16px; margin-top:34px; }
.card { background:#ffffff08; border:1px solid #ffffff12; border-radius:20px; overflow:hidden; }
.card .shot img { border-radius:0; box-shadow:none; }
.card div { padding:14px 18px 18px; } .card h3 { font-size:21px; color:var(--moss); } .card p { margin:6px 0 0; color:var(--dim); font-size:15.5px; line-height:1.5; }
.shot { display:block; } .shot img { width:100%%; display:block; border-radius:16px; box-shadow:0 18px 50px #000b,0 0 0 1px #ffffff12; transition:transform .25s; }
.shot:hover img { transform:scale(1.012); }
figure.sp { max-width:760px; margin:26px auto 0; } figure.sp figcaption { text-align:center; border-left:0; padding-left:0; }
.loops { display:grid; grid-template-columns:repeat(3,1fr); gap:16px; margin-top:30px; }
figure { margin:0; } figure.loop img { width:100%%; border-radius:16px; box-shadow:0 18px 50px #000b,0 0 0 1px #ffffff12; display:block; }
figcaption { color:var(--dim); font-size:14.5px; margin-top:9px; padding-left:13px; border-left:3px solid var(--leaf); line-height:1.45; }
.feature { display:grid; grid-template-columns:1fr 1.1fr; gap:40px; align-items:start; margin-top:54px; }
.feature.flip .text { order:2; } .feature .text h3 { font-size:30px; color:var(--moss); margin-bottom:8px; }
.feature .text h3 span { font:700 15px 'Barlow'; letter-spacing:.14em; color:var(--amber); display:block; margin-bottom:4px; }
.feature .text p { margin:8px 0 12px; } .feature .pics { display:grid; gap:14px; }
.feature .pics.two { grid-template-columns:1fr 1fr; } .feature.wide { grid-template-columns:1fr; }
.feature.wide .text { max-width:52em; }
.gallery { display:grid; grid-template-columns:repeat(4,1fr); gap:16px; margin-top:22px; } .gallery.toys { grid-template-columns:repeat(5,1fr); gap:14px; }
.gallery figcaption { font-size:13.5px; margin-top:7px; } .gallery figcaption b { color:var(--sand); font-family:'Barlow'; letter-spacing:.04em; display:block; font-size:15px; }
.dials { display:grid; grid-template-columns:1fr 1fr; gap:10px; margin:14px 0 6px; }
.dial { background:#ffffff08; border:1px solid #ffffff12; border-left:4px solid var(--c); border-radius:14px; padding:10px 14px; }
.dial b { color:var(--c); font:700 16px 'Barlow'; letter-spacing:.1em; display:block; } .dial span { display:block; font-size:14.5px; color:var(--ink); line-height:1.4; } .dial em { font-size:13.5px; color:var(--faint); font-style:normal; }
.chips { display:flex; flex-wrap:wrap; gap:8px; margin:14px 0; } .chip { font:700 14px 'Barlow'; letter-spacing:.08em; color:var(--c); border:1px solid color-mix(in srgb,var(--c) 45%%,transparent); background:color-mix(in srgb,var(--c) 10%%,transparent); padding:5px 12px; border-radius:999px; }
.chips.small .chip { font-size:12.5px; padding:4px 10px; letter-spacing:.04em; }
ul.ticks { list-style:none; padding:0; margin:12px 0; } ul.ticks li { position:relative; padding-left:26px; margin:7px 0; }
ul.ticks li::before { content:''; position:absolute; left:2px; top:8px; width:12px; height:12px; border-radius:0 60%% 0 60%%; background:linear-gradient(135deg,var(--moss),#5c9a72); transform:rotate(-10deg); }
.note { color:var(--dim); font-size:15.5px; border-left:3px solid var(--leaf); padding-left:12px; margin:12px 0; }
pre.code { background:#060b13; border:1px solid #ffffff14; border-radius:14px; padding:14px 18px; font:500 14.5px/1.55 ui-monospace,Menlo,monospace; color:var(--moss); overflow-x:auto; margin:12px 0; }
kbd { display:inline-block; font:700 14px/1 'Barlow'; letter-spacing:.02em; color:#0e141c; background:#e8ecde; border-radius:999px; padding:4px 10px 3px; margin:0 2px; vertical-align:1px; box-shadow:0 2px 0 #00000055; }
.tip { position:relative; background:linear-gradient(135deg,#1b3326,#12231c); border:1px solid #3f6e52; border-radius:18px 18px 18px 4px; padding:16px 20px 16px 62px; margin:22px 0; }
.tip::before { content:''; position:absolute; left:20px; top:18px; width:26px; height:26px; border-radius:0 60%% 0 60%%; background:linear-gradient(135deg,var(--moss),#5c9a72); transform:rotate(-10deg); }
.tip h4 { margin:0 0 3px; font:700 15px 'Barlow'; letter-spacing:.1em; text-transform:uppercase; color:var(--moss); }
table { width:100%%; border-collapse:separate; border-spacing:0; background:#ffffff06; border:1px solid #ffffff12; border-radius:16px; overflow:hidden; font-size:15.5px; margin:16px 0; }
th { text-align:left; font:700 13px 'Barlow'; letter-spacing:.08em; text-transform:uppercase; color:var(--sand); background:#ffffff0a; padding:10px 14px; }
td { padding:9px 14px; border-top:1px solid #ffffff0d; vertical-align:top; } td:first-child { white-space:nowrap; font:700 14.5px 'Barlow'; color:var(--ink); letter-spacing:.04em; }
.controls { display:grid; grid-template-columns:1.15fr 1fr; gap:36px; align-items:start; margin-top:28px; }
.touch { margin:18px 0; background:#ffffff06; border:1px solid #ffffff12; border-radius:20px; padding:12px; } .touch svg { width:100%%; height:auto; display:block; }
.devices { display:grid; grid-template-columns:repeat(4,1fr); gap:16px; margin-top:30px; }
.dev { background:#ffffff08; border:1px solid #ffffff12; border-radius:20px; padding:18px; display:flex; flex-direction:column; gap:6px; position:relative; overflow:hidden; }
.dev::before { content:''; position:absolute; left:0; top:0; right:0; height:3px; background:var(--c); opacity:.9; }
.dev h3 { font-size:20px; color:var(--ink); } .dev .sys { font:600 13.5px 'Barlow'; letter-spacing:.07em; color:var(--c); text-transform:uppercase; }
.dev p { margin:4px 0 8px; color:var(--dim); font-size:15px; line-height:1.5; flex:1; }
.badge { align-self:flex-start; font:700 12.5px 'Barlow'; letter-spacing:.08em; text-transform:uppercase; padding:4px 11px; border-radius:999px; }
.badge.ok { color:#0e141c; background:var(--moss); } .badge.test { color:#0e141c; background:var(--amber); } .badge.new { color:#0e141c; background:var(--sky); }
.recipes { display:grid; grid-template-columns:repeat(2,1fr); gap:18px; margin-top:30px; }
.recipe { background:#ffffff08; border:1px solid #ffffff12; border-radius:22px; overflow:hidden; display:grid; grid-template-columns:.9fr 1fr; }
.recipe .shot img { border-radius:0; box-shadow:none; height:100%%; object-fit:cover; } .recipe div.body { padding:16px 18px; } .recipe h3 { font-size:21px; color:var(--moss); margin-bottom:4px; }
.recipe ol { margin:6px 0 0; padding-left:20px; font-size:15px; line-height:1.5; color:var(--dim); } .recipe ol li { margin:5px 0; }
details { background:#ffffff08; border:1px solid #ffffff12; border-radius:16px; margin:10px 0; }
summary { cursor:pointer; list-style:none; padding:14px 20px; font:800 18px 'Nunito'; color:var(--ink); display:flex; justify-content:space-between; gap:14px; }
summary::-webkit-details-marker { display:none; } summary::after { content:'+'; color:var(--amber); font:700 22px 'Barlow'; } details[open] summary::after { content:'–'; }
details > div { padding:0 20px 16px; color:var(--dim); } details b { color:var(--sand); font-family:'Barlow'; letter-spacing:.03em; }
.dl td:last-child a { font-weight:700; word-break:break-all; } .dl td span { color:var(--faint); font-size:13.5px; }
footer { margin-top:84px; border-top:1px solid #ffffff14; padding:28px 0 60px; color:var(--faint); font-size:14.5px; }
footer .links { display:flex; gap:18px; flex-wrap:wrap; margin-top:10px; } footer ul { margin:8px 0; padding-left:20px; }
.reveal { opacity:0; transform:translateY(18px); transition:opacity .7s ease,transform .7s ease; } .reveal.in { opacity:1; transform:none; }
@media (prefers-reduced-motion:reduce) { .reveal { opacity:1; transform:none; transition:none; } html { scroll-behavior:auto; } .device { transform:none; } }
@media (max-width:980px) { .hero .wrap,.feature,.controls { grid-template-columns:1fr; } .feature.flip .text { order:0; } .cards,.devices { grid-template-columns:1fr 1fr; }
  .stats { grid-template-columns:repeat(3,1fr); } .gallery,.gallery.toys { grid-template-columns:repeat(2,1fr); } .loops { grid-template-columns:1fr; } .recipes { grid-template-columns:1fr; } }
@media (max-width:560px) { body { font-size:16px; } .wrap { padding:0 16px; } .cards,.devices,.dials { grid-template-columns:1fr; } .recipe { grid-template-columns:1fr; }
  .gallery.toys { grid-template-columns:1fr 1fr; } .brand span { display:none; } .stat b { font-size:30px; } .device { transform:none; } }
/* Keep the Worlds example and download filenames readable on narrow screens. */
.feature > * { min-width:0; }
.code { max-width:100%%; overflow-x:auto; }
@media (max-width:560px) {
  .stats { grid-template-columns:repeat(2,minmax(0,1fr)); }
  .stat { min-width:0; }
  .stat span { display:block; overflow-wrap:anywhere; }
  .controls > * { min-width:0; }
  .controls table { table-layout:fixed; }
  .controls th, .controls td { padding:8px 7px; overflow-wrap:anywhere; }
  .controls td:first-child { white-space:normal; }
  .controls kbd { padding-left:6px; padding-right:6px; max-width:100%%; white-space:normal; }
  #download table { table-layout:fixed; width:100%%; }
  #download th:first-child, #download td:first-child { width:38%%; }
  #download th:nth-child(2), #download td:nth-child(2) { width:62%%; }
  #download td, #download a { overflow-wrap:anywhere; }
}

"""

TOUCH_SVG = """<svg viewBox="0 0 800 360" role="img" aria-label="%(alt)s"><defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#12233a"/><stop offset="1" stop-color="#0a1220"/></linearGradient></defs>
<rect x="2" y="2" width="796" height="356" rx="34" fill="#070c14" stroke="#ffffff22" stroke-width="2"/>
<rect x="186" y="26" width="428" height="308" rx="12" fill="url(#g)" stroke="#ffffff1c"/><text x="400" y="190" text-anchor="middle" fill="#aecfb6" font-family="Nunito,sans-serif" font-weight="800" font-size="26">MUTABLE DREAMS</text>
<g fill="#ffffff10" stroke="#a5beda88" stroke-width="2"><circle cx="96" cy="190" r="62"/></g><g fill="#a5bed9" opacity=".8"><path d="M96 142l14 22H82z"/><path d="M96 238l14-22H82z"/><path d="M48 190l22-14v28z"/><path d="M144 190l-22-14v28z"/></g>
<g font-family="Barlow,sans-serif" font-weight="700" font-size="17" text-anchor="middle" fill="#0e141c">
<rect x="22" y="30" width="62" height="34" rx="17" fill="#96aaff"/><text x="53" y="53">L2</text><rect x="94" y="30" width="62" height="34" rx="17" fill="#96aaff"/><text x="125" y="53">L1</text>
<rect x="640" y="30" width="62" height="34" rx="17" fill="#96aaff"/><text x="671" y="53">R1</text><rect x="712" y="30" width="62" height="34" rx="17" fill="#96aaff"/><text x="743" y="53">R2</text>
<rect x="40" y="300" width="112" height="34" rx="17" fill="#bebed2"/><text x="96" y="323">SELECT</text><rect x="628" y="300" width="68" height="34" rx="17" fill="#bebed2"/><text x="662" y="323">START</text><rect x="704" y="300" width="72" height="34" rx="17" fill="#ffbef0"/><text x="740" y="323">MENU</text>
<circle cx="740" cy="140" r="30" fill="#6ee6be"/><text x="740" y="146" font-size="22">A</text><circle cx="700" cy="196" r="30" fill="#ff8c82"/><text x="700" y="202" font-size="22">B</text>
<circle cx="660" cy="140" r="30" fill="#ffd26e"/><text x="660" y="146" font-size="22">Y</text></g>
<circle cx="700" cy="84" r="30" fill="#78bfff"/><text x="700" y="90" text-anchor="middle" font-family="Barlow,sans-serif" font-weight="700" font-size="22" fill="#0e141c">X</text></svg>"""

SPROUT = ('<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 22V11" stroke="#aecfb6" stroke-width="2" fill="none" stroke-linecap="round"/>'
          '<path d="M12 12C12 7 8 5 3 5c0 5 3 8 9 7z" fill="#86c2b4"/><path d="M12 10c0-4 3-6 8-6 0 4-3 7-8 6z" fill="#aecfb6"/></svg>')


def build(L):
    t = T[L]
    r = t["root"]
    out = []
    A = out.append
    ogimg = SITE + "assets/img/og.jpg"
    A(f'<!doctype html><html lang="{t["lang"]}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">')
    A(f'<title>{html.escape(t["title"])}</title><meta name="description" content="{html.escape(t["desc"])}">')
    A(f'<meta property="og:title" content="{html.escape(t["title"])}"><meta property="og:description" content="{html.escape(t["desc"])}"><meta property="og:type" content="website">')
    A(f'<meta property="og:image" content="{ogimg}"><meta property="og:url" content="{SITE}{t["dir"]}"><meta name="twitter:card" content="summary_large_image">')
    A(f'<link rel="alternate" hreflang="en" href="{SITE}"><link rel="alternate" hreflang="de" href="{SITE}de/"><link rel="canonical" href="{SITE}{t["dir"]}">')
    A('<link rel="icon" href="data:image/svg+xml,' + SPROUT.replace('"', "'").replace("#", "%23").replace("<", "%3C").replace(">", "%3E") + '">')
    A(f'<meta name="theme-color" content="#04070e"><style>{CSS % {"root": r}}</style></head><body>')
    A(f'<header class="top"><div class="wrap"><a class="brand" href="#top">{SPROUT}<span>Mutable Dreams</span></a><nav class="main">')
    A("".join(f'<a href="#{a}">{n}</a>' for a, n in t["nav"]))
    A(f'</nav><a class="lang" href="{t["other"][0]}" hreflang="{"de" if L == "en" else "en"}">{t["other"][1]}</a></div></header>')
    # hero
    A('<div class="hero" id="top"><canvas id="kit" aria-hidden="true"></canvas><div class="wrap"><div>')
    A(f'<div class="kicker">{t["kicker"]}</div><h1>Mutable Dreams</h1><p class="sub">{t["sub"]}</p><div class="cta">')
    A(f'<a class="btn main" href="#download">{t["cta1"]}</a><a class="btn" href="{rel("Mutable-Dreams-" + ("Manual-EN" if L == "en" else "Handbuch-DE") + ".pdf")}">{t["cta2"]}</a>'
      f'<a class="btn" href="https://github.com/{REPO}">{t["cta3"]}</a></div><p class="meta">{t["meta"]}</p></div>')
    A(f'<div class="device"><div class="screen"><picture><source media="(prefers-reduced-motion: reduce)" srcset="{r}assets/img/loops/{L}/garden-poster.webp">'
      f'<img src="{r}assets/img/loops/{L}/garden-loop.webp" width="768" height="576" alt="Mutable Dreams: the garden" fetchpriority="high"></picture></div>'
      f'<div class="dots"><i style="background:#aecfb6"></i><i style="background:#dcc694"></i><i style="background:#86c2b4"></i><i style="background:#a5bed9"></i><i style="background:#c8b1d8"></i><i style="background:#cca091"></i></div></div></div></div>')
    # Published feature film, with separate EN/DE web captions.
    tour_h = "A garden in your hands · the full film" if L == "en" else "Ein Garten in deinen Händen · der ganze Film"
    tour_p = "6:40 of the garden, Sound Lab, played melodies, SELECT menu and Android touch layout." if L == "en" else "6:40 mit Garten, Klanglabor, gespielten Melodien, SELECT-Menü und Android-Touch-Ansicht."
    tour_note = "Actual macOS app performance and original engine music. Synthetic English narration; English and German subtitles. The touch layout is a labelled still from the Mac preview." if L == "en" else "Echte App-Performance auf dem Mac mit Originalmusik. Englische synthetische Stimme; deutsche und englische Untertitel. Die Touch-Ansicht ist ein beschriftetes Standbild aus der Mac-Vorschau."
    tour_file = REL_OLD + "Mutable-Dreams-1.5.9-Feature-Tour.mp4"
    cover_file = REL_OLD + "Mutable-Dreams-1.5.9-Feature-Tour-Cover.jpg"
    en_default = " default" if L == "en" else ""
    de_default = " default" if L == "de" else ""
    A(f'<main><section id="tour"><div class="wrap reveal"><h2>{tour_h}</h2><p class="lead">{tour_p}</p>')
    A(f'<video controls playsinline preload="none" poster="{cover_file}" style="display:block;width:100%;aspect-ratio:16/9;background:#071018;border-radius:18px"><source src="{tour_file}" type="video/mp4">')
    A(f'<track kind="subtitles" src="{r}assets/video/feature-tour-en.vtt" srclang="en" label="English"{en_default}><track kind="subtitles" src="{r}assets/video/feature-tour-de.vtt" srclang="de" label="Deutsch"{de_default}>')
    A(f'<a href="{tour_file}">{tour_h}</a></video><p>{tour_note}</p>')
    A(f'<p><a href="https://youtu.be/dMS1GqUwWYo">{"Watch on YouTube" if L == "en" else "Auf YouTube ansehen"}</a> · <a href="https://youtube.com/shorts/AWsBLmDVJhY">YouTube Short</a> · <a href="{tour_file}">{"Download the full film" if L == "en" else "Film herunterladen"}</a> · <a href="{REL_OLD}Mutable-Dreams-1.5.9-Release-Reel.mp4">{"60-second reel" if L == "en" else "60-Sekunden-Reel"}</a></p></div></section>')
    # pillars
    A(f'<section id="garden"><div class="wrap reveal"><h2>{t["pillars_h"]}</h2><p class="lead">{t["pillars_p"]}</p>')
    A('<div class="stats">' + "".join(f'<div class="stat"><b>{a}</b><span>{b}</span></div>' for a, b in t["stats"]) + "</div>")
    A('<div class="cards">' + "".join(f'<div class="card">{img(L, im, ti)}<div><h3>{ti}</h3><p>{tx}</p></div></div>' for ti, tx, im in t["pillars"]) + "</div></div></section>")
    # loops
    A(f'<section><div class="wrap reveal"><h2>{t["loops_h"]}</h2><p class="lead">{t["loops_p"]}</p><div class="loops">')
    A("".join(hero_loop(L, n, c) for n, c in t["loops"]) + "</div></div></section>")
    # features
    A(f'<section id="features"><div class="wrap"><div class="reveal"><h2>{t["feat_h"]}</h2><p class="lead">{t["feat_p"]}</p></div>')
    for i, (aid, num, title, text, imgs, extra) in enumerate(tour(L)):
        cls = "feature reveal" + (" flip" if i % 2 else "") + (" wide" if not imgs else "")
        extra = re.sub(r"\{IMG:([^}]+)\}", lambda m: img(L, m.group(1)), extra)
        A(f'<div class="{cls}" id="{aid}"><div class="text"><h3><span>{num}</span>{title}</h3><p>{text}</p>{extra}</div>')
        if imgs:
            A(f'<div class="pics{" two" if len(imgs) == 2 else ""}">' + "".join(img(L, n, title) for n in imgs) + "</div>")
        A("</div>")
    A("</div></section>")
    # devices
    A(f'<section id="devices"><div class="wrap reveal"><h2>{t["devices_h"]}</h2><p class="lead">{t["devices_p"]}</p><div class="devices">')
    cols = {"SmartPro": "#8fd3ff", "Brick": "#aecfb6", "Plus": "#dcc694", "Mini": "#86c2b4", "RGB20S": "#a5bed9", "H700": "#face8c", "Android": "#c8b1d8", "Linux": "#cca091"}
    for name, sysname, key, st, txt in t["dev"]:
        A(f'<div class="dev" style="--c:{cols[key]}"><h3>{name}</h3><div class="sys">{sysname}</div><p>{txt}</p><span class="badge {st}">{t["badge"][st]}</span></div>')
    A("</div>")
    A(f'<figure class="sp"><a class="shot" href="{r}assets/img/smartpro.webp" target="_blank" rel="noopener"><img src="{r}assets/img/smartpro.webp" width="1280" height="720" loading="lazy" alt="{t["sp_cap"]}"></a><figcaption>{t["sp_cap"]}</figcaption></figure>')
    A("</div></section>")
    # controls
    A(f'<section id="controls"><div class="wrap reveal"><h2>{t["controls_h"]}</h2><p class="lead">{t["controls_p"]}</p><ul class="ticks">' + "".join(f"<li>{x}</li>" for x in t["rules"]) + "</ul>")
    A(f'<div class="controls"><div><figure>{img(L, "50-help", t["controls_h"])}<figcaption>{t["map_cap"]}</figcaption></figure></div><div>')
    A("<table><tr>" + "".join(f"<th>{h}</th>" for h in t["table_h"]) + "</tr>" + "".join(f"<tr><td>{a}</td><td>{b}</td><td>{c}</td></tr>" for a, b, c in t["table"]) + "</table></div></div>")
    A(f'<div class="tip"><h4>{t["hand_h"]}</h4><p style="margin:4px 0 0">{t["hand_p"]}</p></div>')
    A(f'<h3 style="color:var(--moss);font-size:24px;margin-top:30px">{t["sp_h"]}</h3><p>{t["sp_p"]}</p>')
    A(f'<h3 style="color:var(--moss);font-size:24px;margin-top:30px">{t["touch_h"]}</h3><div class="touch">{TOUCH_SVG % {"alt": t["touch_h"]}}</div><p>{t["touch_p"]}</p></div></section>')
    # ideas
    A(f'<section id="ideas"><div class="wrap reveal"><h2>{t["ideas_h"]}</h2><p class="lead">{t["ideas_p"]}</p><div class="recipes">')
    for title, im, steps in t["ideas"]:
        A(f'<div class="recipe">{img(L, im, title)}<div class="body"><h3>{title}</h3><ol>' + "".join(f"<li>{s}</li>" for s in steps) + "</ol></div></div>")
    A("</div></div></section>")
    # download
    files = [("Trimui Brick (Knulli)", f"Mutable-Dreams-Brick-{OLD}.zip"), ("Trimui Smart Pro (Knulli) · " + t["badge"]["test"], f"Mutable-Dreams-Trimui-Smart-Pro-Knulli-{VERSION}.zip"),
             ("Miyoo Mini Plus (Onion OS)", f"Mutable-Dreams-Miyoo-Mini-Plus-Onion-{OLD}.zip"),
             ("Miyoo Mini · Mini Flip (Onion OS)", f"Mutable-Dreams-Miyoo-Mini-Onion-{OLD}.zip"), ("Powkiddy RGB20S (AmberELEC)", f"Mutable-Dreams-RGB20S-AmberELEC-{OLD}.zip"),
             ("Anbernic H700 (Knulli) · " + t["badge"]["test"], f"Mutable-Dreams-Anbernic-H700-Knulli-{VERSION}.zip"), ("Android (APK)", f"Mutable-Dreams-Android-{OLD}.apk"),
             ("Linux PC x86_64", f"Mutable-Dreams-Linux-OS-x86_64-{OLD}.zip"), ("Linux aarch64", f"Mutable-Dreams-Linux-OS-aarch64-{OLD}.zip"),
             (t["manual"] + " (English)", "Mutable-Dreams-Manual-EN.pdf"), (t["manual"] + " (Deutsch)", "Mutable-Dreams-Handbuch-DE.pdf"),
             (t["src"] + " (GPL-3.0)", f"Mutable-Dreams-Handheld-Source-{VERSION}.zip"), ("SHA-256", "SHA256SUMS.txt")]
    A(f'<section id="download"><div class="wrap reveal"><h2>{t["dl_h"]}</h2><p class="lead">{t["dl_p"]}</p><table class="dl"><tr><th>{t["dl_cols"][0]}</th><th>{t["dl_cols"][1]}</th></tr>')
    A("".join(f'<tr><td>{a}</td><td><a href="{rel(f)}">{f}</a> <span>{mb(f).strip(" ·")}</span></td></tr>' for a, f in files))
    A(f'</table><p><a class="btn" href="https://github.com/{REPO}/releases/tag/v{VERSION}">{t["dl_all"]}</a></p>')
    A(f'<h3 style="color:var(--moss);font-size:26px;margin:34px 0 4px">{t["inst_h"]}</h3><p class="lead" style="margin-top:0">{t["inst_p"]}</p>')
    A("".join(f"<details><summary>{a}</summary><div>{b}</div></details>" for a, b in t["inst"]) + "</div></section>")
    # faq
    A(f'<section id="faq"><div class="wrap reveal"><h2>{t["faq_h"]}</h2>' + "".join(f"<details><summary>{a}</summary><div>{b}</div></details>" for a, b in t["faq"]) + "</div></section></main>")
    # footer
    A(f'<footer><div class="wrap"><h3 style="font-size:20px;color:var(--moss)">{t["credits_h"]}</h3><p>{t["credits"]}</p><ul>' + "".join(f"<li>{x}</li>" for x in t["credits_li"]) + "</ul>")
    A(f'<p>{t["foot"]}</p><div class="links"><a href="https://github.com/{REPO}">{t["src"]}</a><a href="https://github.com/{REPO}/releases/tag/v{VERSION}">{t["release"]}</a>'
      f'<a href="{FM1}">Mutable Dreams FM-1</a><a href="#top">{t["top"]}</a></div></div></footer>')
    A("""<script>
(function(){var rm=window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches;
var els=[].slice.call(document.querySelectorAll('.reveal'));
if(rm||!('IntersectionObserver'in window)){els.forEach(function(e){e.classList.add('in')})}
else{var io=new IntersectionObserver(function(es){es.forEach(function(x){if(x.isIntersecting){x.target.classList.add('in');io.unobserve(x.target)}})},{rootMargin:'0px 0px -8% 0px'});els.forEach(function(e){io.observe(e)})}
if(!rm){var s=document.createElement('script');s.src='""" + r + """assets/js/kit.js';s.onload=function(){try{MDKit.start(document.getElementById('kit'),{scene:'garden',lower:'0',bpm:72,biome:21})}catch(e){}};document.head.appendChild(s)}})();
</script></body></html>""")
    return "".join(out)


if __name__ == "__main__":
    (DOCS / "de").mkdir(parents=True, exist_ok=True)
    (DOCS / "index.html").write_text(build("en"), encoding="utf-8")
    (DOCS / "de/index.html").write_text(build("de"), encoding="utf-8")
    (DOCS / ".nojekyll").write_text("")
    print("docs/index.html", (DOCS / "index.html").stat().st_size // 1024, "KB;", "docs/de/index.html", (DOCS / "de/index.html").stat().st_size // 1024, "KB")
