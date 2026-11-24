#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/promo.py OUTDIR [wide] [short]: the "coming soon" promo films of Mutable Dreams for Trimui Brick (1.5).

The program plays itself from a storyboard: brick-headless runs it with real button presses and its own music,
every frame at 30 a second goes through a pipe into tools/film.swift (the stage large, the Brick's buttons lighting
up as pressed, the scene titles), the run's music goes in as AAC. wide: about 3:30 at 1920 x 1080; short: under a
minute at 1080 x 1920. Beside each film: its music as WAV, the titles (TSV), and the voice-over script with the
scenes' times (DE and EN, for Codex to voice)."""
import os, shutil, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
OUT = Path(sys.argv[1]).resolve()
WANT = sys.argv[2:] or ["wide", "short"]
FONTS = HERE.parent / "Handbuch/fonts"


class Board:
    """the storyboard: the runner's lines and the time they take (tap 140 ms, waits as written)"""
    def __init__(self):
        self.lines, self.t, self.titles, self.voice, self.menu_at = [], 0.0, [], [], 12   # (the pre-roll left it on 12)
    def do(self, line, ms=0):
        self.lines.append(line)
        self.t += ms / 1000.0
    def wait(self, ms):
        self.do(f"wait {ms}", ms)
    def tap(self, b, n=1, gap=0):
        for _ in range(n):
            self.do(f"tap {b}", 140)
            if gap:
                self.wait(gap)
    def hold(self, b, ms):
        self.do(f"press {b}")
        self.wait(ms)
        self.do(f"release {b}")
    def menu(self, i):                           # the jump menu keeps its choice between visits
        self.tap("MENU"); self.wait(350)
        d = i - self.menu_at
        self.tap("DOWN" if d > 0 else "UP", abs(d), 60)
        self.menu_at = i
        self.tap("A"); self.wait(700)
    def home(self):
        self.hold("B", 900); self.wait(400)
    def title(self, length, title, line="", de="", en=""):
        self.titles.append((self.t, self.t + length, title, line))
        if de or en:
            self.voice.append((self.t, de, en))


