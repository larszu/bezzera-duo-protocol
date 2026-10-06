#!/usr/bin/env python3
"""Menueseiten per Tastencode oeffnen und mitschreiben, was das Mainboard dabei
in den Variablenspeicher schreibt. So kommen gespeicherte Werte (Bezuege,
Filtertage, Zeitplaene …) heraus, die das Mainboard sonst nie schickt.

Sicherheitsregeln:
  - nur Tastencodes zum Navigieren, nie ±, nie Reset/Kalibrieren/Spuelen
  - OK (VP 0x0002 = 1) nur auf Seiten, deren Werte das Mainboard auf diesem
    Weg selbst geschrieben hat; dann speichert es nur, was es schon hatte
  - Start und Ende auf dem Startbildschirm

    python3 menue_lesen.py -o menue.json
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tasten_scan import Bridge  # noqa: E402
from bridge import finde_port  # noqa: E402

# (Tastencode, erwartete Seite ohne Sprachblock, Beschreibung)
WEG = [
    (68, 16, "Einstellungen 1"),
    (23, 17, "Einstellungen 2"),
    (84, 54, "Wartung: Bezüge"),
    (23, 17, "zurück Einstellungen 2"),
    (24, 18, "Einstellungen 3"),
    (85, 55, "Wasserfilter: Tage"),
    (24, 18, "zurück Einstellungen 3"),
    (25, 19, "Einstellungen 4"),
    (112, 70, "Auto Ein/Aus Sonntag"),
] + [(113 + i, 71 + i, f"Auto Ein/Aus Tag {i + 2}") for i in range(6)]


def schritt(br: Bridge, code: int | None, seite: int | None, warten: float = 1.2) -> dict:
    """Sendet Tastencode + Seite wie ein Finger am Display und sammelt die
    Schreibvorgaenge des Mainboards (B-Zeilen mit 0x82/0x80 03)."""
    br._leeren()
    if code is not None:
        br.sende(f"c6 a5 05 82 00 00 {code >> 8:02x} {code & 0xFF:02x}")
    if seite is not None:
        br.sende(f"c6 a5 04 80 03 {seite >> 8:02x} {seite & 0xFF:02x}")
    vps: dict[str, list[int]] = {}
    seiten = []
    for z in br._zeilen(warten):
        t = z.split(" ", 2)
        if len(t) < 3 or t[1] != "B":
            continue
        try:
            b = bytes.fromhex(t[2])
        except ValueError:
            continue
        if len(b) >= 7 and b[3] == 0x82:
            vp = (b[4] << 8) | b[5]
            if vp != 0x50:  # Status kommt ohnehin laufend
                vps[f"0x{vp:04X}"] = [int.from_bytes(b[i:i + 2], "big") for i in range(6, len(b) - 1, 2)]
        elif len(b) >= 7 and b[3] == 0x80 and b[4] == 0x03:
            seiten.append((b[5] << 8) | b[6])
    return {"vps": vps, "seiten": seiten}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default="menue.json")
    ap.add_argument("--port")
    ap.add_argument("--sprache", type=int, default=100, help="Seitenblock: 0 EN, 100 DE, 200 IT")
    a = ap.parse_args(argv)
    port = a.port or finde_port()
    br = Bridge(port)
    ergebnis = []
    geschrieben: set[str] = set()
    for code, seite, name in WEG:
        r = schritt(br, code, a.sprache + seite)
        geschrieben |= set(r["vps"])
        ergebnis.append({"code": code, "seite": a.sprache + seite, "name": name, **r})
        print(f"{name:28s} Code {code:3d}: Mainboard-Seiten {r['seiten']} schreibt {r['vps']}", flush=True)
    # zurueck: OK auf der Auto-Ein/Aus-Seite und der Liste nur, wenn das Mainboard deren Werte geschrieben hat
    for name, noetig in [("OK Auto Ein/Aus", {"0x0007", "0x000B"}), ("OK Einstellungen 4", {"0x0020"})]:
        if noetig <= geschrieben:
            r = schritt(br, None, None, 0.1)
            br.sende("c6 a5 05 82 00 02 00 01")
            r = schritt(br, None, None, 1.5)
            print(f"{name:28s}: Mainboard-Seiten {r['seiten']} schreibt {r['vps']}", flush=True)
            ergebnis.append({"name": name, **r})
        else:
            print(f"{name}: übersprungen, Mainboard hat {sorted(noetig - geschrieben)} nicht geschrieben", flush=True)
    json.dump(ergebnis, open(a.out, "w"), indent=1)
    print(f"-> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
