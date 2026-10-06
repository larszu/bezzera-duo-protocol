#!/usr/bin/env python3
"""Flash-Bereiche des Displays ueber DGUS-LibOP LESEN (nur Modus 0xA0).

Register 0x40 En_Lib_OP = 0x5A, 0x41 Lib_OP_Mode = 0xA0 (Flash -> Variablen-
speicher), 0x42 Lib_ID, 0x43 Adresse (3 Byte, Worte), 0x46 VP, 0x48 Laenge.
Der Modus 0x50 wuerde den Flash SCHREIBEN und das Display beschaedigen
koennen; dieses Skript kennt ihn nicht. Quelle: DWIN DGUS Development
Guide V4.3, Abschnitt DGUS-Register.

Das Display haengt an der ESP32-Bridge. Der Zwischenspeicher ist VP 0x1000
(der Variablenspeicher dieses Displays hat 14-Bit-Adressen, hoehere spiegeln).

    python3 libop_lesen.py suche                   # welche Lib-IDs haben Inhalt
    python3 libop_lesen.py dump 13 --worte 4096 -o touch13.bin
"""

from __future__ import annotations

import argparse
import glob
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tasten_scan import KOPF, Bridge  # noqa: E402
from bridge import finde_port  # noqa: E402

PUFFER_VP = 0x1000  # 0x2000.. ist ein interner Puffer des Displays (UART), dort nicht
BLOCK = 32  # Worte je Vorgang: das Display verarbeitet hoechstens 32 Worte je Rahmen
LESEN = 0xA0  # einziger erlaubter Modus


def lies_block(br: Bridge, lib: int, adresse: int, worte: int) -> bytes | None:
    assert LESEN == 0xA0
    a = adresse.to_bytes(3, "big")
    vp = PUFFER_VP.to_bytes(2, "big")
    n = worte.to_bytes(2, "big")
    # Puffer vorher mit Muster fuellen: so sieht man, ob gelesen wurde
    muster = " ".join(["be ef"] * worte)
    br.sende(f"{KOPF} {3 + 2 * worte:02x} 82 {PUFFER_VP >> 8:02x} {PUFFER_VP & 0xFF:02x} {muster}")
    time.sleep(0.02)
    br.sende(f"{KOPF} 0c 80 40 5a {LESEN:02x} {lib:02x} {a.hex(' ')} {vp.hex(' ')} {n.hex(' ')}")
    for _ in range(20):  # warten, bis En_Lib_OP wieder 0 ist
        time.sleep(0.02)
        r = br.frage(f"{KOPF} 03 81 40 01", "814001")
        if r and len(r) >= 4 and r[3] == 0:
            break
    r = br.frage(f"{KOPF} 04 83 {PUFFER_VP >> 8:02x} {PUFFER_VP & 0xFF:02x} {worte:02x}", "83")
    if not r or len(r) < 4 + 2 * worte:
        return None
    return r[4 : 4 + 2 * worte]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("modus", choices=["suche", "dump"])
    ap.add_argument("lib", nargs="?", type=lambda x: int(x, 0))
    ap.add_argument("--worte", type=int, default=4096)
    ap.add_argument("--ab", type=lambda x: int(x, 0), default=0, help="Startadresse in Worten")
    ap.add_argument("-o", "--out")
    ap.add_argument("--port")
    a = ap.parse_args(argv)
    port = a.port or finde_port()
    if not port:
        raise SystemExit("Kein /dev/cu.usbmodem* gefunden.")
    br = Bridge(port)

    if a.modus == "suche":
        for lib in list(range(0, 0x80)):
            d = lies_block(br, lib, 0, 16)
            if d is None:
                print(f"Lib {lib:3d} (0x{lib:02X}): keine Antwort", flush=True)
                continue
            art = "Muster (nicht gelesen)" if d == bytes.fromhex("beef") * 16 else (
                "leer 00" if not any(d) else "leer FF" if all(b == 0xFF for b in d) else "INHALT")
            print(f"Lib {lib:3d} (0x{lib:02X}): {art:22s} {d[:16].hex(' ')}", flush=True)
        return 0

    if a.lib is None:
        raise SystemExit("dump braucht eine Lib-ID")
    out = bytearray()
    adr = a.ab
    while len(out) < 2 * a.worte:
        w = min(BLOCK, a.worte - len(out) // 2)
        # Jeder Block wird so oft gelesen, bis zwei Lesungen gleich sind:
        # Auf der Leitung kippen vereinzelt Bytes (fe -> f3, ff -> be).
        d, vorher = None, None
        for _ in range(6):
            neu = lies_block(br, a.lib, adr, w)
            if neu is not None and b"\xbe\xef\xbe\xef" in neu:
                neu = None  # Fuellmuster steht noch da: LibOP lief nicht
            if neu is not None and neu == vorher:
                d = neu
                break
            vorher = neu
        if d is None:
            print(f"Abbruch bei Wort 0x{adr:06X}: keine Antwort", file=sys.stderr)
            break
        out += d
        adr += w
        print(f"\r{len(out) // 2}/{a.worte} Worte", end="", file=sys.stderr, flush=True)
    print(file=sys.stderr)
    if a.out:
        open(a.out, "wb").write(out)
        print(f"{len(out)} Byte -> {a.out}")
    else:
        for i in range(0, len(out), 32):
            print(f"{a.ab * 2 + i:06x}  {out[i:i + 32].hex(' ')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