def wide_board():
    b = Board()
    b.do("splash")
    b.title(5.5, "@card", "Coming soon|A spin-off of Mutable Dreams for the FM-1",
            de="Mutable Dreams. Bald auf dem Trimui Brick. Der Ableger des Mutable Dreams für das FM-1.",
            en="Mutable Dreams. Coming soon to the Trimui Brick. A spin-off of Mutable Dreams for the FM-1.")
    b.wait(5500)
    b.title(4.5, "The dream garden wakes", "Generative music in your pocket")
    b.wait(4500)
    b.wait(1500)
    b.title(10, "A garden of sound", "Six voices grow into music by themselves",
            de="Ein Garten aus Klang. Sechs Stimmen wachsen von selbst zu Musik, du musst nur zuhören.",
            en="A garden of sound. Six voices grow into music by themselves. All you do is listen.")
    b.wait(8500)
    b.title(11, "Four dials shape the dream", "Fireflies, wind, glow and mist",
            de="Vier Regler formen den Traum: Glühwürmchen, Wind, Glühen und Nebel.",
            en="Four dials shape the dream: fireflies, wind, glow and mist.")
    b.tap("DOWN"); b.hold("RIGHT", 1400); b.wait(1200)
    b.tap("DOWN"); b.hold("RIGHT", 1000); b.wait(1500)
    b.tap("UP", 2, 300); b.wait(4000)
    b.title(11, "Six voices, six plants", "Each one blooms on its notes",
            de="Jede Stimme ist eine Pflanze. Sie blüht auf jedem Ton. Mit L1 und R1 wählst du sie, mit A lässt du sie schweigen.",
            en="Every voice is a plant that blooms on its notes. L1 and R1 choose one, A lets it rest.")
    b.tap("R1", 2, 700); b.tap("A"); b.wait(2500); b.tap("A"); b.wait(1500); b.tap("L1", 2, 600); b.wait(2500)
    b.title(13, "272 worlds of sound", "R2 and the D-pad travel between Biomes",
            de="Zweihundertzweiundsiebzig Klangwelten, die Biome. Mit R2 und dem Steuerkreuz reist du weiter, und der Garten wächst neu.",
            en="Two hundred and seventy-two worlds of sound. R2 and the D-pad travel on, and the garden grows anew.")
    b.do("press R2"); b.wait(500); b.tap("RIGHT"); b.wait(400); b.do("release R2"); b.wait(1800)
    b.tap("A"); b.wait(9500)                                                  # (the question: SEL - the A button - discard and load)
    b.tap("F1"); b.wait(600)
    b.title(9, "The Sound Lab", "Plaits, Rings, Elements, Clouds, Beads, Warps, Marbles, Grids",
            de="Das Klanglabor. Hier steckt das Herz: die offenen Module von Mutable Instruments, Plaits, Rings, Clouds, Beads und mehr.",
            en="The Sound Lab. Its heart: Mutable Instruments' open modules. Plaits, Rings, Clouds, Beads and more.")
    b.tap("DOWN", 2, 500); b.hold("RIGHT", 1200); b.wait(1200); b.tap("UP", 2, 300); b.wait(3000)
    b.title(8, "Every module at a glance", "L1 and R1 together: choose, and you are there",
            de="L1 und R1 zusammen zeigen alle Module auf einen Blick.",
            en="L1 and R1 together show every module at a glance.")
    b.do("press L1"); b.wait(120); b.do("press R1"); b.wait(200); b.do("release R1"); b.do("release L1"); b.wait(1800)
    b.tap("RIGHT", 4, 450); b.wait(1500); b.tap("A"); b.wait(1500)          # SYNTH
    b.title(26, "Twelve instruments", "Glass harmonica, aurora pad, CZ crystal, wind flute ...",
            de="Zwölf Instrumente: eine Glasharmonika, ein Polarlicht-Pad, CZ-Kristalle, eine Windflöte und mehr. Jedes mit seinem eigenen Bild.",
            en="Twelve instruments: a glass harmonica, an aurora pad, CZ crystals, a wind flute and more. Each with a picture of its own.")
    b.hold("RIGHT", 1500); b.do("lab SYNTH.LEVEL 65"); b.wait(600)            # LEVEL up
    b.tap("DOWN"); b.wait(500)
    b.tap("RIGHT", 8, 120); b.wait(5000)                                      # GLASS HARMONICA
    b.tap("RIGHT"); b.wait(5000)                                              # AURORA PAD
    b.tap("RIGHT"); b.wait(4500)                                              # CZ CRYSTAL
    b.tap("RIGHT"); b.wait(4500)                                              # WIND FLUTE
    b.tap("LEFT", 3, 200); b.wait(1500)                                       # back to GLASS
    b.title(8, "Hear what it adds", "X: silent and back. Y: the next one sounding",
            de="X schaltet ein Modul stumm und wieder an. So hörst du genau, was es dazugibt.",
            en="X silences a module and brings it back, so you hear exactly what it adds.")
    b.tap("X"); b.wait(2500); b.tap("X"); b.wait(4500)
    b.do("press L1"); b.wait(120); b.do("press R1"); b.wait(200); b.do("release R1"); b.do("release L1"); b.wait(900)
    b.tap("DOWN", 2, 400); b.wait(400); b.tap("A"); b.wait(800)              # SYNTH > GRIDS > CLOUD
    b.title(9, "Effects", "Clouds, shimmer, tape, a whole mastering chain",
            de="Dazu Effekte: Wolken aus Klangkörnern, Schimmer-Hall, Band, eine ganze Mastering-Kette.",
            en="And effects: clouds of grains, shimmering reverb, tape, a whole mastering chain.")
    b.hold("RIGHT", 1300); b.do("lab CLOUD.MIX 35"); b.wait(7500)
    b.tap("F1"); b.wait(400); b.tap("F2"); b.wait(600)
    b.title(9, "The mixer", "Levels, pan and sends, as flowers",
            de="Der Mixer: Lautstärke, Panorama und Effektwege, gezeichnet als Blumen.",
            en="The mixer: levels, panning and sends, drawn as flowers.")
    b.tap("RIGHT", 2, 500); b.hold("UP", 900); b.wait(5000)
    b.tap("F2"); b.wait(400)
    b.menu(10)
    b.title(14, "128 worlds", "Whole scenes, each with its own story",
            de="Sechsundneunzig Welten: ganze Szenen, jede mit eigenem Tempo, eigenem Klang und einer kleinen Geschichte.",
            en="Ninety-six worlds: whole scenes, each with its own tempo, its own sound and a little story.")
    b.tap("R1", 14, 180); b.wait(900); b.tap("DOWN"); b.wait(1600)           # GLASS AND AIR > Northern Curtain
    b.tap("A"); b.wait(700); b.tap("A"); b.wait(8500)                         # (unsaved: A again enters now)
    b.home()
    b.wait(3000)
    b.menu(12); b.wait(1200)
    b.title(28, "Ten toys and little games", "They play along, in time and in the key",
            de="Auf der Spielwiese warten zehn Spielzeuge und kleine Spiele. Sie spielen mit, immer im Takt und immer in der Tonart.",
            en="The playground holds ten toys and little games. They play along, always in time and always in the key.")
    for k in range(4):
        b.tap("A"); b.wait(500); b.tap("A"); b.wait(5500); b.tap("R1"); b.wait(400)
    b.home()
    b.title(12, "Play it yourself", "The buttons become eight notes that always fit",
            de="Oder spiel selbst: Steuerkreuz und Tasten werden zu acht Tönen, die immer passen.",
            en="Or play it yourself: the D-pad and buttons become eight notes that always fit.")
    b.do("press FN"); b.wait(600)
    for k in ["LEFT", "DOWN", "RIGHT", "UP", "Y", "X", "B", "A", "X", "Y", "UP", "RIGHT"]:
        b.tap(k); b.wait(560)
    b.do("release FN"); b.wait(1500)
    b.menu(4); b.wait(800)
    b.title(11, "Dream journeys", "Fly from island to island, catch the falling stars",
            de="Traumreisen: Flieg von Insel zu Insel und fang die fallenden Sterne.",
            en="Dream journeys: fly from island to island and catch the falling stars.")
    b.tap("Y"); b.wait(1500); b.hold("RIGHT", 1500); b.hold("UP", 900); b.wait(5500)
    b.home(); b.home()
    b.title(11, "Fall asleep to it", "Lullabies and a sleep timer",
            de="Und wenn es Nacht wird: Schlaflieder und ein Schlaf-Timer. Die Musik klingt sanft aus, der Garten schläft.",
            en="And when night falls: lullabies and a sleep timer. The music fades gently, and the garden sleeps.")
    b.do("world 90"); b.wait(4000)                                            # Moon Over The Pond
    b.do("sleeptimer 14000"); b.wait(8000)
    b.title(12, "@card", "Coming soon|A spin-off of Mutable Dreams for the FM-1|12 instruments · 128 worlds · 10 toys · the Sound Lab|by Kid Bunka",
            de="Mutable Dreams für den Trimui Brick. Bald erhältlich. Von Kid Bunka.",
            en="Mutable Dreams for the Trimui Brick. Coming soon. By Kid Bunka.")
    b.wait(12000)
    return b


