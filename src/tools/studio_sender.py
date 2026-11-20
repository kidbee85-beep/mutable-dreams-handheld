#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/studio_sender.py: run the FM-1 C engine (brick-studio) live and serve it as an MP3 radio stream.

    python3 tools/studio_sender.py [--port 8790] [--ffmpeg ffmpeg] [--mastering suite|classic|off]

The whole Mutable Dreams engine - the real Mutable Instruments DSP, the Sound Lab and the mastering chain -
runs in brick-studio (build/mac/brick-studio) in real time, streams 44.1 kHz float32 stereo on its stdout.
This script pipes that through ffmpeg with the same mastering suite as radio_station.py and serves the MP3 at

    http://127.0.0.1:PORT/radio.mp3      (the stream; MP3 320 kbit/s)
    http://127.0.0.1:PORT/health         {"onAir": true, "biome": "...", "playing": ...}
    POST /command                         a control line for brick-studio (text/plain): "biome 12", "master WARM", ...

This is the native path for the radio studio: instead of Chromium's Web Audio, the actual C engine. Nothing is
published and no live sender is touched; it runs on its own port for the A/B test.
"""
import argparse
import json
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from queue import Empty, Queue

HERE = Path(__file__).resolve().parent.parent
STUDIO = HERE / "build" / "mac" / "brick-studio"

MASTERING = {
    "suite": (
        "highpass=f=25:p=2,"
        "acrossover=split=150 2500:order=4th[low][mid][high];"
        "[low]pan=stereo|c0=0.5*c0+0.5*c1|c1=0.5*c0+0.5*c1,"
        "acompressor=threshold=0.063:ratio=2:attack=30:release=300:knee=6[low2];"
        "[mid]acompressor=threshold=0.079:ratio=1.6:attack=20:release=250:knee=6[mid2];"
        "[high]acompressor=threshold=0.050:ratio=1.5:attack=8:release=150:knee=6,stereotools=slev=1.12[high2];"
        "[low2][mid2][high2]amix=inputs=3:normalize=0,"
        "equalizer=f=280:t=q:w=0.9:g=-1.0,highshelf=f=11000:g=1.0,"
        "loudnorm=I=-18:TP=-1.5:LRA=9,"
        "aresample=176400,alimiter=limit=0.84:attack=2:release=80:level=0:latency=1,aresample=44100"
    ),
    "classic": (
        "highpass=f=28:p=2,equalizer=f=285:t=q:w=0.85:g=-1.2,"
        "treble=f=6500:g=0.65,volume=2dB,aresample=88200,"
        "alimiter=limit=0.772:attack=5:release=100:level=0:latency=1,aresample=44100"
    ),
    "off": None,
}

CHUNK = 8192


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, file=sys.stderr, flush=True)


class Studio:
    def __init__(self, ffmpeg, mastering, extra):
        if not STUDIO.exists():
            sys.exit(f"brick-studio missing: {STUDIO}\n  build it with: python3 build.py studio")
        self.engine = subprocess.Popen(
            [str(STUDIO), *extra],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
        command = [ffmpeg, "-nostdin", "-hide_banner", "-loglevel", "error",
                   "-f", "f32le", "-ar", "44100", "-ac", "2", "-i", "pipe:0", "-vn"]
        if MASTERING[mastering]:
            command += ["-af", MASTERING[mastering]]
        command += ["-ar", "44100", "-ac", "2", "-c:a", "libmp3lame", "-b:a", "320k",
                    "-compression_level", "0", "-write_xing", "0", "-id3v2_version", "0", "-flush_packets", "1",
                    "-f", "mp3", "pipe:1"]
        self.encoder = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)
        self.listeners = set()
        self.biome = "?"
        self.playing = False
        threading.Thread(target=self.pump_engine, daemon=True).start()
        threading.Thread(target=self.serve, daemon=True).start()

    def parse_status(self, line):
        line = line.strip()
        if not line.startswith("studio:") or "drift" not in line:
            return
        for mark in ("playing  ", "stopped  "):
            if mark in line:
                self.biome = line.split(mark, 1)[1].strip()
                self.playing = mark.startswith("playing")
                return

    def pump_engine(self):
        # engine PCM -> ffmpeg, and the engine's status lines (biome, playing) parsed out of stderr
        threading.Thread(target=self.pump_status, daemon=True).start()
        try:
            while True:
                data = self.engine.stdout.read(CHUNK)
                if not data:
                    break
                try:
                    self.encoder.stdin.write(data)
                except BrokenPipeError:
                    break
        finally:
            self.stop()

    def pump_status(self):
        for raw in iter(self.engine.stderr.readline, b""):
            self.parse_status(raw.decode("utf-8", "replace"))

    def serve(self):
        for data in iter(lambda: self.encoder.stdout.read(CHUNK), b""):
            for q in list(self.listeners):
                try:
                    q.put_nowait(data)
                except Exception:
                    self.listeners.discard(q)

    def command(self, line):
        line = line.strip()
        if not line:
            return False
        try:
            self.engine.stdin.write((line + "\n").encode())
            self.engine.stdin.flush()
            return True
        except (BrokenPipeError, OSError):
            return False

    def stop(self):
        try:
            self.engine.stdin.close()
        except OSError:
            pass
        for p in (self.engine, self.encoder):
            if p.poll() is None:
                p.terminate()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path == "/radio.mp3":
            return self.mp3()
        if self.path == "/health":
            body = json.dumps({"onAir": self.server.studio.encoder.poll() is None,
                               "biome": self.server.studio.biome,
                               "playing": self.server.studio.playing}).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(404)

    def do_HEAD(self):
        if self.path == "/radio.mp3":
            self.send_response(200)
            self.send_header("Content-Type", "audio/mpeg")
            self.send_header("icy-name", "Mutable Dreams FM (native)")
            self.end_headers()
            return
        self.send_error(404)

    def mp3(self):
        q = Queue(64)
        self.server.studio.listeners.add(q)
        self.send_response(200)
        self.send_header("Content-Type", "audio/mpeg")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.connection.settimeout(4)
            while True:
                try:
                    self.wfile.write(q.get(timeout=45))
                except (Empty, OSError):
                    break
        finally:
            self.server.studio.listeners.discard(q)

    def do_POST(self):
        if self.path == "/command":
            length = int(self.headers.get("Content-Length", "0"))
            line = self.rfile.read(length).decode("utf-8", "replace")
            ok = self.server.studio.command(line)
            body = json.dumps({"ok": ok}).encode()
            self.send_response(200 if ok else 409)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(404)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8790)
    ap.add_argument("--ffmpeg", default="ffmpeg")
    ap.add_argument("--mastering", choices=sorted(MASTERING), default="suite")
    ap.add_argument("extra", nargs="*", help="extra arguments for brick-studio (a SAVE_DIR)")
    args = ap.parse_args()
    studio = Studio(args.ffmpeg, args.mastering, args.extra)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.studio = studio
    log(f"native studio · http://127.0.0.1:{args.port}/radio.mp3 · mastering {args.mastering}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        studio.stop()


if __name__ == "__main__":
    main()
