#!/usr/bin/env python3
"""SD-Karten-Paket (DWIN_SET) mit eigenen Seiten aus einer Display-Sicherung bauen.

Gleiche Logik wie der Seitenbau im Browser (tools/seitenbau/index.html), aber
fuer mehrere Seiten auf einmal und mit Konfiguration:

    python3 sd_paket.py flash/sicherung-20261001-2120 --ziel /Volumes/DWIN \\
        --seite tools/seitenbau/beispiele/bruehkurve.json --config R2=05

Erzeugt DWIN_SET/<seite>.bmp je Seite, 13.bin (Touch, 128 KB = Bibliothek 13),
14.bin (Variablen der Seiten 0-299, 600 KB ab Bibliothek 14, je 64 Seiten
eine Bibliothek) und CONFIG.TXT. Alles hinter der Touch-Tabelle bleibt aus
der Sicherung erhalten. Die Projektdateien sind
seitenbau.json-Dateien (Seitenbau: "Projekt speichern"). Die Ausgangsdateien
kommen vollstaendig aus der Sicherung; nur die genannten Seiten werden ersetzt.
Prueft am Ende, dass alle anderen Seiten byte-gleich geblieben sind.
"""

from __future__ import annotations

import argparse
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 320, 240
SEITE14, EINTRAG = 2048, 32
LIB = 0x20000  # eine Bibliothek: 64 K Worte = 128 KB (gemessen; darueber faengt die Adresse von vorn an)
SEITEN = 300
LIBS14 = (14, 15, 16, 17, 18)  # Seite p liegt in Bibliothek 14 + p // 64
FREI = {96, 97, 98, 99, 196, 197, 198, 199, 296, 297, 298, 299}
SCHRIFT = "/System/Library/Fonts/Helvetica.ttc"
# Konfiguration des Displays 2.2 (Register 0x10-0x1C), R0/R4 nie setzen, RB loescht alles
CONFIG = {"R1": "07", "R2": "04", "R3": "C6", "R6": "40", "R7": "20", "R8": "0A", "RA": "A5"}


def touch_eintraege(d: bytes) -> list[tuple[int, bytes]]:
    """13.bin: 16 Byte Kopf, danach 0-3 Bloecke mit 0xFE am Anfang (wie touch13.py)."""
    out, i = [], 0
    while i + 16 <= len(d):
        if d[i] == 0xFF and d[i + 1] == 0xFF:
            break
        s, art = i, d[i + 14]
        i += 16
        if art in (0xFE, 0xFD):
            v = i
            while i + 16 <= len(d) and d[i] == 0xFE and i - s < 64:
                i += 16
            if i == v and i + 16 <= len(d) and ((d[i] << 8) | d[i + 1]) > 0x0200:
                i += 16  # Block mit beschaedigter FE-Marke
        out.append(((d[s] << 8) | d[s + 1], d[s:i]))
    return out


def w16(a: bytearray, o: int, v: int):
    a[o], a[o + 1] = (v >> 8) & 0xFF, v & 0xFF


def vp_zahl(t) -> int:
    t = str(t).strip()
    return int(t, 16) if t.lower().startswith("0x") else int(t)


def rgb565(h: str) -> int:
    n = int(h[1:], 16)
    return ((n >> 8) & 0xF800) | ((n >> 5) & 0x07E0) | ((n >> 3) & 0x1F)


def touch_fuer(seite: int, e: dict) -> bytes:
    k = bytearray(16)
    w16(k, 0, seite)
    w16(k, 2, e["x"]); w16(k, 4, e["y"]); w16(k, 6, e["x"] + e["b"] - 1); w16(k, 8, e["y"] + e["h"] - 1)
    nach = e.get("ziel") if e["art"] == "seite" else e.get("ziel_danach")
    w16(k, 10, 0xFF00 if nach in (None, "") else int(nach))
    w16(k, 12, 0xFF00)
    if e["art"] == "seite":
        w16(k, 14, 0xFF00)
        return bytes(k)
    w16(k, 14, 0xFD05)  # Tastencode, meldet nicht von selbst (wie die Originaltasten)
    b = bytearray(16)
    b[0] = 0xFE
    w16(b, 1, 0x0000 if e["art"] == "code" else 0x0300)
    w16(b, 4, int(e.get("wert") or 0))
    return bytes(k) + bytes(b)


