#!/usr/bin/env python3
"""Weboberflaeche der Bridge lokal am Rechner, Verbindung zum ESP32 ueber USB.

Liefert dieselbe Oberflaeche wie der ESP32 selbst (web_ui.h, tasten.h), holt
den Zustand aber per USB-Befehl "j" und schickt Befehle per USB. So braucht
der Rechner nicht ins WLAN des ESP32.

    python3 web_lokal.py              # dann http://localhost:8080
    python3 web_lokal.py --port /dev/cu.usbmodem2101 --http 8080
    python3 web_lokal.py --demo       # ohne ESP32: simulierte Maschine und Waage

Nur Standardbibliothek.
"""

from __future__ import annotations

import argparse
import glob
import http.server
import os
import select
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bridge import oeffne  # noqa: E402

BRIDGE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bridge", "duo_bridge")


def aus_header(datei: str, anfang: str, ende: str) -> bytes:
    s = open(os.path.join(BRIDGE, datei), encoding="utf-8").read()
    return s.split(anfang, 1)[1].rsplit(ende, 1)[0].encode("utf-8")


class Usb:
    """Haelt die serielle Verbindung offen (jedes Oeffnen startet den ESP32
    neu) und sammelt die JSON-Zeilen der Antworten auf "j"."""

    def __init__(self, port: str):
        self.fd = oeffne(port)
        self.sperre = threading.Lock()
        self.letzte_json = b"{}"
        self.neu = threading.Event()
        self.letzte_z = b""
        self.neu_z = threading.Event()
        threading.Thread(target=self._lesen, daemon=True).start()

    def _lesen(self):
        rest = b""
        while True:
            if not select.select([self.fd], [], [], 1)[0]:
                continue
            try:
                rest += os.read(self.fd, 65536)
            except (BlockingIOError, OSError):
                time.sleep(0.05)
                continue
            *zeilen, rest = rest.split(b"\n")
            for z in zeilen:
                if z.startswith(b"#J "):
                    self.letzte_json = z[3:].strip()
                    self.neu.set()
                elif z.startswith(b"#Z "):
                    self.letzte_z = z[3:].strip()
                    self.neu_z.set()

    def senden(self, zeile: str):
        with self.sperre:
            os.write(self.fd, (zeile.strip() + "\n").encode())

    def status(self, seit: str) -> bytes:
        self.neu.clear()
        self.senden(f"j {seit}")
        self.neu.wait(1.5)
        return self.letzte_json

    def zusatz(self, pfad: str, rumpf: str = "") -> bytes:
        """Zusatz-API ueber den USB-Befehl "z" (Antwortzeile "#Z ...")."""
        with self.z_sperre:
            self.neu_z.clear()
            self.senden(f"z {pfad} {rumpf}".strip())
            self.neu_z.wait(2)
            return self.letzte_z

    z_sperre = threading.Lock()


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    ap.add_argument("--http", type=int, default=8080)
    ap.add_argument("--demo", action="store_true", help="ohne ESP32, simulierte Maschine und Waage")
    a = ap.parse_args(argv)
    if a.demo:
        from web_demo import Demo  # noqa: E402

        usb = Demo()
        port = "Demo"
    else:
        port = a.port or next(iter(sorted(glob.glob("/dev/cu.usbmodem*"))), None)
        if not port:
            raise SystemExit("Kein /dev/cu.usbmodem* gefunden. Steckt der ESP32?")
        usb = Usb(port)
    seite = aus_header("web_ui.h", 'R"HTML(', ')HTML"')
    tasten = aus_header("tasten.h", 'R"JS(', ')JS"')

    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def _antwort(self, typ: str, daten: bytes):
            self.send_response(200)
            self.send_header("Content-Type", typ)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(daten)

        def do_GET(self):
            if self.path.startswith("/api/") and not self.path.startswith(("/api/status", "/api/cmd")):
                a = usb.zusatz(self.path)
                self._antwort("application/json" if a.startswith(b"{") else "text/plain", a)
            elif self.path.startswith("/api/status"):
                seit = self.path.split("seit=", 1)[1] if "seit=" in self.path else "0"
                self._antwort("application/json", usb.status(seit))
            elif self.path.startswith("/tasten.js"):
                self._antwort("text/javascript; charset=utf-8", tasten)
            else:
                self._antwort("text/html; charset=utf-8", seite)

        def do_POST(self):
            n = int(self.headers.get("Content-Length", 0))
            zeile = self.rfile.read(n).decode("utf-8", "replace")
            if self.path.startswith("/api/") and self.path != "/api/cmd":
                a = usb.zusatz(self.path, zeile.replace("\n", " "))
                self._antwort("text/plain", a)
            elif zeile[:1] in "pwodmse":
                usb.senden(zeile)
                self._antwort("text/plain", b"ok")
            else:
                self.send_response(400)
                self.end_headers()

    srv = http.server.ThreadingHTTPServer(("127.0.0.1", a.http), H)
    print(f"Oberflaeche: http://localhost:{a.http}  (ESP32 an {port}, Strg+C beendet)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
