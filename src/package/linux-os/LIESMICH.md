# Dream Garden: Mutable Dreams als Programm in Mutable Dreams Linux

Für Kid Bunkas Mutable Dreams Linux (Ubuntu Studio mit KDE Plasma 6, Ziel Dell E7240, x86_64). Stand 0.80, 16.11.2026. Geschrieben im Brick-Chat. Im Linux-Projekt `~/Mutable-Dreams-Linux` liegt dazu der Baustein `addons/dream-garden/` (Pins, Prüfskript, README; Commit fc43008). Neu in 0.80: Deutsch/Englisch, `keys.txt` für die Tastatur (`mutable-dreams --keys`), das Handbuch DE/EN unter `share/doc/mutable-dreams` (`mutable-dreams --manual`), beides auch als Aktion im Menüeintrag.

## Name

Im System heißt das Programm **Dream Garden**, App-ID `org.kidbunka.mutabledreams.dreamgarden`. Der Grund: Ein Programm namens „Mutable Dreams“ in einem System namens „Mutable Dreams Linux“ wäre verwirrend. „Dream Garden“ passt zur Namensreihe der anderen Programme (Synth Studio, Tracker Lab, Visual Lab …) und zum Garten als Kern der App.

Der Name ist nur ein Vorschlag: Das zweite Argument von `install.sh` setzt einen anderen, zum Beispiel „Traumgarten“. Der Befehl bleibt `mutable-dreams`.

## Paket und Einbau

Das Paket `Mutable-Dreams-Linux-OS-x86_64-1.1.0.zip` erzeugt `tools/make_linux_os.py x86_64`.

```sh
unzip Mutable-Dreams-Linux-OS-x86_64-1.1.0.zip && cd mutable-dreams
DESTDIR=/pfad/zum/rootfs sh install.sh /usr "Dream Garden"   # ins OS-Abbild
sh install.sh                                                # oder auf einem laufenden System, nach /usr/local
```

Was `install.sh` einrichtet:

| Ort | Inhalt |
|---|---|
| `PREFIX/lib/mutable-dreams/` | Programm und mitgelieferte Dateien |
| `PREFIX/bin/mutable-dreams` | Starter |
| `PREFIX/share/applications/org.kidbunka.mutabledreams.dreamgarden.desktop` | Menüeintrag (Kategorie Audio/Musik) |
| `share/icons/hicolor/256x256/apps/` | Symbol unter dem Namen der App-ID |
| `share/metainfo/` | AppStream-Beschreibung |

- **Symbol im Garden-Icon-Theme:** Das Theme des Linux-Projekts kann ein eigenes Symbol unter dem Namen der App-ID liefern, zum Beispiel das Blumenzeichen `md-brick-garden`. Dann hat es Vorrang.
- **Daten pro Nutzer:** Jeder Nutzer bekommt seine Kopie in `~/.local/share/mutable-dreams`. Ein Update erneuert dort die Programmdateien, `save/` und `samples/` aber nie.

## Auf dem PC

- **Bildschirm:** Vollbild in der Auflösung des Bildschirms. Die Bühne bleibt im Seitenverhältnis 4:3 und wird weich skaliert (`MD_DESKTOP=1`, setzt der Starter). Mit `MD_WINDOW=1` oder `--window` läuft es im Fenster.
- **Ton:** SDL2 über PulseAudio oder PipeWire.
- **Voraussetzungen:** das Paket `libsdl2-2.0-0` (in Ubuntu Studio vorhanden) und glibc 2.34 oder neuer, also Ubuntu 22.04 und neuer.
- **Bedienung:** ein Gamecontroller, oder die Tastatur:

| Brick | Tastatur | Brick | Tastatur |
|---|---|---|---|
| Steuerkreuz | Pfeiltasten | START | Enter |
| A / B | X / Z | SELECT | Rücktaste |
| X / Y | S / A | MENU | Esc |
| L1 / R1 | Q / W | F1 / F2 | F1 / F2 |
| L2 / R2 | 1 / 2 oder Umschalt | FN (Spielmodus) | Tab |

Beenden: MENU (Esc), dann QUIT.

Ohne Controller nennen die Hinweise und die Einführung diese Tasten selbst. Ab 0.78 steht dort zum Beispiel „SHIFT+S KEEP“ statt „R2+X KEEP“.

## Geprüft und nicht geprüft

- **Geprüft:**
  - Der x86_64-Build entsteht mit Zig (glibc-Ziel 2.35); er braucht nur `libSDL2-2.0.so.0`, `libm` und `libc`.
  - `install.sh` und der erste Start (Datenkopie) laufen am Mac in einem Testbaum.
- **Nicht geprüft:** der Start auf einem echten Linux-PC, also Bild, Ton, Controller und Plasma-Menü. Das ist der nächste Schritt, im Linux-Projekt oder in dessen VM.