def vorlagen(lib14: bytes) -> list[bytes]:
    """Zahlenanzeigen (0x10) aus den Originalseiten als Schriftvorlage."""
    out, gesehen = [], set()
    for s in range(SEITEN):
        for i in range(64):
            o = s * SEITE14 + i * EINTRAG
            if lib14[o] != 0x5A:
                break
            if lib14[o + 1] != 0x10:
                continue
            k = (lib14[o + 14], lib14[o + 15])
            if k not in gesehen:
                gesehen.add(k)
                out.append(lib14[o:o + EINTRAG])
    return out


def variable_fuer(e: dict, vl: list[bytes]) -> bytes:
    v = bytearray(32)
    if e["typ"] == "flaeche":
        v[0], v[1] = 0x5A, 0x21
        w16(v, 2, 0xFFFF); w16(v, 4, 0x0008); w16(v, 6, vp_zahl(e["vp"]))
        w16(v, 8, e["x"]); w16(v, 10, e["y"]); w16(v, 12, e["x"] + e["b"] - 1); w16(v, 14, e["y"] + e["h"] - 1)
        return bytes(v)
    v[:] = vl[e.get("vorlage", 0)]
    w16(v, 2, 0xFFFF)
    w16(v, 6, vp_zahl(e["vp_eigen"] if e["vp"] == "eigen" else e["vp"]))
    w16(v, 8, e["x"]); w16(v, 10, e["y"]); w16(v, 12, rgb565(e.get("farbe", "#ffffff")))
    v[0x11], v[0x12] = e.get("stellen", 3), e.get("nachkomma", 0)
    return bytes(v)


