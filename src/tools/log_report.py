#!/usr/bin/env python3
"""tools/log_report.py LOG [LOG ...]: a device's log.txt read page by page.

Every 30 s the program writes a "status" line: underruns since the last line, frames per second,
the stage's draw time (and how much of it was spent letting the sound in), the present time, the
lead kept for the sound, the sound load and the page. The session line at the end sums it up.

This prints, for each page: how often it was seen, its frame rate, draw and present time, and how
many underruns fell into those 30 s. Pages are sorted by the slowest draw first, so the next place
to optimise (or where the picture still stutters) is at the top. A page that is drawn slower than
its frame allows is marked; so is every interval with underruns.

    python3 tools/log_report.py ../Geraete-Logs/Brick-0.60-log.txt
    python3 tools/log_report.py ../Geraete-Logs/*.txt --worst 10
    python3 tools/log_report.py log.txt --json

Exit code 1 when a page is over its frame budget or the session had underruns (a quick check in
tools/reg.sh style scripts); 0 otherwise.
"""
import argparse
import json
import re
import sys
from pathlib import Path

STATUS = re.compile(
    r"status\s+(?P<t>[\d.]+)\s*s:\s*(?P<under>\d+)\s+underruns\s*\(\+(?P<new>\d+)\),\s*"
    r"(?P<fps>[\d.]+)\s*fps,\s*draw avg\s+(?P<draw>[\d.]+)\s*ms"
    r"(?:\s*\(of it waiting for the sound\s+(?P<wait>[\d.]+)\))?"
    r"(?:,\s*present avg\s+(?P<present>[\d.]+)\s*ms)?"
    r"(?:,\s*ahead\s+(?P<ahead>\d+))?"
    r"(?:,\s*sound load now\s+(?P<load>\d+)%)?"
    r",\s*page\s+(?P<page>.+?)\s*$"
)
UNDERRUN = re.compile(r"audio: underrun\s+\d+\s+at\s+([\d.]+)\s*s(?:,\s*page\s+(.+?))?\s*$")
SESSION = re.compile(
    r"(?:session|self-test):\s*(?P<dur>[\d.]+)\s*s,\s*sound\s+(?P<sound>on|OFF)\s+"
    r"(?P<rate>\d+)\s*Hz.*?renderer\s+(?P<renderer>.+?),\s*(?P<frames>\d+)\s+stage frames"
    r"\s*\((?P<fps>[\d.]+)/s\),\s*(?P<under>\d+)\s+underruns.*?"
    r"stage draw avg\s+(?P<draw>[\d.]+)\s*ms\s+max\s+(?P<max>[\d.]+)\s*ms"
    r"(?:.*?page\s+(?P<page>.+?),)?",
)
VIDEO = re.compile(r"video:\s+driver\s+(\S+),\s+renderer\s+(\S+)")
AUDIO = re.compile(r"audio:\s+driver\s+(\S+),\s+(\d+)\s*Hz.*?(\d+)\s+frames")
AHEAD = re.compile(r"audio:\s+kept\s+(\d+)\s+frames ahead")
SCALE = re.compile(r"video:\s+the stage scaled to\s+(\d+)\s*x\s*(\d+)")
KERNEL = re.compile(r"Linux\s+(\d+\.\d+\.\d+\S*)")


def parse(path: Path) -> dict:
    out = {"file": str(path), "pages": {}, "intervals": [], "underruns": [], "session": None,
           "video": None, "audio": None, "ahead": None, "scale": None, "kernel": None,
           "profile_lines": 0}
    page = None
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        m = VIDEO.search(line)
        if m:
            out["video"] = f"{m.group(1)} / {m.group(2)}"
        m = AUDIO.search(line)
        if m:
            out["audio"] = f"{m.group(1)} {m.group(2)} Hz, {m.group(3)} frames/buffer"
        m = AHEAD.search(line)
        if m:
            out["ahead"] = int(m.group(1))
        m = SCALE.search(line)
        if m:
            out["scale"] = f"{m.group(1)}x{m.group(2)}"
        m = KERNEL.search(line)
        if m and not out["kernel"]:
            out["kernel"] = m.group(1)
        m = UNDERRUN.search(line)
        if m:
            out["underruns"].append({"at": float(m.group(1)), "page": (m.group(2) or page or "?").strip()})
            continue
        m = STATUS.search(line)
        if m:
            page = m.group("page").strip()
            iv = {
                "t": float(m.group("t")),
                "new": int(m.group("new")),
                "total": int(m.group("under")),
                "fps": float(m.group("fps")),
                "draw": float(m.group("draw")),
                "wait": float(m.group("wait") or 0.0),
                "present": float(m.group("present") or 0.0),
                "ahead": int(m.group("ahead") or 0),
                "load": int(m.group("load") or 0),
                "page": page,
            }
            out["intervals"].append(iv)
            p = out["pages"].setdefault(page, {"n": 0, "draw": 0.0, "fps": 0.0, "present": 0.0,
                                               "wait": 0.0, "new": 0, "load": 0})
            p["n"] += 1
            for k in ("draw", "fps", "present", "wait", "load"):
                p[k] += iv[k]
            p["new"] += iv["new"]
            continue
        if line.startswith("draw profile"):
            out["profile_lines"] += 1
            continue
        m = SESSION.search(line)
        if m:
            out["session"] = {
                "seconds": float(m.group("dur")),
                "sound": m.group("sound"),
                "rate": int(m.group("rate")),
                "renderer": m.group("renderer").strip(),
                "frames": int(m.group("frames")),
                "fps": float(m.group("fps")),
                "underruns": int(m.group("under")),
                "draw_avg": float(m.group("draw")),
                "draw_max": float(m.group("max")),
                "page": (m.group("page") or "").strip(),
            }
    return out


