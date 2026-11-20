#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutable Dreams for Trimui Brick: the start screen's large faces, rendered by the firmware's own
base/tools/gen_font.py (same glyph format) from its bundled Nunito: XL for the title (capitals only),
XM for the lines under it and the garden's Biome name (lower case too). usage: gen_splash_font.py BASE_DIR OUT.h"""
import importlib.util
import sys
from pathlib import Path

base, out = Path(sys.argv[1]), sys.argv[2]
spec = importlib.util.spec_from_file_location("gen_font", base / "tools/gen_font.py")
gf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gf)
gf.SIZES = [("XL", "Nunito[wght].ttf@Weight=850", 76, 1, False, 65, 90),
            ("XM", "Nunito[wght].ttf@Weight=700", 26, 1, False, 32, 126)]
gf.FONTSET = "splash"
gf.main(out)
