#!/usr/bin/env python3
"""Auswertung von Mitschnitten der Leitung Mainboard <-> Display (Bezzera Duo/Matrix).

Nur Standardbibliothek. Eingabe ist das Log des ESP32-Sniffers
(`../sniffer/`), eine Zeile je Frame:

    <millis> <A|B> <hex hex hex ...>

`A` und `B` sind die beiden RX-Pins des Sniffers. Welche Seite welcher
Controller ist, weiss man erst nach dem Anklemmen. Zeilen, die mit `#`
beginnen, sind Kommentare, etwa Markierungen wie `# jetzt 93 -> 94 Grad`.

Befehle:

    dgus    DWIN-DGUS-Protokoll des Displays (DMT32240M035, "Mini DGUS"):
            Rahmen 5A A5 <len> <cmd> ..., Registerzugriffe 0x80/0x81,
            Variablen (VP) 0x82/0x83. Zeigt Seitenwechsel, jede VP-Aenderung
            und Touch-Meldungen des Displays. Am wahrscheinlichsten richtig.
    stats   Laengen, Anfangsbytes, druckbarer Anteil, Pruefsummen-Kandidaten
    gicar   ASCII-Registerprotokoll wie beim Gicar 3d5 (Ascaso Baby T):
            r/w, Offset, Laenge, Hexdaten, Summe mod 256. Zeigt jede
            Wertaenderung je Adresse.
    diff    Binaerprotokoll (Lelit-Bianca-artig): welche Byte-Positionen sich
            bei Frames gleicher Laenge und gleichen Kopfes aendern, mit Verlauf

Beispiele:

    python3 duo_sniff.py dgus mitschnitt.log
    python3 duo_sniff.py stats mitschnitt.log
    python3 duo_sniff.py gicar mitschnitt.log
    python3 duo_sniff.py diff mitschnitt.log --dir A --len 18
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from typing import Callable, Iterable, Iterator


@dataclass
class Frame:
    t_ms: int
    richtung: str
    daten: bytes

    @property
    def text(self) -> str:
        return self.daten.decode("latin-1")


# ─── Einlesen ────────────────────────────────────────────────────────────────

_ZEILE = re.compile(r"^\s*(\d+)\s+([A-Za-z])\s+((?:[0-9A-Fa-f]{2}\s*)*)$")


def lese_log(zeilen: Iterable[str]) -> Iterator[Frame | str]:
    """Frames und Kommentare in Dateireihenfolge. Kommentare kommen als str."""
    for z in zeilen:
        z = z.rstrip("\r\n")
        if not z.strip():
            continue
        if z.lstrip().startswith("#"):
            yield z.lstrip()[1:].strip()
            continue
        m = _ZEILE.match(z)
        if not m:
            continue  # Boot-Meldungen des ESP32 u. ae.
        yield Frame(int(m.group(1)), m.group(2).upper(), bytes.fromhex(m.group(3)))


def nur_frames(eintraege: Iterable[Frame | str]) -> list[Frame]:
    return [e for e in eintraege if isinstance(e, Frame)]


# ─── Pruefsummen-Kandidaten ──────────────────────────────────────────────────
#
# Jeder Kandidat bekommt den ganzen Frame und sagt, ob dessen Ende zur
# Pruefsumme ueber den Rest passt. Wer bei (fast) allen Frames einer Richtung
# passt, ist es sehr wahrscheinlich.


def _crc16_modbus(d: bytes) -> int:
    crc = 0xFFFF
    for b in d:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def _ascii_hex_sum(f: bytes) -> bool:
    # Gicar 3d5: letzte zwei Zeichen = Hex der Summe aller Zeichen davor, mod 256
    if len(f) < 3:
        return False
    try:
        erwartet = int(f[-2:].decode("ascii"), 16)
    except ValueError:
        return False
    return sum(f[:-2]) % 256 == erwartet


PRUEFSUMMEN: dict[str, Callable[[bytes], bool]] = {
    "sum8": lambda f: len(f) > 1 and sum(f[:-1]) % 256 == f[-1],
    "sum7 (mod 128)": lambda f: len(f) > 1 and sum(f[:-1]) % 128 == f[-1],
    "sum7 ohne Kopfbyte": lambda f: len(f) > 2 and sum(f[1:-1]) % 128 == f[-1],
    "sum8 ohne Kopfbyte": lambda f: len(f) > 2 and sum(f[1:-1]) % 256 == f[-1],
    "xor8": lambda f: len(f) > 1 and _xor(f[:-1]) == f[-1],
    "xor8 ohne Kopfbyte": lambda f: len(f) > 2 and _xor(f[1:-1]) == f[-1],
    "zweierkomplement": lambda f: len(f) > 1 and (sum(f)) % 256 == 0,
    "crc16 modbus (LE)": lambda f: len(f) > 2
    and _crc16_modbus(f[:-2]) == int.from_bytes(f[-2:], "little"),
    "crc16 modbus (BE)": lambda f: len(f) > 2
    and _crc16_modbus(f[:-2]) == int.from_bytes(f[-2:], "big"),
    "ascii-hex sum8 (Gicar 3d5)": _ascii_hex_sum,
}


def _xor(d: bytes) -> int:
    x = 0
    for b in d:
        x ^= b
    return x


def pruefsummen_treffer(frames: list[Frame]) -> list[tuple[str, float]]:
    """Trefferquote je Kandidat, beste zuerst. Leere Frames zaehlen nicht."""
    daten = [f.daten for f in frames if f.daten]
    if not daten:
        return []
    raus = []
    for name, pruef in PRUEFSUMMEN.items():
        raus.append((name, sum(1 for d in daten if pruef(d)) / len(daten)))
    return sorted(raus, key=lambda x: -x[1])


# ─── stats ───────────────────────────────────────────────────────────────────


def druckbar_anteil(d: bytes) -> float:
    if not d:
        return 0.0
    return sum(1 for b in d if 32 <= b < 127) / len(d)


def cmd_stats(frames: list[Frame], out=sys.stdout) -> None:
    je = defaultdict(list)
    for f in frames:
        je[f.richtung].append(f)
    if not je:
        print("Keine Frames gefunden.", file=out)
        return
    for r in sorted(je):
        fs = je[r]
        dauer = (fs[-1].t_ms - fs[0].t_ms) / 1000 if len(fs) > 1 else 0
        print(f"== Richtung {r}: {len(fs)} Frames ueber {dauer:.1f} s", file=out)
        laengen = Counter(len(f.daten) for f in fs)
        print("  Laengen:   " + ", ".join(f"{l}B x{n}" for l, n in laengen.most_common(8)), file=out)
        koepfe = Counter(f.daten[:1].hex() for f in fs if f.daten)
        print("  Byte 0:    " + ", ".join(f"{k} x{n}" for k, n in koepfe.most_common(8)), file=out)
        dgus = sum(1 for f in fs if DGUS_KOPF in f.daten)
        if dgus:
            print(f"  DGUS:      {dgus} Frames enthalten 5A A5 -> `dgus` benutzen", file=out)
        dr = sum(druckbar_anteil(f.daten) for f in fs) / len(fs)
        print(f"  druckbar:  {dr:.0%}" + ("  -> vermutlich ASCII-Protokoll" if dr > 0.95 else ""), file=out)
        if len(fs) > 1:
            abst = sorted(b.t_ms - a.t_ms for a, b in zip(fs, fs[1:]))
            print(f"  Abstand:   Median {abst[len(abst) // 2]} ms", file=out)
        treffer = [(n, q) for n, q in pruefsummen_treffer(fs) if q > 0.5]
        if treffer:
            print("  Pruefsumme: " + ", ".join(f"{n} {q:.0%}" for n, q in treffer), file=out)
        else:
            print("  Pruefsumme: kein Kandidat passt bei mehr als der Haelfte", file=out)
        for f in fs[:3]:
            print(f"  Beispiel:  {format_frame(f.daten)}", file=out)


def format_frame(d: bytes) -> str:
    hexteil = " ".join(f"{b:02x}" for b in d)
    if druckbar_anteil(d) > 0.9:
        return f"{hexteil}  |{d.decode('latin-1')}|"
    return hexteil


# ─── gicar (ASCII-Registerprotokoll) ─────────────────────────────────────────

_GICAR = re.compile(r"^([rw])([0-9A-Fa-f]{4})([0-9A-Fa-f]{4})(.*)([0-9A-Fa-f]{2})$")


@dataclass
class GicarFrame:
    art: str  # "r" oder "w"
    offset: int
    laenge: int
    nutzdaten: str  # Hex-Text, "OK" bei Schreibquittung, "" bei Anfrage
    pruefsumme_ok: bool


def parse_gicar(text: str) -> GicarFrame | None:
    text = text.strip("\r\n\x00 ")
    m = _GICAR.match(text)
    if not m:
        return None
    art, off, lng, nutz, ps = m.groups()
    ok = sum(text[:-2].encode("latin-1")) % 256 == int(ps, 16)
    return GicarFrame(art, int(off, 16), int(lng, 16), nutz, ok)


def gicar_checksumme(befehl: str) -> str:
    """Haengt die Pruefsumme an, z. B. `r00050010` -> `r00050010xx`."""
    return befehl + f"{sum(befehl.encode('ascii')) % 256:02X}"


def cmd_gicar(eintraege: list[Frame | str], out=sys.stdout) -> dict[int, int]:
    """Baut ein Speicherabbild aus Lese-Antworten und Schreibbefehlen und meldet
    jede Aenderung. Gibt das Abbild zurueck (Adresse -> Bytewert)."""
    speicher: dict[int, int] = {}
    n_ok = n_bad = n_fremd = 0
    for e in eintraege:
        if isinstance(e, str):
            print(f"-- {e}", file=out)
            continue
        g = parse_gicar(e.text)
        if g is None:
            n_fremd += 1
            continue
        if not g.pruefsumme_ok:
            n_bad += 1
            continue
        n_ok += 1
        if g.nutzdaten in ("", "OK") or len(g.nutzdaten) != 2 * g.laenge:
            if g.art == "w" and g.nutzdaten == "OK":
                print(f"{e.t_ms:>9} {e.richtung} Schreiben quittiert @0x{g.offset:04x}+{g.laenge}", file=out)
            continue
        neu = bytes.fromhex(g.nutzdaten)
        if g.art == "w":
            print(
                f"{e.t_ms:>9} {e.richtung} SCHREIBT @0x{g.offset:04x} ({g.offset}): {neu.hex()}"
                f"  u16le={_u16(neu)}",
                file=out,
            )
        for i, b in enumerate(neu):
            adr = g.offset + i
            alt = speicher.get(adr)
            if alt is not None and alt != b:
                print(f"{e.t_ms:>9} {e.richtung} 0x{adr:04x} ({adr:>4}): {alt:02x} -> {b:02x}", file=out)
            speicher[adr] = b
    print(
        f"\n{n_ok} gueltige Gicar-Frames, {n_bad} mit falscher Pruefsumme, "
        f"{n_fremd} passen nicht ins Schema. {len(speicher)} Adressen bekannt.",
        file=out,
    )
    return speicher


def _u16(d: bytes) -> str:
    return str(int.from_bytes(d[:2], "little")) if len(d) >= 2 else "-"


# ─── dgus (DWIN-Display) ─────────────────────────────────────────────────────
#
# Rahmenformat nach ADVi3++ (andrivet/ADVi3pp, Marlin/src/advi3pp/core/dgus.h),
# das dieselbe DWIN-M-Serie ("Mini DGUS") ansteuert:
#
#   5A A5 | LEN | CMD | Parameter | Daten      LEN zaehlt CMD + Parameter + Daten
#
#   80 reg daten...          Register schreiben (03 = PIC_ID, also Seitenwechsel)
#   81 reg n                 Register lesen, Antwort: 81 reg n daten...
#   82 vpH vpL worte...      Variable schreiben (VP = Wortadresse, big-endian)
#   83 vpH vpL n             Variable lesen, Antwort: 83 vpH vpL n worte...
#                            Dieselbe Form schickt das Display ungefragt, wenn
#                            eine Taste mit Tastencode gedrueckt wurde.
#   84 ...                   Kurvendaten
#
# Optional haengt eine CRC-16/Modbus ueber CMD+Daten an (Schalter in R2). Das
# Skript erkennt das je Rahmen selbst. DGUS-II-Displays quittieren 0x82 mit
# `82 4F 4B` ("OK"); Mini DGUS tut das nicht. Beides wird verstanden.

DGUS_KOPF = b"\x5a\xa5"

DGUS_REGISTER = {
    0x00: "Version",
    0x01: "Helligkeit",
    0x02: "Summer",
    0x03: "PIC_ID (Seite)",
    0x05: "Touch-Flag",
    0x06: "Touch-Status",
    0x07: "Touch-Position",
    0x0B: "Touch an/aus",
    0x0C: "Laufzeit",
    0x20: "RTC",
    0x40: "EnLibOP",
    0x41: "LibOPMode",
    0x42: "LibID",
    0x43: "LibAddress",
    0x46: "LibOP-VP",
    0x48: "LibOP-Laenge",
    0x4F: "Tastencode",
    0xEE: "Reset",
}


@dataclass
class DgusFrame:
    t_ms: int
    richtung: str
    cmd: int
    nutz: bytes  # Parameter + Daten, ohne CRC
    crc: bool


class DgusParser:
    """Zerlegt den Bytestrom je Richtung in DGUS-Rahmen. Der Sniffer trennt
    nach Pausen, nicht nach Protokoll: Ein Sniffer-Frame kann mehrere Rahmen
    oder einen halben enthalten. Deshalb wird je Richtung aneinandergehaengt
    und am Kopf neu synchronisiert."""

    def __init__(self, kopf: bytes = DGUS_KOPF):
        self.kopf = kopf
        self.puffer: dict[str, bytearray] = defaultdict(bytearray)
        self.zeit: dict[str, int] = {}

    def feed(self, f: Frame) -> list[DgusFrame]:
        raus = []
        b = self.puffer[f.richtung]
        if not b:
            self.zeit[f.richtung] = f.t_ms
        b.extend(f.daten)
        while True:
            i = b.find(self.kopf)
            if i < 0:
                del b[: max(0, len(b) - len(self.kopf) + 1)]
                break
            if i:
                del b[:i]
            if len(b) < 4:
                break
            n = b[2]
            if len(b) < 3 + n:
                break
            koerper = bytes(b[3 : 3 + n])
            del b[: 3 + n]
            mit_crc = len(koerper) >= 3 and _crc16_modbus(koerper[:-2]) in (
                int.from_bytes(koerper[-2:], "little"),
                int.from_bytes(koerper[-2:], "big"),
            )
            if mit_crc:
                koerper = koerper[:-2]
            if koerper:
                raus.append(DgusFrame(self.zeit.get(f.richtung, f.t_ms), f.richtung, koerper[0], koerper[1:], mit_crc))
            self.zeit[f.richtung] = f.t_ms
        return raus


def dgus_rahmen(frames: Iterable[Frame], kopf: bytes = DGUS_KOPF) -> Iterator[DgusFrame]:
    p = DgusParser(kopf)
    for f in frames:
        yield from p.feed(f)


def dgus_beschreibung(r: DgusFrame) -> tuple[str, int | None, bytes]:
    """(Text, VP-Adresse oder None, Nutzdaten der Variable)."""
    c, p = r.cmd, r.nutz
    if c == 0x80 and p:
        name = DGUS_REGISTER.get(p[0], f"R{p[0]:02X}")
        rest = p[1:]
        if p[0] == 0x03 and len(rest) >= 2:
            return f"Register schreiben {name} = Seite {int.from_bytes(rest[:2], 'big')}", None, b""
        return f"Register schreiben {name} = {rest.hex(' ')}", None, b""
    if c == 0x81 and len(p) >= 2:
        name = DGUS_REGISTER.get(p[0], f"R{p[0]:02X}")
        if len(p) == 2:
            return f"Register lesen {name}, {p[1]} Byte", None, b""
        rest = p[2:]
        if p[0] == 0x03 and len(rest) >= 2:
            return f"Register {name} ist Seite {int.from_bytes(rest[:2], 'big')}", None, b""
        return f"Register {name} = {rest.hex(' ')}", None, b""
    if c == 0x82:
        if p == b"OK":
            return "Quittung OK", None, b""
        if len(p) >= 2:
            vp = int.from_bytes(p[:2], "big")
            return f"VP schreiben 0x{vp:04X} = {_worte(p[2:])}", vp, p[2:]
    if c == 0x83 and len(p) >= 3:
        vp = int.from_bytes(p[:2], "big")
        if len(p) == 3:
            return f"VP lesen 0x{vp:04X}, {p[2]} Wort", None, b""
        return f"VP 0x{vp:04X} meldet {_worte(p[3:])}", vp, p[3:]
    if c == 0x84:
        return f"Kurvendaten {p.hex(' ')}", None, b""
    return f"unbekannt cmd=0x{c:02X} {p.hex(' ')}", None, b""


def _worte(d: bytes) -> str:
    if len(d) % 2 == 0 and d:
        w = [int.from_bytes(d[i : i + 2], "big") for i in range(0, len(d), 2)]
        text = " ".join(f"{x:04X}({x})" for x in w[:8])
        if len(w) > 8:
            text += f" ... ({len(w)} Worte)"
    else:
        text = d.hex(" ")
    if druckbar_anteil(d.rstrip(b"\x00\xff")) > 0.9 and len(d) >= 4:
        text += f'  "{d.rstrip(bytes([0, 0xFF])).decode("latin-1")}"'
    return text


def cmd_dgus(eintraege: list[Frame | str], nur_aenderungen: bool = False, out=sys.stdout) -> dict[int, bytes]:
    """Gibt jeden DGUS-Rahmen aus (bei `nur_aenderungen` nur neue VP-Werte,
    Seitenwechsel und Touch-Meldungen) und am Ende eine Tabelle aller VPs.
    Rueckgabe: VP-Adresse -> letzter Wert."""
    vps: dict[int, bytes] = {}
    zaehler: Counter = Counter()
    von: dict[int, set] = defaultdict(set)
    n = n_crc = 0
    parser = DgusParser()
    for e in eintraege:
        if isinstance(e, str):
            print(f"-- {e}", file=out)
            continue
        for r in parser.feed(e):
            n += 1
            n_crc += r.crc
            text, vp, daten = dgus_beschreibung(r)
            neu = True
            if vp is not None:
                zaehler[vp] += 1
                von[vp].add(r.richtung)
                neu = vps.get(vp) != daten
                vps[vp] = daten
            # Ein einzelnes Wort per 0x83 ist meist ein Tastendruck: jedes Mal zeigen.
            taste = r.cmd == 0x83 and len(daten) == 2
            if not nur_aenderungen or r.cmd == 0x80 or (vp is not None and (neu or taste)):
                print(f"{r.t_ms:>9} {r.richtung} {text}", file=out)

    print(f"\n{n} DGUS-Rahmen, davon {n_crc} mit CRC.", file=out)
    if not n:
        print("Kein 5A A5 gefunden. Baudrate/Invertierung pruefen oder `stats` ansehen.", file=out)
    if vps:
        print("\nVP      Anzahl  Richtung  letzter Wert", file=out)
        for vp in sorted(vps):
            print(f"0x{vp:04X}  {zaehler[vp]:>6}  {','.join(sorted(von[vp])):<8}  {_worte(vps[vp])}", file=out)
    return vps


# ─── diff (Binaerprotokoll) ──────────────────────────────────────────────────


def cmd_diff(
    eintraege: list[Frame | str], richtung: str, laenge: int | None, kopf: str | None, out=sys.stdout
) -> dict[int, list[tuple[int, int]]]:
    """Welche Positionen aendern sich, und wann? Gibt je veraenderlicher
    Position die Liste (t_ms, neuer Wert) zurueck."""
    fs = [e for e in eintraege if isinstance(e, Frame) and e.richtung == richtung]
    if laenge is None and fs:
        laenge = Counter(len(f.daten) for f in fs).most_common(1)[0][0]
    fs = [f for f in fs if len(f.daten) == laenge]
    if kopf:
        k = bytes.fromhex(kopf)
        fs = [f for f in fs if f.daten.startswith(k)]
    if not fs:
        print("Keine passenden Frames.", file=out)
        return {}
    werte = defaultdict(Counter)
    for f in fs:
        for i, b in enumerate(f.daten):
            werte[i][b] += 1
    konstant = [i for i in range(laenge) if len(werte[i]) == 1]
    variabel = [i for i in range(laenge) if len(werte[i]) > 1]
    print(f"{len(fs)} Frames, {laenge} Byte, Richtung {richtung}", file=out)
    print("konstant: " + " ".join(f"[{i}]={next(iter(werte[i])):02x}" for i in konstant), file=out)
    print("variabel: " + " ".join(f"[{i}]({len(werte[i])})" for i in variabel), file=out)

    verlauf: dict[int, list[tuple[int, int]]] = defaultdict(list)
    vorher = None
    for e in eintraege:
        if isinstance(e, str):
            print(f"-- {e}", file=out)
            continue
        if e not in fs:
            continue
        if vorher is not None:
            for i in variabel:
                if e.daten[i] != vorher[i]:
                    verlauf[i].append((e.t_ms, e.daten[i]))
                    print(f"{e.t_ms:>9} [{i:>2}] {vorher[i]:02x} -> {e.daten[i]:02x}", file=out)
        vorher = e.daten
    return dict(verlauf)


# ─── CLI ─────────────────────────────────────────────────────────────────────


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("dgus", "stats", "gicar", "diff"):
        p = sub.add_parser(name)
        p.add_argument("log", help="Sniffer-Log, '-' fuer stdin")
        if name == "dgus":
            p.add_argument("--changes", action="store_true", help="nur neue Werte, Seitenwechsel, Tasten")
        if name == "diff":
            p.add_argument("--dir", default="A", help="Richtung A oder B")
            p.add_argument("--len", type=int, help="nur Frames dieser Laenge (Standard: haeufigste)")
            p.add_argument("--head", help="nur Frames mit diesem Anfang, Hex, z. B. 81")
    a = ap.parse_args(argv)

    quelle = sys.stdin if a.log == "-" else open(a.log, encoding="utf-8", errors="replace")
    with quelle:
        eintraege = list(lese_log(quelle))

    if a.cmd == "dgus":
        cmd_dgus(eintraege, a.changes)
    elif a.cmd == "stats":
        cmd_stats(nur_frames(eintraege))
    elif a.cmd == "gicar":
        cmd_gicar(eintraege)
    else:
        cmd_diff(eintraege, a.dir.upper(), a.len, a.head)
    return 0


if __name__ == "__main__":
    sys.exit(main())