def report(rep: dict, worst: int, target_fps: float) -> bool:
    name = Path(rep["file"]).name
    print(f"== {name}")
    facts = [f for f in (rep["kernel"] and f"kernel {rep['kernel']}",
                         rep["video"] and f"video {rep['video']}",
                         rep["audio"] and f"audio {rep['audio']}",
                         rep["scale"] and f"scaled to {rep['scale']}",
                         rep["ahead"] and f"lead {rep['ahead']} frames") if f]
    if facts:
        print("   " + " · ".join(facts))

    s = rep["session"]
    if s:
        print(f"   session {s['seconds']:.0f} s · {s['frames']} frames ({s['fps']:.1f}/s) · "
              f"draw {s['draw_avg']:.1f} ms avg / {s['draw_max']:.1f} ms max · "
              f"{s['underruns']} underruns · end on {s['page']}")
    if not rep["intervals"]:
        print("   no status lines (a log from before 0.60, or the session was too short)")
        return True

    print(f"   {'page':<14} {'seen':>4} {'fps':>6} {'draw':>7} {'present':>8} {'wait':>6} {'underr':>7}")
    order = sorted(rep["pages"].items(), key=lambda kv: -(kv[1]["draw"] / kv[1]["n"]))
    budget = 1000.0 / target_fps if target_fps > 0 else 0.0
    bad = False
    for page, p in order:
        n = p["n"]
        draw, fps = p["draw"] / n, p["fps"] / n
        over = budget > 0 and draw > budget * 0.85
        bad = bad or over or p["new"] > 0
        mark = "!" if over else ("*" if p["new"] else " ")
        print(f" {mark} {page[:13]:<13} {n:>4} {fps:>6.1f} {draw:>7.1f} {p['present'] / n:>8.1f} "
              f"{p['wait'] / n:>6.1f} {p['new']:>7}")
    print(f"   (! draw over 85 % of a {target_fps:.0f} fps frame, {budget:.1f} ms; * underruns in these 30 s)")

    slow = sorted(rep["intervals"], key=lambda iv: -iv["draw"])[:worst]
    if slow:
        print(f"   slowest {len(slow)} intervals:")
        for iv in slow:
            print(f"     {iv['t']:>6.0f} s  {iv['draw']:>6.1f} ms draw  {iv['present']:>6.1f} ms present  "
                  f"+{iv['new']:>3} underruns  {iv['page']}")
    if rep["profile_lines"]:
        print(f"   {rep['profile_lines']} draw profile line(s): "
              f"python3 tools/symbolize_profile.py <log> <the program that ran>")
    if s and s["underruns"]:
        print("   SESSION HAD UNDERRUNS")
    return not bad and not (s and s["underruns"])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+", type=Path)
    ap.add_argument("--worst", type=int, default=5, help="how many slow intervals to list (default 5)")
    ap.add_argument("--fps", type=float, default=30.0,
                    help="the frame rate the device is meant to hold (MD_FPS; default 30, Miyoo 15)")
    ap.add_argument("--json", action="store_true", help="the parsed numbers as JSON, no table")
    args = ap.parse_args()

    files = []
    for p in args.logs:
        if p.is_dir():
            files += sorted(p.glob("*.txt"))
        else:
            files.append(p)
    if not files:
        print("no logs", file=sys.stderr)
        return 2

    ok = True
    for f in files:
        if not f.is_file():
            print(f"missing: {f}", file=sys.stderr)
            ok = False
            continue
        rep = parse(f)
        if args.json:
            print(json.dumps(rep, indent=2))
        else:
            ok = report(rep, args.worst, args.fps) and ok
            print()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
