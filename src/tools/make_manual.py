#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/make_manual.py OUTDIR: the manual (Handbuch) in German and English, as HTML pages in the app's own look and
as PDF (Google Chrome, headless). Text: manual/de.html and manual/en.html (HTML with three small helpers:
<fig src="02-garden-playing">caption</fig> a picture from the app, <k>A</k> a button as the app's pebble,
<tip>...</tip> a leaf with a hint). Pictures: tools/manual_shots.py, taken in each language.
Fonts: the app's own (Barlow Semi Condensed, Nunito, from the FM-1 sources, SIL OFL)."""
import html, os, re, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
OUT = Path(sys.argv[1]).resolve()
FONTS = Path(os.environ.get("MD21_SOURCE", HERE.parent.parent / "FM-1/Release-2.1/Source")) / "base/assets/fonts"
VERSION = re.search(r"(?:test build|release) ([0-9][^ \n]*)", (HERE / "package/knulli/mutabledreams/README.txt").read_text())[1]
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

CSS = """
@font-face { font-family: 'Nunito'; src: url('fonts/Nunito.ttf'); font-weight: 200 1000; }
@font-face { font-family: 'Barlow'; src: url('fonts/BarlowSemiCondensed-Bold.ttf'); font-weight: 700; }
@font-face { font-family: 'Barlow'; src: url('fonts/BarlowSemiCondensed-SemiBold.ttf'); font-weight: 600; }
:root { --night: #04070e; --deep: #0a1220; --panel: #101b28; --ink: #ebf0e8; --dim: #a9b4bd; --faint: #6f7d88;
        --moss: #aecfb6; --sand: #dcc694; --teal: #86c2b4; --sky: #a5beda; --lilac: #c8b1d8; --rose: #cca091;
        --amber: #face8c; --leaf: #38664a; }
* { box-sizing: border-box; }
html { background: var(--night); }
body { margin: 0; color: var(--ink); font: 400 17px/1.6 'Nunito', sans-serif;
       background: radial-gradient(1200px 700px at 15% -10%, #12233a 0%, transparent 60%),
                   radial-gradient(900px 600px at 100% 20%, #1c1630 0%, transparent 60%), var(--night); }
main { max-width: 980px; margin: 0 auto; padding: 0 28px 80px; }
h1, h2, h3 { font-family: 'Nunito', sans-serif; font-weight: 800; line-height: 1.2; }
h2 { font-size: 34px; margin: 72px 0 8px; padding-top: 18px; color: var(--ink); position: relative; }
h2::before { content: ''; position: absolute; top: 0; left: 0; width: 120px; height: 4px; border-radius: 2px;
             background: linear-gradient(90deg, var(--moss), var(--sand), var(--teal), var(--sky), var(--lilac), var(--rose)); }
h2 .n { color: var(--amber); font-family: 'Barlow'; font-weight: 700; margin-right: 10px; }
h3 { font-size: 23px; margin: 36px 0 6px; color: var(--moss); }
h4 { font: 700 15px 'Barlow'; letter-spacing: .06em; text-transform: uppercase; color: var(--sand); margin: 24px 0 4px; }
p { margin: 8px 0 12px; }
a { color: var(--teal); }
strong { color: #fff; }
.label { font: 700 0.92em 'Barlow'; letter-spacing: .03em; color: var(--sand); }
kbd { display: inline-block; font: 700 14px/1 'Barlow'; letter-spacing: .02em; color: #0e141c; background: #e8ecde;
      border-radius: 999px; padding: 4px 10px 3px; margin: 0 2px; vertical-align: 1px; box-shadow: 0 2px 0 #00000055; }
.cover { min-height: 92vh; display: flex; flex-direction: column; justify-content: center; padding: 60px 0 40px; }
.cover .kicker { font: 700 16px 'Barlow'; letter-spacing: .3em; color: var(--sand); text-transform: uppercase; }
.cover h1 { font-size: 72px; margin: 10px 0 0; letter-spacing: -0.01em;
            background: linear-gradient(90deg, #fff, var(--moss) 40%, var(--sky) 70%, var(--lilac));
            -webkit-background-clip: text; background-clip: text; color: transparent; }
.cover .sub { font-size: 22px; color: var(--dim); margin: 6px 0 28px; }
.cover img { width: 100%; border-radius: 22px; box-shadow: 0 30px 80px #000c, 0 0 0 1px #ffffff14; }
.cover .meta { margin-top: 18px; color: var(--faint); font: 600 15px 'Barlow'; letter-spacing: .05em; }
.toc { columns: 2; column-gap: 40px; background: #ffffff08; border: 1px solid #ffffff12; border-radius: 22px;
       padding: 24px 30px; margin: 30px 0; }
.toc a { display: block; color: var(--ink); text-decoration: none; padding: 3px 0; break-inside: avoid; }
.toc a span { color: var(--amber); font: 700 15px 'Barlow'; display: inline-block; width: 30px; }
figure { margin: 22px 0 26px; }
figure img { width: 100%; display: block; border-radius: 16px; box-shadow: 0 18px 50px #000b, 0 0 0 1px #ffffff12; }
figcaption { color: var(--dim); font-size: 15px; margin-top: 8px; padding-left: 14px; border-left: 3px solid var(--leaf); }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 18px; }
.pair figure { margin: 10px 0; }
.tip { position: relative; background: linear-gradient(135deg, #1b3326, #12231c); border: 1px solid #3f6e52;
       border-radius: 18px 18px 18px 4px; padding: 14px 18px 14px 58px; margin: 18px 0; }
.tip::before { content: ''; position: absolute; left: 18px; top: 16px; width: 24px; height: 24px; border-radius: 0 60% 0 60%;
               background: linear-gradient(135deg, var(--moss), #5c9a72); transform: rotate(-10deg); }
.tip b.t { font: 700 14px 'Barlow'; letter-spacing: .1em; color: var(--moss); text-transform: uppercase; display: block; }
table { width: 100%; border-collapse: separate; border-spacing: 0; margin: 14px 0 20px; background: #ffffff06;
        border: 1px solid #ffffff12; border-radius: 16px; overflow: hidden; font-size: 16px; }
th { text-align: left; font: 700 13px 'Barlow'; letter-spacing: .08em; text-transform: uppercase; color: var(--sand);
     background: #ffffff0a; padding: 10px 14px; }
td { padding: 9px 14px; border-top: 1px solid #ffffff0d; vertical-align: top; }
td:first-child { white-space: nowrap; }
ul, ol { padding-left: 22px; } li { margin: 4px 0; }
.dots { display: inline-flex; gap: 6px; vertical-align: middle; }
.dots i { width: 12px; height: 12px; border-radius: 50%; display: inline-block; }
footer { margin-top: 80px; color: var(--faint); font-size: 14px; border-top: 1px solid #ffffff14; padding-top: 18px; }
@media (max-width: 700px) { .toc { columns: 1; } .pair { grid-template-columns: 1fr; } .cover h1 { font-size: 46px; } h2 { font-size: 27px; } }
@page { size: A4; margin: 0; }
@media print {
  html, body { -webkit-print-color-adjust: exact; print-color-adjust: exact; background: var(--night); }
  main { padding: 14mm 14mm 16mm; max-width: none; }
  h2 { break-before: page; margin-top: 0; }
  figure, .tip, table, .pair { break-inside: avoid; }
  .cover { min-height: auto; padding-top: 30mm; }
}
"""

