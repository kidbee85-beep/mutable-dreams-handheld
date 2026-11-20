#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutable Dreams for Trimui Brick: the stage's own faces, about 1.6 x the FM-1's (whose faces are made for a
240 x 240 screen; on the Brick's 1024 x 768 at 3.2 inches they were too small to read). Rendered by the firmware's
base/tools/gen_font.py (same glyph format) from its bundled Barlow and Nunito. The stage (brick_stage.inc and what
it includes) uses them under the FM-1's names (brick_core.c maps FONT_T -> FONT_ST_T ...); the FM-1 engine keeps its
own. usage: gen_stage_font.py BASE_DIR OUT.h"""
import importlib.util
import sys
from pathlib import Path

base, out = Path(sys.argv[1]), sys.argv[2]
import types
src = (base / "tools/gen_font.py").read_text()   # (read, not changed: its report line assumes a part with the digits)
src = src.replace("digit adv {glyphs[ord('0') - first][0]}", "first {first}")
# (0.80) the German faces (from 196) keep the dots over capital umlauts: their top is measured with them, so they
# are a few rows taller than the base face; bk_text lifts them by the difference (the baselines stay together)
src = src.replace('font.getbbox("A8|(", features=feats)', 'font.getbbox("\u00c4\u00d6\u00dcA8|(" if first >= 192 else "A8|(", features=feats)')
gf = types.ModuleType("gen_font")
gf.__file__ = str(base / "tools/gen_font.py")
exec(compile(src, gf.__file__, "exec"), gf.__dict__)
gf.SIZES = [("ST_T", "BarlowSemiCondensed-Bold.ttf", 19, 1, False, 32, 126),
            ("ST_XS", "BarlowSemiCondensed-Bold.ttf", 20, 1, False, 32, 126),
            ("ST_H", "BarlowSemiCondensed-Bold.ttf", 22, 1, False, 32, 126),
            ("ST_S", "Nunito[wght].ttf@Weight=700", 22, 1, False, 32, 126),
            ("ST_B", "Nunito[wght].ttf@Weight=800", 25, 1, False, 32, 126),
            ("ST_M", "Nunito[wght].ttf@Weight=750", 30, 1, False, 32, 126),
            ("ST_L", "Nunito[wght].ttf@Weight=800", 40, 1, False, 32, 126),
            # (0.80) German: the same faces from 196 (A umlaut) to 252 (u umlaut), with sharp s between - the stage
            # writes them in UTF-8 and bk_text finds them here
            ("ST_T_X", "BarlowSemiCondensed-Bold.ttf", 19, 1, False, 196, 252),
            ("ST_XS_X", "BarlowSemiCondensed-Bold.ttf", 20, 1, False, 196, 252),
            ("ST_H_X", "BarlowSemiCondensed-Bold.ttf", 22, 1, False, 196, 252),
            ("ST_S_X", "Nunito[wght].ttf@Weight=700", 22, 1, False, 196, 252),
            ("ST_B_X", "Nunito[wght].ttf@Weight=800", 25, 1, False, 196, 252),
            ("ST_M_X", "Nunito[wght].ttf@Weight=750", 30, 1, False, 196, 252),
            ("ST_L_X", "Nunito[wght].ttf@Weight=800", 40, 1, False, 196, 252),
            # the FM-1's own faces at three times their size: its pages' words, set sharp on the 3x stage. A face's
            # glyphs must stay under 64 KB (16-bit offsets): the large ones come in parts, by character range
            ("H3_T", "BarlowSemiCondensed-Bold.ttf", 33, 1, False, 32, 126),
            ("H3_XS", "BarlowSemiCondensed-Bold.ttf", 36, 1, False, 32, 126),
            ("H3_H", "BarlowSemiCondensed-Bold.ttf", 42, 1, False, 32, 126),
            ("H3_S", "Nunito[wght].ttf@Weight=650", 42, 1, False, 32, 126),
            ("H3_B0", "Nunito[wght].ttf@Weight=800", 48, 1, False, 32, 79),
            ("H3_B1", "Nunito[wght].ttf@Weight=800", 48, 1, False, 80, 126),
            ("H3_M0", "Nunito[wght].ttf@Weight=700", 66, 1, False, 32, 63),
            ("H3_M1", "Nunito[wght].ttf@Weight=700", 66, 1, False, 64, 95),
            ("H3_M2", "Nunito[wght].ttf@Weight=700", 66, 1, False, 96, 126),
            ("H3_L0", "Nunito[wght].ttf@Weight=750", 90, 1, False, 32, 55),
            ("H3_L1", "Nunito[wght].ttf@Weight=750", 90, 1, False, 56, 71),
            ("H3_L2", "Nunito[wght].ttf@Weight=750", 90, 1, False, 72, 87),
            ("H3_L3", "Nunito[wght].ttf@Weight=750", 90, 1, False, 88, 95)]
gf.FONTSET = "stage"
gf.main(out)
