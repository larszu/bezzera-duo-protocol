#!/usr/bin/env python3
"""PulseView/sigrok-Mitschnitt (.sr) -> Sniffer-Log fuer duo_sniff.py.

Fuer den billigen 24-MHz-8-Kanal-Logic-Analyzer (fx2lafw): In PulseView
aufnehmen, als .sr speichern, dann

    python3 sr2log.py mitschnitt.sr > mitschnitt.log
    python3 duo_sniff.py dgus mitschnitt.log

Dekodiert UART 8N1 auf zwei Kanaelen. Standard: D0 -> Kanal A, D1 -> Kanal B,
Baudrate automatisch aus dem kuerzesten Puls. Nur Standardbibliothek.

Optionen:
    --a 0 --b 1        welche Logikkanaele (D0..D7) A und B sind
    --baud 115200      Baudrate fest vorgeben statt automatisch
    --invert           Leitung ist invertiert (Ruhepegel low)
"""

from __future__ import annotations

import argparse
import configparser
import re
import sys
import zipfile
from typing import Iterator

STANDARD_BAUD = (1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600,
                 76800, 115200, 230400, 250000, 460800, 921600)


def _rate(text: str) -> float:
    m = re.match(r"\s*([\d.]+)\s*([kMG]?)Hz", text)
    if not m:
        raise ValueError(f"unbekannte Samplerate: {text!r}")
    return float(m.group(1)) * {"": 1, "k": 1e3, "M": 1e6, "G": 1e9}[m.group(2)]


def lese_sr(pfad: str) -> tuple[float, int, bytes]:
    """(Samplerate in Hz, Bytes je Sample, Rohdaten) aus einer .sr-Datei."""
    with zipfile.ZipFile(pfad) as z:
        meta = configparser.ConfigParser()
        meta.read_string(z.read("metadata").decode())
        dev = next(s for s in meta.sections() if s.startswith("device"))
        rate = _rate(meta[dev]["samplerate"])
        unit = int(meta[dev].get("unitsize", "1"))
        basis = meta[dev].get("capturefile", "logic-1")
        teile = sorted(
            (n for n in z.namelist() if n == basis or n.startswith(basis + "-")),
            key=lambda n: int(n.rsplit("-", 1)[1]) if n != basis else 0,
        )
        daten = b"".join(z.read(n) for n in teile)
    return rate, unit, daten


def kanal(daten: bytes, unit: int, nr: int, invert: bool = False) -> bytes:
    """Ein Kanal als Bytefolge aus 0/1. Kanaele 0-7 liegen im ersten Byte."""
    if unit > 1:
        daten = daten[nr // 8 :: unit]
        nr %= 8
    tabelle = bytes(((b >> nr) & 1) ^ invert for b in range(256))
    return daten.translate(tabelle)


def kuerzester_puls(bits: bytes) -> int | None:
    """Robuster kuerzester Puls in Samples (5-%-Quantil, gegen Stoerspitzen)."""
    kanten = sorted(_alle(bits, b"\x01\x00") + _alle(bits, b"\x00\x01"))
    if len(kanten) < 10:
        return None
    laengen = sorted(b - a for a, b in zip(kanten, kanten[1:]))
    return laengen[len(laengen) // 20]


def schaetze_baud(bits: bytes, rate: float) -> int | None:
    p = kuerzester_puls(bits)
    if not p:
        return None
    roh = rate / p
    return min(STANDARD_BAUD, key=lambda b: abs(b - roh))


def _alle(bits: bytes, muster: bytes) -> list[int]:
    raus, i = [], bits.find(muster)
    while i >= 0:
        raus.append(i + 1)  # Index des ersten Samples nach der Kante
        i = bits.find(muster, i + 1)
    return raus


def uart(bits: bytes, rate: float, baud: int) -> Iterator[tuple[int, int]]:
    """(Sample-Index des Startbits, Byte). 8N1, Ruhepegel 1."""
    bl = rate / baud
    n = len(bits)
    i = bits.find(b"\x01\x00")
    while 0 <= i:
        start = i + 1
        mitte_stop = start + int(9.5 * bl)
        if mitte_stop >= n:
            break
        if bits[start + int(0.5 * bl)] == 0:  # echtes Startbit
            wert = 0
            for k in range(8):
                wert |= bits[start + int((1.5 + k) * bl)] << k
            if bits[mitte_stop]:  # Stoppbit ok
                yield start, wert
            i = bits.find(b"\x01\x00", mitte_stop)
        else:
            i = bits.find(b"\x01\x00", start)


def log_zeilen(ereignisse: list[tuple[int, str, int]], rate: float, baud: int) -> Iterator[str]:
    """Bytes je Richtung zu Frames buendeln (Pause > 3 Zeichenzeiten)."""
    luecke = 30 * rate / baud
    offen: dict[str, tuple[int, int, list[int]]] = {}
    fertig: list[tuple[int, str, list[int]]] = []
    for s, r, b in sorted(ereignisse):
        f = offen.get(r)
        if f and s - f[1] <= luecke:
            f[2].append(b)
            offen[r] = (f[0], s, f[2])
        else:
            if f:
                fertig.append((f[0], r, f[2]))
            offen[r] = (s, s, [b])
    fertig += [(f[0], r, f[2]) for r, f in offen.items()]
    for s, r, bs in sorted(fertig):
        yield f"{int(s * 1000 / rate)} {r} " + " ".join(f"{b:02x}" for b in bs)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sr")
    ap.add_argument("--a", type=int, default=0, help="Logikkanal fuer A (Standard D0)")
    ap.add_argument("--b", type=int, default=1, help="Logikkanal fuer B (Standard D1)")
    ap.add_argument("--baud", type=int, help="fest statt automatisch")
    ap.add_argument("--invert", action="store_true")
    a = ap.parse_args(argv)

    rate, unit, daten = lese_sr(a.sr)
    ereignisse = []
    baud_log = {}
    for name, nr in (("A", a.a), ("B", a.b)):
        bits = kanal(daten, unit, nr, a.invert)
        baud = a.baud or schaetze_baud(bits, rate)
        baud_log[name] = baud
        if not baud:
            print(f"# Kanal {name} (D{nr}): kaum Flanken, uebersprungen", file=sys.stderr)
            continue
        ereignisse += [(s, name, b) for s, b in uart(bits, rate, baud)]
    print(f"# {a.sr}: {rate / 1e6:g} MHz, Baud {baud_log}", file=sys.stderr)
    bauds = [b for b in baud_log.values() if b]
    for z in log_zeilen(ereignisse, rate, bauds[0] if bauds else 115200):
        print(z)
    return 0


if __name__ == "__main__":
    sys.exit(main())
