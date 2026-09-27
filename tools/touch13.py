#!/usr/bin/env python3
"""Touch-Konfiguration des Displays (DGUS 13.bin) dekodieren.

Gelesen mit `libop_lesen.py dump 13` aus dem Display-Flash. Format laut
DWIN DGUS Development Guide V4.3, Kapitel 7: je Taste 16 Byte Kopf

    Pic_ID(2) Xs Ys Xe Ye (8) Pic_Next(2) Pic_On(2) TP_Code(2)

gefolgt von den Funktionsparametern in 16-Byte-Bloecken, die jeweils mit
0xFE beginnen (Eingabe 3, Popup 2, sonst 1; keine bei TP_Code 0xFFxx/0x00xx).
Der Auszug muss fehlerfrei sein (libop_lesen.py liest jeden Block doppelt). TP_Code 0xFExx/0xFDxx = Funktion xx
(FD: geaenderte Werte werden nicht selbst ans Mainboard geschickt, das
Mainboard muss abfragen), 0xFFxx = keine Funktion, nur Seitenwechsel.

    python3 touch13.py flash/lib13.bin                # Tabelle
    python3 touch13.py flash/lib13.bin --json docs/tasten.json
    python3 touch13.py flash/lib13.bin --seite 101
    python3 touch13.py flash/lib13.bin --web bridge/duo_bridge/tasten.h
"""

from __future__ import annotations

import argparse
import json
import sys

FUNKTION = {0x00: "Eingabe", 0x01: "Popup", 0x02: "Plus/Minus", 0x03: "Schieber", 0x04: "Uhr stellen",
            0x05: "Tastencode", 0x06: "Text", 0x07: "Register", 0x08: "Druckzustand", 0x09: "Drehregler"}


def w(b: bytes, i: int) -> int:
    return int.from_bytes(b[i:i + 2], "big")


def dekodiere(daten: bytes) -> list[dict]:
    tasten, i = [], 0
    while i + 16 <= len(daten):
        kopf = daten[i:i + 16]
        if kopf[:2] == b"\xff\xff" or all(x == 0xFF for x in kopf):
            break
        t = {
            "seite": w(kopf, 0), "xs": w(kopf, 2), "ys": w(kopf, 4), "xe": w(kopf, 6), "ye": w(kopf, 8),
            "folgeseite": None if kopf[10] == 0xFF else w(kopf, 10),
            "druckbild": None if kopf[12] == 0xFF else w(kopf, 12),
            "tp_code": w(kopf, 14),
        }
        i += 16
        art = kopf[14]
        fn = kopf[15]
        # Funktionsparameter: 16-Byte-Bloecke, die jeweils mit 0xFE beginnen
        # (Eingabe 3, Popup 2, sonst 1). Nicht nach der Funktionsnummer
        # zaehlen: Im Duo-Projekt gibt es Eintraege mit falscher Nummer
        # (FD00 mit Tastencode-Parametern), die sonst den naechsten
        # Eintrag verschlucken.
        bloecke = b""
        if art in (0xFE, 0xFD):
            while i + 16 <= len(daten) and daten[i] == 0xFE and len(bloecke) < 48:
                bloecke += daten[i:i + 16]
                i += 16
            # Ein Block, dessen FE-Marke im Flash selbst beschaedigt ist (an der
            # Duo auf Seite 9: f3 statt fe): Folgt kein plausibler Eintrag
            # (Seitennummer > 0x0200), gehoert er trotzdem hierher.
            if not bloecke and i + 16 <= len(daten) and w(daten, i) > 0x0200:
                bloecke = daten[i:i + 16]
                i += 16
                t["flash_fehler"] = f"Marke 0x{bloecke[0]:02X} statt 0xFE"
        if art in (0xFE, 0xFD) and bloecke:
            t["funktion"] = FUNKTION.get(fn, f"0x{fn:02X}")
            t["meldet_selbst"] = art == 0xFE
            t["vp"] = w(bloecke, 1)
            if fn == 0x05:  # Tastencode: VP, VP_Mode, Key_Code
                t["vp_mode"] = bloecke[3]
                t["wert"] = w(bloecke, 4)
            elif fn == 0x02:  # Plus/Minus
                t["vp_mode"] = bloecke[3]
                t["richtung"] = "+" if bloecke[4] else "-"
                t["kreislauf"] = bool(bloecke[5])
                t["schritt"] = w(bloecke, 6)
                t["min"] = w(bloecke, 8)
                t["max"] = w(bloecke, 10)
            elif fn == 0x00:  # Eingabe
                t["format"] = bloecke[3]
                t["stellen"] = f"{bloecke[4]}.{bloecke[5]}"
                if len(bloecke) >= 48 and bloecke[33] == 0xFF:
                    t["min"] = int.from_bytes(bloecke[34:38], "big", signed=True)
                    t["max"] = int.from_bytes(bloecke[38:42], "big", signed=True)
            elif fn == 0x01:  # Popup
                t["vp_mode"] = bloecke[3]
                t["popup_seite"] = w(bloecke, 4)
            else:
                t["roh"] = bloecke.hex(" ")
        elif art == 0xFF:
            t["funktion"] = "nur Seitenwechsel"
        else:
            t["funktion"] = f"Tastencode ASCII 0x{t['tp_code']:04X}"
        tasten.append(t)
    return tasten


