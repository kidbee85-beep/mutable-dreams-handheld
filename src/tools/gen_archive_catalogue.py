#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutable Dreams for Trimui Brick, by Kid Bunka: the ARCHIVE catalogue (curated voices for VOICES).

Builds, on the Mac, the list of poems the Brick may fetch: LibriVox recordings on archive.org, one Ogg file each.
A poem goes in only if
  - its archive.org item carries a free licence: public domain, CC0, the public domain mark, or CC-BY
  - its author is found at LibriVox with a death year before CUTOFF (died more than 70 years ago: free in the EU)
  - it lasts 20 s .. 4 min, and its title does not say it is a translation
At most PER_AUTHOR poems of one author per basket. Every answer of archive.org and LibriVox is kept in
build/archive-cache/, so each is asked once.

  gen_archive_catalogue.py OUT.tsv REVIEW.md

OUT.tsv columns: basket, item, file, title, author, born, died, reader, seconds, bytes, licence
"""
import json
import os
import re
import sys
import time
import unicodedata
import urllib.parse
import urllib.request
from pathlib import Path

CUTOFF = 1956                                    # died before this year
PER_AUTHOR = 4
MAX_PER_BASKET = 90
UA = "MutableDreams-catalogue/0.29 (+librivox and archive.org, cached, polite)"
CACHE = Path(__file__).resolve().parents[1] / "build" / "archive-cache"
BASKETS = [
    ("GERMAN POEMS", "Sammlung deutscher Gedichte", [f"{i:03d}" for i in range(1, 41)]),
    ("ENGLISH POEMS", "Short Poetry Collection", [f"{i:03d}" for i in range(1, 31)]),
]
FREE = ("publicdomain", "/zero/", "/by/")        # PD, PD mark, CC0, CC-BY (not -sa, -nc, -nd)
TRANSLATED = re.compile(r"(übers|uebers|übertr|translat|nachdicht)", re.I)
NAMELESS = re.compile(r"^(unknown|anonymous|anon\.?|various|unbekannt|traditional)$", re.I)
# poets of other languages: in the German basket they can only be translations (translator unknown)
FOREIGN = re.compile(r"(shakespeare|byron|shelley|keats|burns|poe|whitman|baudelaire|verlaine|rimbaud|hugo|petrar|"
                     r"dante|hafis|hafez|omar|tagore|puschkin|pushkin|lermontov|tennyson|wordsworth|dickinson|"
                     r"longfellow|ovid|horaz|horace|homer|sappho|catull)", re.I)


def fetch(url, key):
    CACHE.mkdir(parents=True, exist_ok=True)
    f = CACHE / (re.sub(r"[^A-Za-z0-9_.-]+", "_", key)[:150] + ".json")
    if f.exists():
        return json.loads(f.read_text())
    time.sleep(0.4)                              # (polite)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    try:
        with urllib.request.urlopen(req, timeout=40) as r:
            data = json.loads(r.read().decode("utf-8"))
    except Exception as e:                       # (a failed answer is kept too, as nothing)
        print("  !", url, e, file=sys.stderr)
        data = {}
    f.write_text(json.dumps(data))
    return data


def free_licence(url):
    u = (url or "").lower()
    return any(k in u for k in FREE) and not any(k in u for k in ("-sa", "-nc", "-nd", "/by-sa", "/by-nc", "/by-nd"))


def norm(s):
    s = unicodedata.normalize("NFKD", s or "").encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z ]+", " ", s).split()


_authors = {}
def author_dates(name):
    """(full name, born, died) from LibriVox, matched by last name and first initial; None if unsure"""
    if name in _authors:
        return _authors[name]
    words = norm(name)
    if not words:
        _authors[name] = None
        return None
    last = words[-1]
    data = fetch("https://librivox.org/api/feed/authors/?format=json&last_name=" + urllib.parse.quote(last), "author_" + last)
    best = None
    for a in data.get("authors", []) or []:
        lw, fw = norm(a.get("last_name")), norm(a.get("first_name"))
        if not lw or lw[-1] != last:
            continue
        if len(words) > 1 and fw and fw[0][0] != words[0][0]:
            continue
        try:
            died = int(str(a.get("dod") or "").strip()[:4])
        except ValueError:
            continue
        cand = (f"{a.get('first_name', '')} {a.get('last_name', '')}".strip(), str(a.get("dob") or "").strip(), died)
        if best and best[2] != died:             # two authors of that name with different years: unsure
            best = None
            break
        best = cand
    _authors[name] = best
    return best


def book(title):
    """LibriVox's collection of that exact title: (archive.org identifier, {file base: reader})"""
    data = fetch("https://librivox.org/api/feed/audiobooks/?format=json&extended=1&title=" +
                 urllib.parse.quote("^" + title), "book_" + title)
    for b in data.get("books", []) or []:
        if (b.get("title") or "").strip() != title:
            continue
        ident = (b.get("url_iarchive") or "").rstrip("/").rsplit("/", 1)[-1]
        readers = {}
        for sec in b.get("sections", []) or []:
            base = (sec.get("listen_url") or "").rsplit("/", 1)[-1].replace("_64kb.mp3", "")
            names = [r.get("display_name") for r in sec.get("readers", []) or [] if r.get("display_name")]
            if base and names:
                readers[base] = ", ".join(names)
        return ident, readers
    return None, {}


