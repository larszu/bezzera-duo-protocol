#!/usr/bin/env python3
"""Alle Seiten des Displays durchschalten und jede fotografieren.

Das Display haengt an der ESP32-Bridge (../bridge/duo_bridge), eine Kamera
schaut auf das Display. Vorher werden VPs mit Testzahlen gefuellt, damit man
auf den Bildern sieht, welche Variable wo angezeigt wird.

    python3 seiten_foto.py --kameras                     # Kameras auflisten
    python3 seiten_foto.py 0 150 --kamera "PC-LM1E Camera"
    python3 seiten_foto.py 90 90                         # nur Seite 90

Bilder: captures/seiten/seite_NNN.jpg.

Die Kamera laeuft durchgehend als `imagesnap -t` (Zeitraffer) und schreibt
alle 0,5 s ein Bild. Pro Seite wird das erste Bild genommen, das nach dem
Seitenwechsel plus Wartezeit entstanden ist. ffmpeg/avfoundation hat die
USB-Webcam nach Abbruechen blockiert, imagesnap nicht.
Voraussetzung: `brew install imagesnap`.
"""

from __future__ import annotations

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bridge import lies, oeffne  # noqa: E402

# Testzahlen: Wort i von VP 0x0050 bekommt 10 + i, sichtbar als 10..18.
# VP 0x0063 (Firmware Mainboard) = 21 -> "FW: 2.1".
TESTWERTE = ["w 0x0050 10 11 12 13 14 15 16 17 18", "w 0x0063 21"]


def erstes_nach(ordner: str, zeit: float) -> str | None:
    bilder = [(os.path.getmtime(p), p) for p in glob.glob(os.path.join(ordner, "*.jpg"))]
    bilder = [b for b in bilder if b[0] >= zeit]
    return min(bilder)[1] if bilder else None


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("von", type=int, nargs="?", default=0)
    ap.add_argument("bis", type=int, nargs="?", default=150)
    ap.add_argument("--kamera", default="PC-LM1E Camera", help="Geraetename wie bei --kameras")
    ap.add_argument("--kameras", action="store_true", help="nur Kameras auflisten")
    ap.add_argument("--warte", type=float, default=1.0, help="Sekunden nach Seitenwechsel bis zum Foto")
    ap.add_argument("--ziel", default="captures/seiten")
    ap.add_argument("--port")
    a = ap.parse_args(argv)

    if a.kameras:
        subprocess.run(["imagesnap", "-l"])
        return 0

    port = a.port or next(iter(sorted(glob.glob("/dev/cu.usbmodem*"))), None)
    if not port:
        raise SystemExit("Kein /dev/cu.usbmodem* gefunden. Steckt der ESP32?")
    os.makedirs(a.ziel, exist_ok=True)
    roh = tempfile.mkdtemp(prefix="duo_kamera_")
    cam = subprocess.Popen(["imagesnap", "-q", "-d", a.kamera, "-w", "2", "-t", "0.5"], cwd=roh,
                           stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    stumm = open(os.devnull, "w")
    fd = oeffne(port)
    try:
        start = time.time()
        while not glob.glob(os.path.join(roh, "*.jpg")):
            if cam.poll() is not None or time.time() - start > 20:
                raise SystemExit(f"Kamera '{a.kamera}' liefert kein Bild (imagesnap -l zeigt die Namen).")
            time.sleep(0.2)
        for b in TESTWERTE:
            os.write(fd, (b + "\n").encode())
            lies(fd, 0.2, out=stumm)
        for s in range(a.von, a.bis + 1):
            os.write(fd, f"p {s}\n".encode())
            lies(fd, a.warte, out=stumm)
            ab = time.time()
            bild = None
            while bild is None and time.time() - ab < 5:
                time.sleep(0.2)
                bild = erstes_nach(roh, ab)
            ziel = os.path.join(a.ziel, f"seite_{s:03d}.jpg")
            if bild:
                shutil.copyfile(bild, ziel)
                print(f"Seite {s:3d} -> {ziel}", flush=True)
            else:
                print(f"Seite {s:3d}: kein Kamerabild", flush=True)
            for alt in glob.glob(os.path.join(roh, "*.jpg")):  # Zwischenbilder nicht sammeln
                if os.path.getmtime(alt) < ab:
                    os.remove(alt)
    finally:
        os.close(fd)
        cam.terminate()
        try:
            cam.wait(timeout=5)
        except subprocess.TimeoutExpired:
            cam.kill()
        shutil.rmtree(roh, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