def zeile(t: dict) -> str:
    ziel = f"→ {t['folgeseite']}" if t["folgeseite"] is not None else ""
    text = f"{t['seite']:4d}  ({t['xs']:3d},{t['ys']:3d})-({t['xe']:3d},{t['ye']:3d}) {ziel:7s} {t['funktion']}"
    if "vp" in t:
        text += f"  VP 0x{t['vp']:04X}"
    if "wert" in t:
        text += f" = {t['wert']}"
    if "richtung" in t:
        text += f" {t['richtung']}{t['schritt']} [{t['min']}..{t['max']}]"
    if "min" in t and "richtung" not in t:
        text += f" [{t['min']}..{t['max']}]"
    if "popup_seite" in t:
        text += f" Popup Seite {t['popup_seite']}"
    if t.get("flash_fehler"):
        text += f"  !! {t['flash_fehler']}"
    if t.get("meldet_selbst"):
        text += "  (meldet selbst)"
    return text


WEB_TYP = {"nur Seitenwechsel": 0, "Tastencode": 5, "Plus/Minus": 2, "Eingabe": 1}


def web_header(tasten: list[dict]) -> str:
    """Kompakte Tabelle fuer die Weboberflaeche der Bridge."""
    zeilen = []
    for t in tasten:
        f = t["funktion"]
        typ = WEB_TYP.get(f, 0 if f.startswith("Tastencode ASCII") else 9)
        r = [t["seite"], t["xs"], t["ys"], t["xe"], t["ye"],
             -1 if t["folgeseite"] is None else t["folgeseite"], typ, t.get("vp", -1)]
        if typ == 5:
            r += [t["wert"]]
        elif typ == 2:
            r += [1 if t["richtung"] == "+" else -1, t["schritt"], t["min"], t["max"], int(t["kreislauf"])]
        elif typ == 1:
            r += [t.get("min", -32768), t.get("max", 32767)]
        zeilen.append(r)
    js = json.dumps(zeilen, separators=(",", ":"))
    return (
        "// Erzeugt von tools/touch13.py --web aus der Touch-Konfiguration (13.bin)\n"
        "// des Display-Flashs. Nicht von Hand bearbeiten.\n"
        "// Je Taste: [seite, xs, ys, xe, ye, folgeseite (-1 = keine), typ, vp, ...]\n"
        "//   typ 0: nur Seitenwechsel, 5: Tastencode [wert], 2: Plus/Minus [richtung,\n"
        "//   schritt, min, max, kreislauf], 1: Zahleneingabe [min, max], 9: sonstiges\n"
        "#pragma once\n"
        f'static const char TASTEN_JS[] PROGMEM = R"JS(const TASTEN={js};)JS";\n'
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("datei")
    ap.add_argument("--seite", type=int)
    ap.add_argument("--json")
    ap.add_argument("--web", help="Header fuer die Bridge-Weboberflaeche schreiben")
    a = ap.parse_args(argv)
    tasten = dekodiere(open(a.datei, "rb").read())
    if a.json:
        json.dump(tasten, open(a.json, "w"), indent=1, ensure_ascii=False)
    if a.web:
        open(a.web, "w").write(web_header(tasten))
    for t in tasten:
        if a.seite is None or t["seite"] == a.seite:
            print(zeile(t))
    print(f"# {len(tasten)} Tasten auf {len({t['seite'] for t in tasten})} Seiten", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