def person(name):
    """'Hoelderlin, Friedrich' > 'Friedrich Hoelderlin'"""
    name = (name or "").strip()
    if name.count(",") == 1:
        last, first = [x.strip() for x in name.split(",")]
        return f"{first} {last}".strip()
    return name


def main(out_tsv, review_md):
    rows, review = [], []
    for basket, prefix, numbers in BASKETS:
        per_author, kept, skipped = {}, [], {"licence": 0, "author unknown/too young": 0, "length": 0, "translation": 0}
        for number in numbers:
            ident, readers = book(f"{prefix} {number}")
            if not ident:
                continue
            meta = fetch("https://archive.org/metadata/" + ident, "meta_" + ident)
            lic = (meta.get("metadata", {}) or {}).get("licenseurl") or ""
            if not free_licence(lic):
                skipped["licence"] += 1
                continue
            named = {}                           # file base > (title, author): the Ogg, else its MP3 twin
            for f in meta.get("files", []) or []:
                base = re.sub(r"(_64kb)?\.(mp3|ogg)$", "", f.get("name", ""))
                if f.get("title") and f.get("creator") and base not in named:
                    named[base] = (f["title"].strip(), person(f["creator"]))
            for f in meta.get("files", []) or []:
                if f.get("format") != "Ogg Vorbis":
                    continue
                base = f["name"][:-4]
                title, creator = named.get(base, ("", ""))
                try:
                    secs, size = float(f.get("length") or 0), int(f.get("size") or 0)
                except ValueError:
                    continue
                if not title or not creator:
                    skipped["no title/author"] = skipped.get("no title/author", 0) + 1
                    continue
                if not (20 <= secs <= 240):
                    skipped["length"] += 1
                    continue
                title = re.sub(r"^\s*\d+\s*[-.:]\s*", "", title)   # (a track number in front: away)
                if TRANSLATED.search(title) or (basket.startswith("GERMAN") and FOREIGN.search(creator)):
                    skipped["translation"] += 1
                    continue
                if NAMELESS.match(creator.strip()):
                    skipped["author unknown/too young"] += 1
                    continue
                who = author_dates(creator)
                if not who or who[2] >= CUTOFF:
                    skipped["author unknown/too young"] += 1
                    continue
                try:
                    born = int(who[1][:4])
                except ValueError:
                    born = 0
                if born and born < 1500:         # here only as a translation, its translator unknown
                    skipped["translation"] += 1
                    continue
                if per_author.get(who[0], 0) >= PER_AUTHOR or len(kept) >= MAX_PER_BASKET:
                    continue
                per_author[who[0]] = per_author.get(who[0], 0) + 1
                kept.append((basket, ident, f["name"], title, who[0], who[1], str(who[2]),
                             readers.get(base, ""), str(int(round(secs))), str(size), lic))
            if len(kept) >= MAX_PER_BASKET:
                break
        rows += kept
        review.append(f"## {basket}: {len(kept)} Gedichte\n\nAusgelassen: " +
                      ", ".join(f"{k} {v}" for k, v in skipped.items()) + "\n")
        review += [f"- **{r[3]}**, {r[4]} ({r[5]}–{r[6]}), gelesen von {r[7] or '?'}, {int(r[8]) // 60}:{int(r[8]) % 60:02d}, "
                   f"[{r[1]}](https://archive.org/details/{r[1]})" for r in kept]
        review.append("")
    with open(out_tsv, "w", encoding="utf-8") as o:
        o.write("# Mutable Dreams ARCHIVE catalogue: basket, item, file, title, author, born, died, reader, seconds, bytes, licence\n")
        for r in rows:
            o.write("\t".join(x.replace("\t", " ").replace("\n", " ") for x in r) + "\n")
    Path(review_md).write_text("# Archiv-Katalog: Stimmen für VOICES\n\n"
                               f"Erzeugt von `tools/gen_archive_catalogue.py`. Regel: freie Lizenz des Eintrags (gemeinfrei, CC0, Public "
                               f"Domain Mark, CC-BY), Autor:in bei LibriVox mit Todesjahr vor {CUTOFF}, 20 s bis 4 min, keine "
                               "Übersetzung im Titel und keine Autor:innen vor 1500 (hier nur übersetzt), höchstens vier Gedichte je Autor:in. Bitte einmal durchsehen: Übersetzungen "
                               "ohne Hinweis im Titel kann das Werkzeug nicht erkennen.\n\n" + "\n".join(review), encoding="utf-8")
    print(f"{len(rows)} poems -> {out_tsv}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
