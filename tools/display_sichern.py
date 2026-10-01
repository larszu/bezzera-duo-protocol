#!/usr/bin/env python3
"""Display-Flash vollstaendig sichern (nur lesend) ueber die Bridge.

Die Bridge-Firmware liest selbst (USB-Befehl "S", bridge/duo_bridge/sichern.h)
und schickt je 32 Worte eine Zeile "#S …"; dieses Skript sammelt sie ein und
schreibt Dateien. Waehrend der Sicherung steht die Bridge in Modus 1 (sie
beantwortet das Mainboard selbst), danach wieder im vorherigen Modus.

    python3 display_sichern.py suche              # welche Bibliotheken Inhalt haben
    python3 display_sichern.py alles              # alle mit Inhalt + Bildspeicher-Probe
    python3 display_sichern.py lib 14             # eine Bibliothek (256 KB)
    python3 display_sichern.py db 0 65536         # Datenbank/Bildspeicher, Wortadresse, Worte

Ziel: flash/sicherung-<Datum>/ (nicht im Repo), mit manifest.json (SHA-256).
Nur Standardbibliothek.
"""

from __future__ import annotations

import argparse
import glob
import hashlib
import json
import os
import select
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bridge import oeffne  # noqa: E402

LIB_WORTE = 0x20000  # jede Bibliothek 128 KW = 256 KB (DGUS-Handbuch, Register 0x42)
BILD_WORTE = 0x20000  # Platz je Bild im Datenbank-Bereich (Annahme, Probe klaert es)


class Bridge:
    def __init__(self, port: str):
        self.fd = oeffne(port)
        self.rest = b""

    def senden(self, z: str):
        os.write(self.fd, (z + "\n").encode())

    def zeilen(self, sekunden: float):
        ende = time.monotonic() + sekunden
        while time.monotonic() < ende:
            if not select.select([self.fd], [], [], 0.2)[0]:
                continue
            try:
                self.rest += os.read(self.fd, 65536)
            except BlockingIOError:
                continue
            *z, self.rest = self.rest.split(b"\n")
            for x in z:
                yield x.decode("latin-1").strip()

    def zustand(self) -> dict:
        self.senden("?")
        for z in self.zeilen(1.5):
            if z.startswith("# baud="):
                return dict(t.split("=", 1) for t in z[2:].split() if "=" in t)
        return {}

    def lesen(self, befehl: str, worte: int, zeitlimit: float) -> bytes | None:
        """S-Befehl ausfuehren, Bloecke sammeln; None bei Fehler."""
        daten: dict[int, bytes] = {}
        self.senden(befehl)
        start = time.monotonic()
        letzte = start
        for z in self.zeilen(zeitlimit):
            if z.startswith("#S fertig"):
                break
            if z.startswith("#S fehler"):
                print(f"\n  {z}", file=sys.stderr)
                return None
            if z.startswith("#S ") and len(z.split()) == 5:
                _, _, _, adr, hexwerte = z.split()
                daten[int(adr)] = bytes.fromhex(hexwerte)
                if time.monotonic() - letzte > 1:
                    letzte = time.monotonic()
                    kb = len(daten) * 64 / 1024
                    print(f"\r  {kb:7.1f} KB  {kb / (letzte - start):5.1f} KB/s", end="", file=sys.stderr, flush=True)
        print(file=sys.stderr)
        anfang = int(befehl.split()[3]) if befehl.startswith("S lib") else int(befehl.split()[2])
        erwartet = range(anfang, anfang + worte, 32)
        fehlt = [a for a in erwartet if a not in daten]
        if fehlt and len(fehlt) <= 64 and not befehl.endswith(" 32"):
            # einzelne Zeilen gehen auf dem USB-Weg gelegentlich verloren: nachholen
            kopf = " ".join(befehl.split()[:3]) if befehl.startswith("S lib") else "S db"
            for adr in fehlt:
                d = self.lesen(f"{kopf} {adr} 32", 32, 10)
                if d is not None:
                    daten[adr] = d
            fehlt = [a for a in erwartet if a not in daten]
            if not fehlt:
                print("  fehlende Bloecke nachgeholt", file=sys.stderr)
        if fehlt:
            print(f"  {len(fehlt)} Bloecke fehlen (erster bei Wort {fehlt[0]})", file=sys.stderr)
            return None
        return b"".join(daten[a] for a in erwartet)


