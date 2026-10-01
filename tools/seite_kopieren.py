#!/usr/bin/env python3
"""Hintergrundbild einer Seite auf einen leeren Bildplatz kopieren (SCHREIBT ins Display-Flash).

Weg ohne SDK: Datenbank-Register 0x56-0x5F. Lesen (0xA0) holt 32 Worte nach
VP 0x1000, Schreiben (0x50) schreibt VP 0x1000 zurueck in den Flash. Laut
DGUS-Handbuch liegt Datenbank-Adresse 0 auf Bild 128; Bilder 0-127 sind so
nicht erreichbar. Deshalb geht der Test nur zwischen Bildern >= 128,
z. B. 201 (vorhandene Seite) -> 296 (leer).

Ablauf und Schutz:
  1. Es muss eine Sicherung aus display_sichern.py mit Probe von Quelle und
     Ziel vorliegen (--sicherung); das Ziel muss dort leer sein.
  2. Ohne --wirklich wird nur gelesen und verglichen (Trockenlauf).
  3. Mit --wirklich: Block fuer Block schreiben, sofort zuruecklesen, beim
     ersten Unterschied abbrechen.
Danach Seite 296 per "c6 a5 04 80 03 01 28" aufrufen und anschauen.

    python3 seite_kopieren.py 201 296 --sicherung flash/sicherung-20261001-1200
    python3 seite_kopieren.py 201 296 --sicherung … --wirklich
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from display_sichern import BILD_WORTE, Bridge, leer  # noqa: E402

BILD_NUTZ = 320 * 240  # Worte RGB565 je Bild


def schreiben(br: Bridge, adr: int, daten: bytes):
    werte = " ".join(f"{b:02x}" for b in daten)
    br.senden(f"d c6 a5 {3 + len(daten):02x} 82 10 00 {werte}")
    time.sleep(0.02)
    a = adr.to_bytes(4, "big").hex(" ")
    br.senden(f"d c6 a5 0c 80 56 5a 50 {a} 10 00 00 {len(daten) // 2:02x}")
    time.sleep(0.15)  # Flash schreiben; das Lesen danach wartet ohnehin auf 0x56 = 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("quelle", type=int)
    ap.add_argument("ziel", type=int)
    ap.add_argument("--sicherung", required=True)
    ap.add_argument("--wirklich", action="store_true")
    ap.add_argument("--port")
    a = ap.parse_args(argv)
    if min(a.quelle, a.ziel) < 128:
        raise SystemExit("Nur Bilder ab 128 sind ueber die Datenbank erreichbar.")
    probe = os.path.join(a.sicherung, f"bild_{a.ziel:03d}_probe.bin")
    if not os.path.exists(probe) or not os.path.exists(os.path.join(a.sicherung, "manifest.json")):
        raise SystemExit(f"Keine Sicherung mit {probe}. Erst: display_sichern.py alles")
    if not leer(open(probe, "rb").read()):
        raise SystemExit(f"Bild {a.ziel} ist laut Sicherung nicht leer. Abbruch.")
    port = a.port or next(iter(sorted(glob.glob("/dev/cu.usbmodem*"))), None)
    if not port:
        raise SystemExit("Kein /dev/cu.usbmodem* gefunden. Steckt die Bridge?")
    br = Bridge(port)
    z = br.zustand()
    modus_vorher = z.get("emulation", "0")
    if z.get("ausgabe") == "1":
        br.senden("x")
    br.senden("e 1")
    q0, z0 = (a.quelle - 128) * BILD_WORTE, (a.ziel - 128) * BILD_WORTE
    try:
        print(f"Bild {a.quelle} lesen ({BILD_NUTZ * 2 // 1024} KB) …")
        quelle = br.lesen(f"S db {q0} {BILD_NUTZ}", BILD_NUTZ, 3600)
        if quelle is None or leer(quelle):
            raise SystemExit("Quelle nicht lesbar oder leer: Annahme zur Bildlage stimmt nicht. Nichts geschrieben.")
        os.makedirs(a.sicherung, exist_ok=True)
        open(os.path.join(a.sicherung, f"bild_{a.quelle:03d}.bin"), "wb").write(quelle)
        print(f"Bild {a.ziel} lesen …")
        ziel = br.lesen(f"S db {z0} {BILD_NUTZ}", BILD_NUTZ, 3600)
        if ziel is None or not leer(ziel):
            raise SystemExit(f"Bild {a.ziel} nicht vollstaendig leer. Nichts geschrieben.")
        if not a.wirklich:
            print("Trockenlauf ok: Quelle hat Inhalt, Ziel ist leer. Mit --wirklich schreiben.")
            return 0
        log = []
        for i in range(0, BILD_NUTZ, 32):
            block = quelle[2 * i:2 * i + 64]
            if leer(block) and all(b == block[0] for b in block):
                continue
            schreiben(br, z0 + i, block)
            zurueck = br.lesen(f"S db {z0 + i} 32", 32, 10)
            if zurueck != block:
                raise SystemExit(f"Abweichung bei Wort {z0 + i:#x}: Schreiben greift nicht wie erwartet. Abbruch.")
            log.append(z0 + i)
            if len(log) % 100 == 0:
                print(f"\r  {i * 100 // BILD_NUTZ:3d} %", end="", flush=True)
        json.dump({"quelle": a.quelle, "ziel": a.ziel, "bloecke": len(log)},
                  open(os.path.join(a.sicherung, f"kopie_{a.quelle}_{a.ziel}.json"), "w"))
        print(f"\nFertig. Bild {a.ziel} anzeigen: d c6 a5 04 80 03 {a.ziel >> 8:02x} {a.ziel & 0xFF:02x}")
    finally:
        br.senden("S")
        br.senden(f"e {modus_vorher}")
        if z.get("ausgabe") == "1":
            br.senden("x")
    return 0


if __name__ == "__main__":
    sys.exit(main())
