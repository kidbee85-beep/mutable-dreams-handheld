# Notice: third-party code and credits

Mutable Dreams for handhelds is by **Kid Bunka**, licensed under the **GNU General Public License version 3 only** (`LICENSE`). It is built from the unchanged **Mutable Dreams 2.1** sources (https://github.com/kidbee85-beep/mutable-dreams, release v2.1) plus the port layer in `src/`.

Included code and its notices (the full list, with licence texts, is in `src/package/knulli/mutabledreams/SOURCE.txt` and `LICENSE-MIT-Mutable-Instruments.txt`):

- **Mutable Instruments** modules by Émilie Gillet: Plaits, Rings, Elements, Clouds, Beads, Warps, Marbles, Tides 2 and stmlib (MIT; two fixes in Clouds: `dsp/window.h`, `dsp/grain.h`); the drum map and its blending in GRIDS from Mutable Instruments Grids (GPL-3.0-or-later, used under GPL-3.0). `src/third_party/mi/`.
- **stb_vorbis** 1.22 by Sean Barrett (public domain / MIT, used under its public domain dedication), `src/third_party/stb/`.
- **felucca-bsp / fm1-base** (the FM-1 platform layer; MPL-2.0, distributed under GPL-3.0 per MPL-2.0 section 3.3) comes with the 2.1 sources.
- **SDL2** (zlib licence): the headers in `src/third_party/SDL2/`; the programs use the system's SDL2 (the Miyoo packages carry steward-fu's `mmiyoo` SDL2, zlib licence).
- **Fonts** Barlow and Nunito (SIL Open Font License 1.1): `docs/assets/fonts/` with their licence texts.
- **Voices** in the archive: LibriVox recordings via archive.org, public domain or freely licensed, credited beside each file.
- **Visual Kit** (`docs/assets/js/kit.js`): by Kid Bunka, GPL-3.0.

Independent from Mutable Instruments, M-VAVE, Trimui, Anbernic, Miyoo and Powkiddy. All trademarks belong to their owners.
