# Mutable Dreams: German UI glossary (0.80)

The app is written in English. With DEUTSCH chosen, each text it draws is looked up in `de.txt`, beside this file.

## File format

Pairs of lines. An English text exactly as drawn, then its German:

```
en CHOOSE
de WÄHLEN
```

- `#` stands for a number. It is put back in order, so the German line needs the same number of `#`.
- Text that is missing from the table stays English.

## Tone

- Short, friendly and dreamy, like the English. Address the user as "du".
- Keep CAPITALS where the English uses capitals, and normal case where it uses normal case.
- Umlauts and ß are written as such (UTF-8). In CAPITALS, ß becomes SS.
- German is about 20 % longer than English. Prefer short words, because space on the 1024 × 768 screen is tight. A hint action of 1–3 words should stay 1–3 words.

## Fixed terms

### Places

| English | German |
|---|---|
| GARDEN | GARTEN |
| MIXER | MIXER |
| SOUND LAB | KLANGLABOR |
| ATLAS / BIOME ATLAS | ATLAS / BIOM-ATLAS |
| SEQUENCE | SEQUENZ |
| DREAM MAP | TRAUMKARTE |
| WORLDS | WELTEN |
| ARCHIVE | ARCHIV |
| MODULATION | MODULATION |
| VOICE | STIMME |
| SPACE | RAUM |
| PLAYGROUND | SPIELWIESE |
| HELP | HILFE |
| MENU | MENÜ |
| PLAY MODE | SPIELMODUS |
| LIGHT PICTURE | LEICHTES BILD |
| FULL PICTURE | VOLLES BILD |
| CALM MOTION | RUHIGE BEWEGUNG |
| LIVELY MOTION | LEBHAFTE BEWEGUNG |
| QUICK BAR | SCHNELLLEISTE |
| THE GUIDE | DER WEGWEISER |
| DREAM TALE(S) | TRAUMGESCHICHTE(N) |
| CHAPTER | KAPITEL |
| FREE FLIGHT | FREIER FLUG |
| JOURNEY | REISE |

In normal case these follow the same pattern: Garden → Garten, Sound Lab → Klanglabor, Playground → Spielwiese, Worlds → Welten, Dream Map → Traumkarte.

### Biomes and the garden's dials

- Biome → Biom (plural: Biome).
- FIREFLIES (the dial) → LICHTER, WIND → WIND, GLOW → GLÜHEN, MIST → NEBEL. The toy FIREFLY → GLÜHWURM (short: it must fit the carousel).

### Toys (the playground)

| English | German |
|---|---|
| BOUNCE | HÜPFBÄLLE |
| ORBITS | PLANETEN |
| RAIN | REGEN |
| SWING | PENDEL |
| FIREFLY | GLÜHWURM |
| WHEEL | RAD |
| ECHO | ECHO |
| SEED | PUSTEBLUME |
| JELLY | QUALLE |
| STARFALL | STERNREGEN |
| KEEP | BEHALTEN |
| UNDO KEEP | ZURÜCK |

### Verbs in key hints

| English | German |
|---|---|
| CHOOSE | WÄHLEN |
| GO | LOS |
| CLOSE | SCHLIESSEN |
| BACK | ZURÜCK |
| TURN | DREHEN |
| PLAY | SPIELEN |
| EDIT | BEARBEITEN |
| HOLD | HALTEN |
| SAVE | SICHERN |
| TRACK | SPUR |
| DIAL | REGLER |
| VALUE | WERT |
| FIELD | FELD |
| PAGE | SEITE |
| ON/OFF | AN/AUS |
| MUTE | STUMM |
| LOAD | LADEN |
| ENTER | BETRETEN |
| WANDER | WANDERN |
| SURPRISE | ÜBERRASCHUNG |
| THROW | WERFEN |
| TILT | NEIGEN |
| GRAVITY | SCHWERKRAFT |

### Always English

These stay exactly as they are:

- Button names: A B X Y L1 R1 L2 R2 START SELECT MENU F1 F2 FN D-PAD, and the PC keys (SHIFT, ESC, …).
- Product and module names: Mutable Dreams, FM-1, Trimui Brick, Plaits, Rings, Elements, Clouds, Beads, Marbles, Grids, Tides, Warps, MIDI, LFO, EQ, BPM, Hz, dB, ms.
- Sound parameter names on the Sound Lab and voice pages: ATTACK, DECAY, DENSITY, TIMBRE, CUTOFF, …
- Scale and mode names.
- The names of Biomes, worlds, world families and poems, and the poets.
