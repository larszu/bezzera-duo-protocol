#!/usr/bin/env python3
"""Live-Anzeige: was das Mainboard gerade ans Display schickt.

Liest ueber sigrok-cli direkt vom Logic Analyzer (fx2lafw), dekodiert UART
auf beiden Kanaelen, zerlegt die DGUS-Rahmen und zeigt im Terminal Seite,
VP-Werte, Uhr und die letzten Aenderungen. Der Analyzer hoert nur zu.

    python3 duo_live.py                      # live vom Analyzer
    python3 duo_live.py --replay boot.sr     # Mitschnitt in Echtzeit abspielen
    python3 duo_live.py --replay boot.sr --speed 0   # so schnell wie moeglich

Anschluss wie im README: CH1 (D0) = gelb/TXD vom Display, CH2 (D1) =
weiss/RXD zum Display. Kanal A ist also das Display, B das Mainboard.
Nur Standardbibliothek plus sigrok-cli.
"""

from __future__ import annotations

import argparse
import os
import pty
import re
import subprocess
import sys
import time
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from duo_sniff import DGUS_REGISTER, DgusParser, Frame, dgus_beschreibung  # noqa: E402

ZEILE = re.compile(r"^(\d+)-\d+ uart-([12]): ([0-9A-Fa-f]{2})$")
# uart-1 haengt an D0, uart-2 an D1 (Reihenfolge der -P-Optionen unten).
RICHTUNG = {"1": "A", "2": "B"}
NAME = {"A": "Display", "B": "Mainboard"}

# Bedeutung der Worte in VP 0x0050. Wort 3 und 4 sind Temperaturen in °C,
# am Display abgelesen 2026-09-27. Welcher Kessel welcher ist, steht noch aus.
VP50_NAMEN = {3: "Temperatur °C", 4: "Temperatur °C"}


def sigrok_befehl(replay: str | None, samplerate: str, baud: int) -> list[str]:
    quelle = ["-i", replay] if replay else ["-d", "fx2lafw", "--config", f"samplerate={samplerate}", "--continuous"]
    return [
        "sigrok-cli", *quelle, "-C", "D0,D1",
        "-P", f"uart:rx=D0:baudrate={baud}",
        "-P", f"uart:rx=D1:baudrate={baud}",
        "-A", "uart=rx-data", "--protocol-decoder-samplenum",
    ]


def lies_zeilen(befehl: list[str]):
    """sigrok-cli puffert voll, wenn stdout eine Pipe ist. Ueber ein Pseudo-
    terminal kommt jede Zeile sofort."""
    master, slave = pty.openpty()
    # stdin ebenfalls aufs Pseudoterminal: sigrok-cli beendet --continuous bei EOF auf stdin
    proc = subprocess.Popen(befehl, stdin=slave, stdout=slave, stderr=subprocess.STDOUT, close_fds=True)
    os.close(slave)
    rest = b""
    try:
        while True:
            try:
                block = os.read(master, 65536)
            except OSError:
                break
            if not block:
                break
            rest += block
            *zeilen, rest = rest.split(b"\n")
            for z in zeilen:
                yield z.decode("latin-1").strip()
    finally:
        proc.terminate()
        os.close(master)


