#!/usr/bin/env python3
"""Tastencodes des Displays finden, ohne Touch und ohne Maschine.

DGUS loest die Touch-Taste mit Tastencode k aus, wenn man k in Register
0x4F schreibt. Das Skript probiert auf jeder angegebenen Seite alle Codes
durch und notiert, was die Taste im Display bewirkt: Seitenwechsel und/oder
ein Wert in VP 0x0000/0x0001 (die beiden VPs, die das Mainboard abfragt).

Das Display haengt allein an der ESP32-Bridge. Das Mainboard ist NICHT
angeschlossen, es kann also nichts an der Maschine ausgeloest werden.

    python3 tasten_scan.py 1                 # Seite 1, Codes 1..255
    python3 tasten_scan.py 0 1 3 --bis 64
    python3 tasten_scan.py 0-95 --json docs/tasten.json

Ausgabe je Treffer: Seite, Code, neue Seite, VP 0x0000, VP 0x0001.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import select
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bridge import oeffne, finde_port# noqa: E402

KOPF = "c6 a5"


class Bridge:
    def __init__(self, port: str):
        self.fd = oeffne(port)
        self.rest = b""
        os.write(self.fd, b"x\n")  # Mitschnitt aus: nur Antworten interessieren
        time.sleep(0.3)
        self._leeren()
        os.write(self.fd, b"x\n")  # wieder an (Antworten kommen als A-Zeilen)
        time.sleep(0.1)
        self._leeren()

    def _leeren(self):
        while select.select([self.fd], [], [], 0.05)[0]:
            try:
                os.read(self.fd, 65536)
            except BlockingIOError:
                break
        self.rest = b""

    def _zeilen(self, sekunden: float):
        ende = time.monotonic() + sekunden
        while (r := ende - time.monotonic()) > 0:
            if not select.select([self.fd], [], [], r)[0]:
                continue
            try:
                self.rest += os.read(self.fd, 65536)
            except BlockingIOError:
                continue
            *z, self.rest = self.rest.split(b"\n")
            for x in z:
                yield x.decode("latin-1").strip()

    def sende(self, hexbytes: str):
        os.write(self.fd, f"d {hexbytes}\n".encode())

    def frage(self, hexbytes: str, antwort_start: str, timeout: float = 0.3) -> bytes | None:
        """Sendet an das Display und wartet auf eine A-Zeile, die mit
        antwort_start (hex, ohne Kopf/Laenge) beginnt."""
        self._leeren()
        self.sende(hexbytes)
        for z in self._zeilen(timeout):
            teile = z.split(" ", 2)
            if len(teile) == 3 and teile[1] == "A":
                b = bytes.fromhex(teile[2])
                if b[3:].hex().startswith(antwort_start.replace(" ", "")):
                    return b[3:]
        return None

    def seite(self) -> int | None:
        r = self.frage(f"{KOPF} 03 81 03 02", "810302")
        return int.from_bytes(r[3:5], "big") if r and len(r) >= 5 else None

    def vp01(self) -> tuple[int, int] | None:
        r = self.frage(f"{KOPF} 04 83 00 00 02", "83000002")
        if r and len(r) >= 8:
            return int.from_bytes(r[4:6], "big"), int.from_bytes(r[6:8], "big")
        return None


def seitenliste(args: list[str]) -> list[int]:
    out = []
    for a in args:
        if "-" in a:
            v, b = a.split("-")
            out += range(int(v), int(b) + 1)
        else:
            out.append(int(a))
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("seiten", nargs="+", help="Seiten, z. B. 1 3 20-26")
    ap.add_argument("--von", type=int, default=1)
    ap.add_argument("--bis", type=int, default=255)
    ap.add_argument("--json", help="Treffer zusaetzlich als JSON speichern (wird ergaenzt)")
    ap.add_argument("--port")
    a = ap.parse_args(argv)
    port = a.port or finde_port()
    if not port:
        raise SystemExit("Kein /dev/cu.usbmodem* gefunden.")
    br = Bridge(port)

    treffer = {}
    if a.json and os.path.exists(a.json):
        treffer = json.load(open(a.json))
    for s in seitenliste(a.seiten):
        gefunden = []
        for k in range(a.von, a.bis + 1):
            br.sende(f"{KOPF} 07 82 00 00 00 00 00 00")  # VP 0x0000/0x0001 = 0
            br.sende(f"{KOPF} 04 80 03 {s >> 8:02x} {s & 0xFF:02x}")
            time.sleep(0.03)
            br.sende(f"{KOPF} 03 80 4f {k:02x}")
            time.sleep(0.08)
            neu = br.seite()
            vp = br.vp01()
            if neu is None or vp is None:
                print(f"Seite {s} Code {k}: keine Antwort", flush=True)
                continue
            if neu != s or vp != (0, 0):
                eintrag = {"code": k, "seite_neu": neu, "vp0000": vp[0], "vp0001": vp[1]}
                gefunden.append(eintrag)
                print(f"Seite {s:3d} Code {k:3d} (0x{k:02X}) -> Seite {neu:3d}  VP0000={vp[0]}  VP0001={vp[1]}",
                      flush=True)
        print(f"# Seite {s}: {len(gefunden)} Tasten", flush=True)
        treffer[str(s)] = gefunden
        if a.json:
            with open(a.json, "w") as f:
                json.dump(treffer, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