def build(lang):
    src = (HERE / f"manual/{lang}.html").read_text()
    title = re.search(r"<title>(.*?)</title>", src).group(1)
    src = re.sub(r"<title>.*?</title>\s*", "", src)
    pics = f"bilder-{lang}"
    src = re.sub(r'<fig src="([^"]+)">(.*?)</fig>',
                 lambda m: f'<figure><img src="{pics}/{m.group(1)}.jpg" alt=""><figcaption>{m.group(2)}</figcaption></figure>', src, flags=re.S)
    src = re.sub(r"<k>(.*?)</k>", r"<kbd>\1</kbd>", src)
    tip_word = "Tipp" if lang == "de" else "Tip"
    src = re.sub(r"<tip>(.*?)</tip>", lambda m: f'<div class="tip"><b class="t">{tip_word}</b>{m.group(1)}</div>', src, flags=re.S)
    src = src.replace("{{VERSION}}", VERSION)
    n = 0
    toc = []
    def head(m):
        nonlocal n
        n += 1
        anchor = f"k{n}"
        toc.append(f'<a href="#{anchor}"><span>{n}</span>{m.group(1)}</a>')
        return f'<h2 id="{anchor}"><span class="n">{n}</span>{m.group(1)}</h2>'
    src = re.sub(r"<h2>(.*?)</h2>", head, src)
    src = src.replace("{{TOC}}", '<nav class="toc">' + "".join(toc) + "</nav>")
    page = (f'<!doctype html><html lang="{lang}"><head><meta charset="utf-8">'
            f'<meta name="viewport" content="width=device-width, initial-scale=1"><title>{html.escape(title)}</title>'
            f'<style>{CSS}</style></head><body><main>{src}</main></body></html>')
    name = "Mutable-Dreams-Handbuch-DE" if lang == "de" else "Mutable-Dreams-Manual-EN"
    (OUT / f"{name}.html").write_text(page)
    if Path(CHROME).exists():
        subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--virtual-time-budget=8000",
                        f"--print-to-pdf={OUT / (name + '.pdf')}", (OUT / f"{name}.html").as_uri()],
                       check=True, capture_output=True)
    return name

OUT.mkdir(parents=True, exist_ok=True)
(OUT / "fonts").mkdir(exist_ok=True)
for f, to in (("Nunito[wght].ttf", "Nunito.ttf"), ("BarlowSemiCondensed-Bold.ttf", None), ("BarlowSemiCondensed-SemiBold.ttf", None)):
    shutil.copyfile(FONTS / f, OUT / "fonts" / (to or f))
for lang in sys.argv[2:] or ["de", "en"]:
    print(build(lang))