def short_board():
    b = Board()
    b.do("splash")
    b.title(3.5, "@card", "Coming soon|on the Trimui Brick",
            de="Mutable Dreams. Bald auf dem Trimui Brick.", en="Mutable Dreams. Coming soon to the Trimui Brick.")
    b.wait(3500)
    b.wait(3000)
    b.title(8, "A garden of sound", "Generative music in your pocket",
            de="Ein Garten aus Klang, der von selbst Musik macht.", en="A garden of sound that makes music by itself.")
    b.tap("DOWN"); b.hold("RIGHT", 1200); b.wait(4000)
    b.tap("F1"); b.wait(300)
    b.do("press L1"); b.wait(120); b.do("press R1"); b.wait(200); b.do("release R1"); b.do("release L1"); b.wait(800)
    b.title(13, "The Sound Lab", "Twelve instruments, Mutable Instruments modules",
            de="Ein Klanglabor mit zwölf Instrumenten und den Modulen von Mutable Instruments.",
            en="A Sound Lab with twelve instruments and Mutable Instruments' modules.")
    b.tap("RIGHT", 4, 300); b.tap("A"); b.wait(500)
    b.hold("RIGHT", 1300); b.do("lab SYNTH.LEVEL 65"); b.tap("DOWN"); b.tap("RIGHT", 8, 110); b.wait(3500); b.tap("RIGHT"); b.wait(3500)
    b.tap("F1"); b.wait(300)
    b.title(8, "128 worlds", "Each with its own sound and story",
            de="Sechsundneunzig Welten.", en="Ninety-six worlds.")
    b.do("world 85"); b.wait(700); b.tap("A"); b.wait(5660)                  # (the question: SEL - A - discard and load)
    b.menu(12); b.wait(800)
    b.title(9, "Toys and games", "Always in time, always in the key",
            de="Spielzeuge und Spiele, immer im Takt.", en="Toys and games, always in time.")
    for k in range(2):
        b.tap("A"); b.wait(400); b.tap("A"); b.wait(3000); b.tap("R1"); b.wait(300)
    b.home()
    b.title(7, "Play it yourself", "Eight notes that always fit",
            de="Oder spiel selbst.", en="Or play it yourself.")
    b.do("press FN"); b.wait(400)
    for k in ["LEFT", "DOWN", "RIGHT", "UP", "Y", "X", "B", "A", "X"]:
        b.tap(k); b.wait(400)
    b.do("release FN"); b.wait(500)
    b.title(6, "@card", "Coming soon|by Kid Bunka", de="Mutable Dreams. Bald. Von Kid Bunka.", en="Mutable Dreams. Coming soon. By Kid Bunka.")
    b.wait(6000)
    return b


