# Prompt für Claude Code (CLI) am Rechner mit Logic Analyzer

Diesen Text in einem Terminal auf dem Rechner, an dem der Logic Analyzer
hängt, an `claude` übergeben (z. B. `claude` starten und einfügen).

---

Du arbeitest an meinem Projekt, das Protokoll zwischen Mainboard und Display
meiner Espressomaschine **Bezzera Duo DE** zu entschlüsseln. An diesem Rechner
hängt per USB ein **24-MHz-8-Kanal-Logic-Analyzer** (fx2lafw / Saleae-Klon).
Er greift passiv die Leitung Mainboard ↔ Display am Mainboard-Stecker
**CN6 „DISPLAY“** ab: `GND` = schwarze Ader, `D0` = rosa Ader, `D1` = grüne Ader.
Die rote Ader (+5 V) ist nicht angeschlossen.

**Repo:** `https://github.com/larszu/bezzera-duo-protocol`, Branch
`claude/dwin-dgus-display` (falls PR #1 schon gemergt ist: `main`). Lies zuerst
`README.md` komplett. Dort stehen alle Befunde: DWIN-Display DMT32240M035
mit „Mini DGUS“-Protokoll (`5A A5 LEN CMD …`), Mainboard PRO.EL.IND mit
NXP MC9S08PA32, erwartet 115200 Baud, TTL.

Werkzeuge im Repo:
- `tools/sr2log.py` – `.sr` (sigrok) → Log mit UART-Dekodierung, Baudrate automatisch
- `tools/duo_sniff.py` – `dgus` (DGUS-Decoder, VP-Tabelle), `stats`, `diff`
- Tests: `python3 -m unittest discover -s tests`

## Deine Aufgabe

1. **Umgebung:** Repo klonen, Branch auschecken, Tests laufen lassen.
   `sigrok-cli` installieren, falls nicht vorhanden (macOS: `brew install
   sigrok-cli`, Debian/Ubuntu: `sudo apt install sigrok-cli`, Windows:
   Installer von sigrok.org, vorher Treiber mit Zadig). Frag mich, bevor du
   etwas mit `sudo` oder einem Installer ausführst.
2. **Gerät finden:** `sigrok-cli --scan`. Wird der Analyzer nicht gefunden,
   erkläre mir, was zu tun ist (Treiber, USB-Rechte/udev-Regel), statt zu raten.
3. **Sicherheitsabfrage, bevor du irgendetwas aufnimmst:** Frag mich, ob ich
   an rosa und grün gegen schwarz im Ruhezustand eine **positive** Spannung
   (ca. 3,3 V oder 5 V) gemessen habe. Bei „negativ“ oder „weiß nicht“ brichst
   du ab und sagst mir, dass ein MAX3232 nötig ist. Der Analyzer verträgt
   keine negativen Spannungen.
4. **Testaufnahme (5 s):**
   `sigrok-cli -d fx2lafw --config samplerate=2m --time 5s -C D0,D1 -o probe.sr`
   Dann `python3 tools/sr2log.py probe.sr > probe.log` und
   `python3 tools/duo_sniff.py stats probe.log`. Prüf: Baudrate plausibel,
   Rahmen beginnen mit `5a a5`? Wenn nicht: mit `--invert` und anderen
   Baudraten probieren, `stats` ansehen, Ergebnis erklären. Aus der Richtung der
   Rahmen ableiten, welcher Kanal Mainboard → Display ist (das Mainboard
   schreibt regelmäßig `82`-Rahmen, das Display antwortet/meldet mit `83`).
5. **Geführte Aufnahmen.** Führe mich durch Sitzungen von je 30–60 s.
   Starte die Aufnahme im Hintergrund, notiere die Startzeit, und sag mir dann
   Schritt für Schritt, was ich tun soll, zum Beispiel „jetzt Maschine
   einschalten“, „jetzt Tank-Meldung antippen“, „jetzt 20 s nichts tun“. Nach
   jeder Anweisung wartest du auf mein „ok“ und notierst den Zeitpunkt relativ
   zum Aufnahmestart. Füge diese Zeitpunkte als Kommentarzeilen (`# …`) an der
   passenden Stelle in das Log ein. Erweitere dafür `tools/sr2log.py` um eine
   Option `--marks datei` (Zeilen `<ms> <text>`), mit Test. Sitzungen:
   - Kaltstart: Aufnahme starten, dann einschalten (Bootsequenz, Uhr stellen, Seitenaufbau)
   - Tank-Meldung bestätigen (der einzige Touch, der bei mir zuverlässig geht)
   - Ruhe 60 s (welche VPs das Mainboard zyklisch schreibt: Temperaturen, Druck, Uhr)
   - Aufheizen: mehrere Minuten, damit sich Temperatur- und Druck-VPs sichtbar ändern
   - Bezug: Hebel hoch/runter (Chrono, Druck)
   - „press to start“: kurz und lang drücken
6. **Auswerten:** Für jede Sitzung `duo_sniff.py dgus --changes`. Ordne die
   VPs den Anzeigen zu (Brüh-/Dampftemperatur, Druck Gruppe 0–10 bar und
   Dampf 0–2,5 bar, Wasserstand, Uhrzeit/Datum, Chrono, Seiten-IDs,
   Tastencodes). Vergleiche mit dem, was ich auf dem Display sehe, und frag
   mich nach den angezeigten Werten, wenn du eine Umrechnung prüfen willst.
7. **Dokumentieren:** Lege `docs/vp-map.md` an: Tabelle VP-Adresse,
   Richtung, Bedeutung, Kodierung/Umrechnung, Beleg (welche Sitzung,
   welcher Zeitpunkt). Getrennt: sicher / vermutet. Die Roh-`.sr`-Dateien
   kommen nach `captures/` (nur wenn sie unter ~20 MB sind, sonst frag mich).
   README an den neuen Stand anpassen.
8. **Git:** Commits mit klaren Nachrichten auf dem Branch, dann pushen. Nichts
   nach `main` pushen, keine Force-Pushes.

## Grenzen

- **Nur passiv mitschneiden.** Nichts auf die Leitung senden, kein ESP32
  flashen, keine Schreibbefehle an Display oder Mainboard, ohne mich vorher
  ausdrücklich zu fragen.
- An der Maschine liegen 230 V: Wenn etwas umgesteckt werden muss, sag mir,
  dass ich vorher den Netzstecker ziehen soll.
- Wenn du dir bei einer Deutung nicht sicher bist, schreib „vermutet“ und
  nenne den Grund. Lieber nachfragen als raten.
