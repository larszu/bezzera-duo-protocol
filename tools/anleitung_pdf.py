#!/usr/bin/env python3
"""Bauanleitung als PDF aus den Markdown-Dokumenten (fuer jedes Release).

    python3 tools/anleitung_pdf.py            # build/anleitung-de.pdf und build/anleitung-en.pdf
    python3 tools/anleitung_pdf.py --sprache de

Braucht Python-Markdown (pip install markdown) und Chrome/Chromium (headless).
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

import markdown

WURZEL = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Kapitel: (Titel, Quelle). Quelle "README.md#Ueberschrift" = nur dieser Abschnitt.
KAPITEL = {
    "de": [
        ("Firmware auf den ESP32", "docs/flashen.md"),
        ("Verkabelung", "README.md#### Verkabelung"),
        ("Pinbelegung beider Boards", "docs/pinbelegung.md"),
        ("Ersatz-Display zeigt keine Werte", "README.md### 🩹 Ersatz-Display zeigt keine Werte?"),
        ("Fernzugriff", "docs/fernzugriff.md"),
    ],
    "en": [
        ("Flashing the ESP32", "docs/flashen.en.md"),
        ("Wiring", "README.en.md#### Wiring"),
        ("Pinout of both boards", "docs/pinbelegung.en.md"),
        ("Replacement display shows no values", "README.en.md### 🩹 Replacement display shows no values?"),
        ("Remote access", "docs/fernzugriff.en.md"),
    ],
}
TITEL = {"de": "Bezzera Duo Bridge – Bauanleitung", "en": "Bezzera Duo Bridge – Build guide"}
INHALT = {"de": "Inhalt", "en": "Contents"}

CSS = """
@page { size: A4; margin: 18mm 16mm; }
body { font: 10.5pt/1.45 -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; color: #1b1f24; }
h1 { font-size: 20pt; margin: 0 0 4mm; }
h2 { font-size: 15pt; border-bottom: 1px solid #ccd; padding-bottom: 2mm; margin-top: 0; page-break-before: always; }
h3 { font-size: 12pt; margin-top: 6mm; }
h4 { font-size: 10.5pt; }
table { border-collapse: collapse; width: 100%; margin: 3mm 0; font-size: 9.5pt; }
th, td { border: 1px solid #ccd; padding: 1.5mm 2mm; text-align: left; vertical-align: top; }
th { background: #eef1f5; }
code { font: 9pt Menlo, Consolas, monospace; background: #f1f3f6; padding: 0 1mm; }
pre { background: #f1f3f6; padding: 3mm; white-space: pre-wrap; font-size: 8.5pt; page-break-inside: avoid; }
pre code { background: none; padding: 0; }
img { max-width: 100%; }
a { color: #1f5d8c; text-decoration: none; }
.titel { page-break-after: always; }
.titel p { color: #555; }
.inhalt li { margin: 1.5mm 0; }
"""


def abschnitt(text: str, ueberschrift: str) -> str:
    """Nur den Abschnitt ab der Ueberschrift bis zur naechsten gleicher oder hoeherer Ebene."""
    ebene = len(ueberschrift) - len(ueberschrift.lstrip("#"))
    zeilen = text.splitlines()
    for i, z in enumerate(zeilen):
        if z.strip() == ueberschrift.strip():
            ende = len(zeilen)
            for j in range(i + 1, len(zeilen)):
                m = re.match(r"^(#+)\s", zeilen[j])
                if m and len(m.group(1)) <= ebene:
                    ende = j
                    break
            return "\n".join(zeilen[i + 1:ende])
    raise SystemExit(f"Abschnitt nicht gefunden: {ueberschrift}")


def kapitel_text(quelle: str) -> tuple[str, str]:
    datei, _, ueberschrift = quelle.partition("#")
    text = open(os.path.join(WURZEL, datei), encoding="utf-8").read()
    if ueberschrift:
        text = abschnitt(text, ueberschrift)
    else:
        text = re.sub(r"^# .*\n", "", text, count=1, flags=re.M)  # eigener Titel kommt vom Kapitel
    text = re.sub(r'<p align="right">.*?</p>\s*', "", text, flags=re.S)  # Sprachumschalter
    text = re.sub(r"<details>\s*<summary>(.*?)</summary>", r"\n**\1**\n", text, flags=re.S)
    text = text.replace("</details>", "")
    # Ueberschriften eine Ebene tiefer, damit das Kapitel h2 bleibt
    text = re.sub(r"^(#{1,5}) ", lambda m: "#" + m.group(1) + " ", text, flags=re.M)
    return text, os.path.dirname(os.path.join(WURZEL, datei))


def bauen(sprache: str, ziel: str, version: str) -> None:
    teile = [f'<div class="titel"><h1>{TITEL[sprache]}</h1><p>{version} · github.com/larszu/bezzera-duo-protocol</p>',
             f'<h3>{INHALT[sprache]}</h3><ol class="inhalt">']
    teile += [f"<li>{t}</li>" for t, _ in KAPITEL[sprache]]
    teile.append("</ol></div>")
    for titel, quelle in KAPITEL[sprache]:
        text, basis = kapitel_text(quelle)
        html = markdown.markdown(text, extensions=["tables", "fenced_code"])
        # relative Bilder und Links auf Dateien ins Repo aufloesen
        html = re.sub(r'src="(?!https?:|file:|data:)([^"]+)"',
                      lambda m: f'src="file://{os.path.normpath(os.path.join(basis, m.group(1)))}"', html)
        if quelle.startswith("README") and ("Verkabelung" in quelle or "Wiring" in quelle):
            html += f'<p><img src="file://{os.path.join(WURZEL, "docs/breadboard_hybrid.png")}"></p>'
        teile.append(f"<h2>{titel}</h2>{html}")
    seite = f'<!doctype html><html lang="{sprache}"><meta charset="utf-8"><style>{CSS}</style><body>{"".join(teile)}</body></html>'
    with tempfile.NamedTemporaryFile("w", suffix=".html", delete=False, encoding="utf-8") as f:
        f.write(seite)
        html_pfad = f.name
    chrome = next((c for c in (os.environ.get("CHROME"), "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
                               shutil.which("google-chrome"), shutil.which("chromium"), shutil.which("chromium-browser"))
                   if c and os.path.exists(c)), None)
    if not chrome:
        raise SystemExit("Chrome/Chromium nicht gefunden (CHROME=… setzen)")
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    subprocess.run([chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
                    "--allow-file-access-from-files", f"--print-to-pdf={ziel}", f"file://{html_pfad}"],
                   check=True, capture_output=True)
    os.unlink(html_pfad)
    print(f"{ziel} ({os.path.getsize(ziel) // 1024} KB)")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sprache", choices=["de", "en"], action="append")
    ap.add_argument("--version", default=os.environ.get("GITHUB_REF_NAME", "main"))
    ap.add_argument("--ziel", default=os.path.join(WURZEL, "build"))
    a = ap.parse_args(argv)
    for s in a.sprache or ["de", "en"]:
        bauen(s, os.path.join(a.ziel, f"anleitung-{s}.pdf"), a.version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