class Zustand:
    def __init__(self):
        self.seite: int | None = None
        self.seiten_seit = 0.0
        self.vps: dict[int, bytes] = {}
        self.vp_zeit: dict[int, float] = {}
        self.register: dict[int, bytes] = {}
        self.rahmen = {"A": 0, "B": 0}
        self.letzter = {"A": 0.0, "B": 0.0}
        self.log: deque[str] = deque(maxlen=14)
        self.t = 0.0

    def verarbeite(self, r, t: float) -> None:
        self.t = t
        self.rahmen[r.richtung] += 1
        self.letzter[r.richtung] = t
        text, vp, daten = dgus_beschreibung(r)
        if r.cmd == 0x80 and r.nutz:
            reg, wert = r.nutz[0], r.nutz[1:]
            if reg == 0x03 and len(wert) >= 2:
                seite = int.from_bytes(wert[:2], "big")
                if seite != self.seite:
                    self.seite, self.seiten_seit = seite, t
                    self.eintrag(t, r.richtung, f"Seite {seite}")
                return
            if self.register.get(reg) != wert:
                self.eintrag(t, r.richtung, text)
            self.register[reg] = wert
        elif r.cmd == 0x81 and len(r.nutz) > 2:
            self.register[r.nutz[0]] = r.nutz[2:]
        if vp is not None:
            alt = self.vps.get(vp)
            if alt != daten:
                self.vp_zeit[vp] = t
                if alt is not None and len(alt) == len(daten) > 2:
                    # Mehrwortige VP: nur die geaenderten Worte nennen
                    # Temperaturrauschen (±2) nicht ins Protokoll
                    diff = [
                        f"W{i} {x}→{y}"
                        for i, (x, y) in enumerate(zip(worte(alt), worte(daten)))
                        if x != y and not (vp == 0x0050 and i in VP50_NAMEN and abs(x - y) <= 2)
                    ]
                    if diff:
                        self.eintrag(t, r.richtung, f"0x{vp:04X} " + ", ".join(diff))
                elif alt is not None or r.richtung == "A":
                    self.eintrag(t, r.richtung, text)
            self.vps[vp] = daten

    def eintrag(self, t: float, richtung: str, text: str) -> None:
        self.log.append(f"{t:8.1f} s  {NAME[richtung]:<9} {text}")


def worte(d: bytes) -> list[int]:
    return [int.from_bytes(d[i : i + 2], "big") for i in range(0, len(d) - 1, 2)]


def rtc_text(d: bytes | None) -> str:
    # Mini DGUS: JJ MM TT Wochentag hh mm ss, alles BCD
    if not d or len(d) < 7:
        return "–"
    b = d[-7:] if d[0] == 0x5A and len(d) == 8 else d[:7]
    return f"20{b[0]:02x}-{b[1]:02x}-{b[2]:02x} {b[4]:02x}:{b[5]:02x}:{b[6]:02x}"