def louder(path):
    """the WAV raised so its loudest sample is at -1 dBFS, by at most +12 dB (the stage mixes quietly, for headroom)"""
    import array, math, wave
    with wave.open(str(path), "rb") as w:
        par, data = w.getparams(), array.array("h", w.readframes(w.getnframes()))
    peak = max(1, max(abs(x) for x in data))
    g = min(10 ** (-1 / 20) * 32767 / peak, 10 ** (12 / 20))
    data = array.array("h", (max(-32767, min(32767, int(round(x * g)))) for x in data))
    with wave.open(str(path), "wb") as w:
        w.setparams(par)
        w.writeframes(data.tobytes())
    return 20 * math.log10(g)


def tc(t):
    return f"{int(t // 60)}:{t % 60:05.2f}"


def srt_time(t):
    ms = int(round(t * 1000))
    return f"{ms // 3600000:02d}:{ms // 60000 % 60:02d}:{ms // 1000 % 60:02d},{ms % 1000:03d}"


def make(kind):
    b = wide_board() if kind == "wide" else short_board()
    name = "Mutable-Dreams-Brick-Coming-Soon" + ("" if kind == "wide" else "-Short")
    work = Path(tempfile.mkdtemp())
    # (the runner's commands split at spaces: its pipe and WAV in the work folder, the WAV moved out afterwards)
    pipe, wav, script, tsv = work / "frames", work / "music.wav", work / "board.txt", OUT / f"{name}-titles.tsv"
    # (the music plays from the first frame: started before the film, under the start screen)
    pre = ["wait 2500", "tap START", "wait 1500", "tap MENU", "wait 350"] + ["tap DOWN", "wait 60"] * 12 + \
          ["tap A", "wait 9000", "press B", "wait 900", "release B", "wait 2000"]   # (the tale's first card seen before)
    lines = pre + [f"wav {wav}", f"film {pipe}"] + b.lines + ["filmstop", "wavstop"]
    script.write_text("\n".join(lines) + "\n")
    tsv.write_text("# start\tend\ttitle\tline (@card: a full card, its lines split at |)\n" +
                   "".join(f"{a:.2f}\t{e:.2f}\t{t}\t{l}\n" for a, e, t, l in b.titles))
    os.mkfifo(pipe)
    (work / "worlds").mkdir()                    # (the runner reads the worlds beside its save folder)
    shutil.copy(HERE / "package/knulli/mutabledreams/worlds/worlds.txt", work / "worlds/worlds.txt")
    enc = HERE / "build/film"
    if not enc.exists() or enc.stat().st_mtime < (HERE / "tools/film.swift").stat().st_mtime:
        subprocess.run(["swiftc", "-O", str(HERE / "tools/film.swift"), "-o", str(enc)], check=True)
    p = subprocess.Popen([str(enc), str(pipe), str(tsv), kind, str(wav), str(OUT / f"{name}.mp4"), str(FONTS)],
                         stderr=open(work / "encoder.log", "w"), env=dict(os.environ, KEEP_VIDEO="1"))
    subprocess.run([str(HERE / "build/mac/brick-headless"), str(script), str(work / "save")], check=True, cwd=HERE,
                   stdout=subprocess.DEVNULL)
    if p.wait() != 0:
        sys.exit(f"the encoder failed: {(work / 'encoder.log').read_text()[-2000:]}")
    gain = louder(wav)                           # (the music up to a level a film needs, then into the picture)
    if subprocess.run([str(enc), "-", str(tsv), kind, str(wav), str(OUT / f"{name}.mp4"), str(FONTS)],
                      stderr=open(work / "mux.log", "w")).returncode != 0 or not (OUT / f"{name}.mp4").exists():
        sys.exit(f"the music did not go in: {(work / 'mux.log').read_text()[-2000:]}")
    print(f"{name}: the music {gain:+.1f} dB")
    shutil.move(str(wav), str(OUT / f"{name}-music.wav"))
    for lang, k in (("DE", 1), ("EN", 2)):       # the voice-over: what to say when, and as subtitles
        v = [(t, d if lang == "DE" else e) for t, d, e in b.voice]
        (OUT / f"{name}-Sprechertext-{lang}.txt").write_text(
            f"Mutable Dreams for Trimui Brick - {name} - voice-over ({lang}), {tc(b.t)} long. Each line starts at its time.\n\n" +
            "".join(f"[{tc(t)}]  {s}\n" for t, s in v))
        (OUT / f"{name}-{lang}.srt").write_text("".join(
            f"{i + 1}\n{srt_time(t)} --> {srt_time((v[i + 1][0] if i + 1 < len(v) else b.t) - 0.3)}\n{s}\n\n" for i, (t, s) in enumerate(v)))
    print(f"{name}: {tc(b.t)}")


OUT.mkdir(parents=True, exist_ok=True)
for k in WANT:
    make(k)
