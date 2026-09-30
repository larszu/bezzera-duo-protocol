#!/usr/bin/env python3
"""Variablenspeicher des Displays lesen (nur Lesebefehl 0x83).

Das Mainboard laesst sich nicht abfragen; alles, was es der Oberflaeche
mitteilt, schreibt es in den Variablenspeicher des Displays. Dieses Skript
liest VP-Bereiche in Bloecken zu 32 Worten ueber die Bridge. Im Modus 0
gehen die Antworten nur ins Protokoll, nicht ans Mainboard.

    python3 vp_dump.py                              # 0x0000..0x0FFF -> JSON auf stdout
    python3 vp_dump.py --von 0x0000 --bis 0x0200 -o vp.json
    python3 vp_dump.py --diff alt.json neu.json     # nur geaenderte VPs

Nur Standardbibliothek.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tasten_scan import KOPF, Bridge  # noqa: E402

BLOCK = 32


def lies(br: Bridge, von: int, bis: int) -> dict[str, int]:
    werte: dict[str, int] = {}
    vp = von
    while vp < bis:
        n = min(BLOCK, bis - vp)
        d = None
        for _ in range(4):  # vereinzelt kippen Bytes: zweimal gleich lesen
            a = br.frage(f"{KOPF} 04 83 {vp >> 8:02x} {vp & 0xFF:02x} {n:02x}", f"83{vp:04x}{n:02x}", 0.4)
            b = br.frage(f"{KOPF} 04 83 {vp >> 8:02x} {vp & 0xFF:02x} {n:02x}", f"83{vp:04x}{n:02x}", 0.4)
            if a and a == b and len(a) >= 4 + 2 * n:
                d = a[4 : 4 + 2 * n]
                break
        if d is None:
            print(f"VP 0x{vp:04X}: keine stabile Antwort", file=sys.stderr)
        else:
            for i in range(n):
                werte[f"0x{vp + i:04X}"] = int.from_bytes(d[2 * i : 2 * i + 2], "big")
        vp += n
        print(f"\r0x{vp:04X}", end="", file=sys.stderr, flush=True)
    print(file=sys.stderr)
    return werte


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--von", type=lambda x: int(x, 0), default=0)
    ap.add_argument("--bis", type=lambda x: int(x, 0), default=0x1000)
    ap.add_argument("-o", "--out")
    ap.add_argument("--diff", nargs=2, metavar=("ALT", "NEU"))
    ap.add_argument("--port")
    a = ap.parse_args(argv)
    if a.diff:
        alt, neu = (json.load(open(f)) for f in a.diff)
        for k in sorted(set(alt) | set(neu), key=lambda x: int(x, 16)):
            if alt.get(k) != neu.get(k):
                print(f"{k}: {alt.get(k)} -> {neu.get(k)}")
        return 0
    port = a.port or next(iter(sorted(glob.glob("/dev/cu.usbmodem*"))), None)
    if not port:
        raise SystemExit("Kein /dev/cu.usbmodem* gefunden.")
    werte = lies(Bridge(port), a.von, a.bis)
    text = json.dumps(werte, indent=0)
    if a.out:
        open(a.out, "w").write(text)
        belegt = {k: v for k, v in werte.items() if v}
        print(f"{len(werte)} VPs gelesen, {len(belegt)} ungleich 0 -> {a.out}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