def zeichne(z: Zustand, quelle: str) -> str:
    FETT, GRUEN, GRAU, AUS = "\x1b[1m", "\x1b[32m", "\x1b[2m", "\x1b[0m"
    frisch = lambda vp: z.t - z.vp_zeit.get(vp, -99) < 1.0  # noqa: E731
    o = ["\x1b[H\x1b[2J", f"{FETT}Bezzera Duo – Display-Leitung live{AUS}   {GRAU}{quelle}, t = {z.t:.1f} s{AUS}", ""]
    o.append(f"  Seite (PIC_ID)   {FETT}{z.seite if z.seite is not None else '–'}{AUS}"
             + (f"   {GRAU}seit {z.t - z.seiten_seit:.1f} s{AUS}" if z.seite is not None else ""))
    o.append(f"  Uhr im Display   {rtc_text(z.register.get(0x20) or z.register.get(0x1F))}")
    for r in ("B", "A"):
        still = z.t - z.letzter[r]
        warn = f"  \x1b[31mstill seit {still:.1f} s{AUS}" if z.rahmen[r] and still > 2 else ""
        o.append(f"  Rahmen {NAME[r]:<9} {z.rahmen[r]:>6}{warn}")
    o.append("")

    d50 = z.vps.get(0x0050)
    if d50:
        o.append(f"{FETT}  VP 0x0050 (Mainboard → Display, Status){AUS}")
        for i, w in enumerate(worte(d50)):
            name = VP50_NAMEN.get(i, "")
            farbe = GRUEN if frisch(0x0050) else ""
            o.append(f"    Wort {i}  {farbe}{w:>6}{AUS}  {GRAU}0x{w:04X}  {name}{AUS}")
        o.append("")

    o.append(f"{FETT}  Weitere VPs{AUS}")
    for vp in sorted(z.vps):
        if vp == 0x0050:
            continue
        w = " ".join(str(x) for x in worte(z.vps[vp])) or z.vps[vp].hex(" ")
        hinweis = {0x0000: "  Status/Seite? 1 vor OK, 5 danach", 0x0001: "  Tastencode vom Touch?"}.get(vp, "")
        farbe = GRUEN if frisch(vp) else ""
        o.append(f"    0x{vp:04X}  {farbe}{w:<20}{AUS}{GRAU}{hinweis}{AUS}")
    regs = [f"{DGUS_REGISTER.get(k, f'R{k:02X}')}={v.hex(' ')}" for k, v in sorted(z.register.items()) if k not in (0x03, 0x1F, 0x20)]
    if regs:
        o.append(f"    Register  {GRAU}{'  '.join(regs)}{AUS}")
    o.append("")
    o.append(f"{FETT}  Letzte Aenderungen{AUS}")
    o.extend(f"  {GRAU}{e}{AUS}" for e in z.log)
    o.append(f"\n{GRAU}  Strg+C beendet.{AUS}")
    return "\n".join(o)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--replay", help=".sr-Datei abspielen statt live aufzunehmen")
    ap.add_argument("--speed", type=float, default=1.0, help="Abspieltempo bei --replay, 0 = ohne Pause")
    ap.add_argument("--samplerate", default="2m", help="Abtastrate live (Standard 2m)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--kopf", default="c6a5", help="DGUS-Rahmenkopf, Hex (Duo: c6a5)")
    a = ap.parse_args(argv)

    rate = None
    if a.replay:
        # Abtastrate der Datei fuer die Zeitachse
        import configparser
        import zipfile
        with zipfile.ZipFile(a.replay) as zf:
            cp = configparser.ConfigParser()
            cp.read_string(zf.read("metadata").decode())
        rate = sigrok_rate(cp.get("device 1", "samplerate"))
    else:
        rate = sigrok_rate(a.samplerate)
        # Der fx2lafw-Klon uebernimmt eine geaenderte Abtastrate erst beim
        # naechsten Lauf. Ein kurzer Vorlauf setzt sie, sonst stimmt die
        # Zeitachse nicht und UART dekodiert Unsinn.
        subprocess.run(["sigrok-cli", "-d", "fx2lafw", "--config", f"samplerate={a.samplerate}", "--samples", "1000"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL, check=False)

    kopf = bytes.fromhex(a.kopf)
    parser = {"A": DgusParser(kopf), "B": DgusParser(kopf)}
    z = Zustand()
    quelle = a.replay or "fx2lafw"
    start = time.monotonic()
    gezeichnet = 0.0
    try:
        for zeile in lies_zeilen(sigrok_befehl(a.replay, a.samplerate, a.baud)):
            m = ZEILE.match(zeile)
            if not m:
                if zeile and not zeile.startswith("uart"):
                    z.log.append(f"sigrok: {zeile}")
                continue
            t = int(m.group(1)) / rate
            richtung = RICHTUNG[m.group(2)]
            if a.replay and a.speed > 0:
                warte = t / a.speed - (time.monotonic() - start)
                if warte > 0:
                    time.sleep(warte)
            for r in parser[richtung].feed(Frame(int(t * 1000), richtung, bytes.fromhex(m.group(3)))):
                z.verarbeite(r, t)
            jetzt = time.monotonic()
            if jetzt - gezeichnet > 0.2:
                sys.stdout.write(zeichne(z, quelle))
                sys.stdout.flush()
                gezeichnet = jetzt
    except KeyboardInterrupt:
        pass
    sys.stdout.write(zeichne(z, quelle) + "\n")
    return 0


def sigrok_rate(text: str) -> float:
    m = re.match(r"^\s*([\d.]+)\s*([kKmMgG]?)", text)
    if not m:
        raise ValueError(f"Abtastrate nicht lesbar: {text}")
    return float(m.group(1)) * {"": 1, "k": 1e3, "m": 1e6, "g": 1e9}[m.group(2).lower()]


if __name__ == "__main__":
    sys.exit(main())