def bild(p: dict) -> Image.Image:
    im = Image.new("RGB", (W, H), p.get("bg", "#000000"))
    g = ImageDraw.Draw(im)
    for e in p["el"]:
        if e["typ"] == "form":
            g.rounded_rectangle([e["x"], e["y"], e["x"] + e["b"] - 1, e["y"] + e["h"] - 1], e.get("radius", 0), fill=e["farbe"])
        if e["typ"] == "taste" and e.get("grafik"):
            g.rounded_rectangle([e["x"], e["y"], e["x"] + e["b"] - 1, e["y"] + e["h"] - 1], e.get("radius", 8),
                                fill=e.get("farbe", "#c4561c"))
            if e.get("beschriftung"):
                f = ImageFont.truetype(SCHRIFT, e.get("groesse", 16), index=1)
                g.text((e["x"] + e["b"] / 2, e["y"] + e["h"] / 2), e["beschriftung"], font=f,
                       fill=e.get("textfarbe", "#ffffff"), anchor="mm")
        if e["typ"] == "text":
            f = ImageFont.truetype(SCHRIFT, e["groesse"], index=1 if e.get("fett") else 0)
            g.text((e["x"], e["y"]), e["text"], font=f, fill=e["farbe"])
    return im


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sicherung")
    ap.add_argument("--ziel", required=True, help="Ordner, in den DWIN_SET geschrieben wird")
    ap.add_argument("--seite", action="append", default=[], help="seitenbau.json (mehrfach)")
    ap.add_argument("--config", action="append", default=[], help="z. B. R2=05 (Touch-Piepen aus)")
    a = ap.parse_args(argv)

    lies = lambda n: open(os.path.join(a.sicherung, f"lib_{n:03d}.bin"), "rb").read()[:LIB]
    lib13 = lies(13)
    lib14 = bytearray()
    for n in LIBS14:
        pfad = os.path.join(a.sicherung, f"lib_{n:03d}.bin")
        if not os.path.exists(pfad):
            raise SystemExit(f"lib_{n:03d}.bin fehlt in der Sicherung")
        lib14 += lies(n)
    lib14 = lib14[:SEITEN * SEITE14]
    alt14 = bytes(lib14)
    vl = vorlagen(lib14)
    touch = touch_eintraege(lib13)
    alt_touch = list(touch)

    ziel = os.path.join(a.ziel, "DWIN_SET")
    os.makedirs(ziel, exist_ok=True)
    seiten = []
    for pfad in a.seite:
        p = json.load(open(pfad))
        s = p["seite"]
        ergaenzen = p.get("ergaenzen", False)  # bestehende Seite: anhaengen, Bild bleibt
        if not ergaenzen:
            if s not in FREI:
                raise SystemExit(f"Seite {s} ist keine freie Seite ({sorted(FREI)})")
            if any(t[0] == s for t in alt_touch) or lib14[s * SEITE14] == 0x5A:
                raise SystemExit(f"Seite {s} ist im Display schon belegt")
        seiten.append(s)
        # Touch: Reihenfolge der Originaldatei beibehalten; neue Tasten nach den
        # vorhandenen dieser Seite bzw. vor der ersten hoeheren Seite
        neu = [(s, touch_fuer(s, e)) for e in p["el"] if e["typ"] == "taste"]
        pos = next((i for i, t in enumerate(touch) if t[0] > s), len(touch))
        touch = touch[:pos] + neu + touch[pos:]
        vars_ = [e for e in p["el"] if e["typ"] in ("zahl", "flaeche")]
        start = 0
        if ergaenzen:
            while start < 64 and lib14[s * SEITE14 + start * EINTRAG] == 0x5A:
                start += 1
        else:
            lib14[s * SEITE14:(s + 1) * SEITE14] = b"\xff" * SEITE14
        if start + len(vars_) > 64:
            raise SystemExit("hoechstens 64 Anzeigen je Seite")
        for i, e in enumerate(vars_, start):
            lib14[s * SEITE14 + i * EINTRAG:s * SEITE14 + (i + 1) * EINTRAG] = variable_fuer(e, vl)
        if ergaenzen:
            if lib14[s * SEITE14:s * SEITE14 + start * EINTRAG] != alt14[s * SEITE14:s * SEITE14 + start * EINTRAG]:
                raise SystemExit(f"Seite {s}: vorhandene Anzeigen veraendert, Abbruch")
            print(f"Seite {s} ergaenzt: {len(neu)} Tasten, {len(vars_)} Anzeigen (nach {start} vorhandenen), Bild bleibt")
        else:
            bild(p).save(os.path.join(ziel, f"{s}.bmp"))
            print(f"Seite {s}: {len(neu)} Tasten, {len(vars_)} Anzeigen, Bild {s}.bmp")

    # Pruefen: alles ausser den neuen Seiten unveraendert
    rest = [t for t in touch if t[0] not in seiten]
    if [t[1] for t in rest] != [t[1] for t in alt_touch if t[0] not in seiten]:
        raise SystemExit("Touch-Tabelle: andere Seiten veraendert, Abbruch")
    for s in seiten:  # vorhandene Tasten der Seite: unveraendert und in alter Reihenfolge vorn
        alt = [t[1] for t in alt_touch if t[0] == s]
        if [t[1] for t in touch if t[0] == s][:len(alt)] != alt:
            raise SystemExit(f"Seite {s}: vorhandene Tasten veraendert, Abbruch")
    for s in range(SEITEN):
        if s not in seiten and lib14[s * SEITE14:(s + 1) * SEITE14] != alt14[s * SEITE14:(s + 1) * SEITE14]:
            raise SystemExit(f"Variablen Seite {s} veraendert, Abbruch")

    if seiten:
        tab = b"".join(t[1] for t in touch) + b"\xff" * 16
        ende_alt = sum(len(t[1]) for t in alt_touch) + 16
        if any(b not in (0x00, 0xFF) for b in lib13[ende_alt:len(tab)]):
            raise SystemExit("Hinter der Touch-Tabelle liegen Daten, die die neuen Tasten ueberschreiben wuerden")
        open(os.path.join(ziel, "13.bin"), "wb").write(tab + lib13[len(tab):])
        open(os.path.join(ziel, "14.bin"), "wb").write(bytes(lib14))
    cfg = dict(CONFIG)
    for c in a.config:
        k, v = c.upper().split("=")
        if k in ("R0", "R4", "RB"):
            raise SystemExit(f"{k} nicht setzen (R0/R4 Hardware, RB loescht das Display)")
        cfg[k] = v
    open(os.path.join(ziel, "CONFIG.TXT"), "w", newline="").write("".join(f"{k}={v}\r\n" for k, v in cfg.items()))
    print(f"CONFIG.TXT: {' '.join(f'{k}={v}' for k, v in cfg.items())}")
    print(f"Fertig: {ziel}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