def leer(d: bytes) -> bool:
    return all(b == 0xFF for b in d) or not any(d)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("was", choices=["suche", "alles", "lib", "db"])
    ap.add_argument("zahlen", nargs="*", type=lambda x: int(x, 0))
    ap.add_argument("--port")
    ap.add_argument("--ziel")
    a = ap.parse_args(argv)
    port = a.port or next(iter(sorted(glob.glob("/dev/cu.usbmodem*"))), None)
    if not port:
        raise SystemExit("Kein /dev/cu.usbmodem* gefunden. Steckt die Bridge?")
    br = Bridge(port)
    z = br.zustand()
    if not z:
        raise SystemExit("Bridge antwortet nicht auf '?'.")
    modus_vorher = z.get("emulation", "0")
    if z.get("ausgabe") == "1":
        br.senden("x")  # Mitschnitt aus, sonst verdraengt er die Daten
    br.senden("e 1")
    time.sleep(0.5)
    ziel = a.ziel or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "flash",
                                  "sicherung-" + time.strftime("%Y%m%d-%H%M"))
    os.makedirs(ziel, exist_ok=True)
    manifest_pfad = os.path.join(ziel, "manifest.json")
    manifest = json.load(open(manifest_pfad)) if os.path.exists(manifest_pfad) else {}

    def speichern(name: str, daten: bytes, info: str):
        open(os.path.join(ziel, name), "wb").write(daten)
        manifest[name] = {"bytes": len(daten), "sha256": hashlib.sha256(daten).hexdigest(), "inhalt": info}
        json.dump(manifest, open(manifest_pfad, "w"), indent=1)
        print(f"  -> {name} ({len(daten)} Byte)")

    try:
        libs: list[int] = []
        if a.was in ("suche", "alles"):
            print("Suche Bibliotheken mit Inhalt (je 64 Byte am Anfang, in der Mitte, am Ende) …")
            for lib in range(128):
                inhalt = False
                for adr in (0, LIB_WORTE // 2, LIB_WORTE - 32):
                    d = br.lesen(f"S lib {lib} {adr} 32", 32, 10)
                    if d and not leer(d):
                        inhalt = True
                        break
                print(f"  Lib {lib:3d}: {'INHALT' if inhalt else 'leer'}", flush=True)
                if inhalt:
                    libs.append(lib)
            json.dump({"libs_mit_inhalt": libs}, open(os.path.join(ziel, "suche.json"), "w"))
        if a.was == "lib":
            libs = a.zahlen
        if a.was in ("lib", "alles"):
            for lib in libs:
                print(f"Lib {lib}: 256 KB lesen …")
                d = br.lesen(f"S lib {lib} 0 {LIB_WORTE}", LIB_WORTE, 3600)
                if d is None:
                    print(f"  Lib {lib} unvollstaendig, nicht gespeichert")
                    continue
                speichern(f"lib_{lib:03d}.bin", d, f"Bibliothek {lib}")
                alt = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "flash", f"lib{lib}.bin")
                if os.path.exists(alt):  # frueherer Teilauszug: muss mit dem Anfang uebereinstimmen
                    v = open(alt, "rb").read()
                    print(f"  Abgleich mit flash/lib{lib}.bin ({len(v)} Byte): "
                          f"{'gleich' if d[:len(v)] == v else 'ABWEICHUNG'}")
        if a.was == "alles":
            # Bildspeicher: Datenbank-Adresse 0 liegt laut Handbuch bei Bild 128.
            # 320x240x2 Byte = 75 KW passen in einen 128-KW-Platz (BILD_WORTE,
            # noch unbestaetigt). Probe der Bilder 128, 201 und 296 (leer).
            for bild in (128, 201, 296):
                adr = (bild - 128) * BILD_WORTE
                print(f"Bild {bild}: Probe 4 KB an Datenbank-Wort {adr:#x} …")
                d = br.lesen(f"S db {adr} 2048", 2048, 120)
                if d is not None:
                    speichern(f"bild_{bild:03d}_probe.bin", d, f"Datenbank ab Wort {adr:#x}, 2048 Worte")
        if a.was == "db":
            adr, worte = a.zahlen[0], a.zahlen[1]
            d = br.lesen(f"S db {adr} {worte}", worte, 3600)
            if d is not None:
                speichern(f"db_{adr:08x}_{worte}.bin", d, f"Datenbank ab Wort {adr:#x}")
    finally:
        br.senden("S")
        br.senden(f"e {modus_vorher}")
        if z.get("ausgabe") == "1":
            br.senden("x")
        print(f"Bridge wieder in Modus {modus_vorher}. Sicherung in {os.path.abspath(ziel)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
