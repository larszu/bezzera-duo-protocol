# Bezzera Duo / Matrix: Protokoll zwischen Mainboard und Display

Arbeitsstand, um die Verbindung zwischen Mainboard (7661047.xx) und dem
3,5"-Touchdisplay (5963201.xx) der Bezzera Duo DE/MN und der baugleichen
Matrix zu entschlüsseln. Ziel ist dasselbe wie beim Reddit-Projekt von
*Vivid-Ad-2039*: ein ESP32, der Register liest und schreibt.

## 1. Was feststeht

**Das Display ist ein DWIN DMT32240M035_07WTZ4**, darauf ein Touch-Overlay
CDQ9439-3.5-A (abgelesen an einer Duo DE, 2026-09-25). DWIN-Displays sind
fertige serielle HMI-Module: Bilder, Schriften und die Lage der Touchflächen
liegen im Flash des Displays. Das Display selbst hat keine Maschinenlogik. Es
zeigt Variablen an und meldet Touch-Eingaben über UART. Der Touch ist
kapazitiv und hat einen eigenen Controller am Flachkabel. Für das Protokoll
zum Mainboard spielt er keine Rolle.

Daraus folgt sehr wahrscheinlich:

- **Das Mainboard ist der Master und hält die ganze Logik**: PID, Sollwerte,
  Passwörter, Chrono. Das passt dazu, dass ein Mainboard-Reset die Passwörter
  auf 1901/1906 setzt.
- **Die „Register“ von Vivid-Ad-2039 sind DGUS-Variablen (VPs).** Das Mainboard
  schreibt Werte in VP-Adressen des Displays, das Display meldet Eingaben als
  VP-Wert zurück. Das ist tatsächlich „quite simple“.
- **„MCU Passthrough“** heißt dann: Der ESP32 sitzt zwischen Mainboard und
  Display und reicht die DGUS-Rahmen durch. Zum Testen kann er das Durchreichen
  abschalten.

### Das Protokoll: DWIN „Mini DGUS“

Die M-Serie von DWIN (DMT…M…) läuft mit „Mini DGUS“. Dieselbe Serie steckt im
Drucker Wanhao i3 Plus (DMT48270M043). Dafür hat
[ADVi3++](https://github.com/andrivet/ADVi3pp) das Protokoll vollständig
implementiert (`Marlin/src/advi3pp/core/dgus.h`):

```
5A A5 | LEN | CMD | Parameter | Daten        LEN = Bytes ab CMD (inkl. CRC, falls an)

80 reg daten...        Register schreiben     80 03 00 05  -> auf Seite 5 wechseln
81 reg n               Register lesen         Antwort: 81 reg n daten...
82 vpH vpL worte...    Variable schreiben     82 10 00 03 A7 -> VP 0x1000 = 935
83 vpH vpL n           Variable lesen         Antwort: 83 vpH vpL n worte...
                       Dieselbe Form sendet das Display ungefragt bei Tasten
                       mit Tastencode:        83 20 00 01 00 02
84 ...                 Kurvendaten
```

- Worte sind big-endian. Eine Temperatur steht wahrscheinlich als Wert × 10 in
  einem Wort (93,5 °C = 935 = `03 A7`). Das ist nicht bestätigt, bei DGUS aber üblich.
- Wichtige Register: `03` aktuelle Seite (PIC_ID), `4F` Tastencode, `05`–`07`
  Touch, `00` Firmwareversion.
- Rahmenkopf (`5A A5`), Baudrate und CRC sind im Display einstellbar
  (`DWIN_SET/CONFIG.txt`, Register R1/R2/R3/RA). ADVi3++ nutzt 115200 Baud,
  Kopf 5A A5, ohne CRC. Bezzera kann das anders eingestellt haben. Das Skript
  erkennt eine CRC-16/Modbus pro Rahmen selbst.
- Neuere DGUS-II-Displays (T5/T5L) nutzen dasselbe Rahmenformat, wechseln die
  Seite aber über VP `0x0084` und quittieren 0x82 mit `4F 4B`. Beides versteht
  das Skript auch.

### Was es sonst gibt

| Suche (2026-09-24) | Ergebnis |
|---|---|
| GitHub-Code `setwarmup bezzera`, `"/set/espresso"` | 0 Treffer |
| GitHub-Nutzer `Vivid-Ad-2039` | existiert nicht |
| GitHub-Repos `bezzera` | nur [BB005-Mühlentimer](https://github.com/hellgelino/bezzera-bb005-digital-timer) und [bezzi-tank](https://github.com/hcrohland/bezzi-tank) (Ultraschall-Füllstand, fasst die Elektronik nicht an) |
| `bezzera duo esp32` | [brewos-io/firmware](https://github.com/brewos-io/firmware): ersetzt die ganze Steuerung, Duo/Matrix ungetestet, kein Protokoll |
| Reddit-Profil | von hier aus nicht erreichbar. Selbst ansehen: `old.reddit.com/user/Vivid-Ad-2039` |
| `DMT32240M035_07WTZ4` | kein Datenblatt zu genau dieser Variante gefunden. `_07` und `Z4` sind vermutlich kundenspezifisch. Die Serie DMT32240M035_03W ist als „Smart UART, TTL“ beschrieben |

### Frühere Hypothese: Gicar-Protokolle

Händler nennen die Steuerung „Gicar PID controller“
([Whole Latte Love](https://www.wholelattelove.com/products/bezzera-matrix-mn-dual-boiler-espresso-machine)).
Für Gicar sind zwei Protokolle dokumentiert: das ASCII-Registerprotokoll der
Ascaso Baby T ([antondlr/gicar-serial](https://github.com/antondlr/gicar-serial):
`r/w` + Offset + Länge, Summe mod 256, 115200 Baud) und das Binärprotokoll
Display ↔ Platine der Lelit Bianca
([lelit-bianca-protocol](https://github.com/magnusnordlander/lelit-bianca-protocol):
9600 Baud invertiert, Summe mod 128). **Für die Displayleitung der Duo ist das
mit dem DWIN-Fund unwahrscheinlich.** Die Hauptplatine kann trotzdem von Gicar
sein. Das Gicar-ASCII-Protokoll könnte an einer anderen Schnittstelle des
Mainboards hängen, etwa an einem Service- oder Bluetooth-Anschluss. Das Skript
kann deshalb beides weiterhin erkennen (`gicar`, `stats`).

## 2. Messen

> ⚠️ In der Maschine liegen 230 V. Stecker ziehen, bevor du etwas anklemmst.
> Mit laufender Maschine nur an die bereits verlegten Messleitungen gehen.
> Kessel und Dampf sind heiß.

### 2.1 Was die Displayrückseite zeigt (Foto 2026-09-27, Duo DE)

- **Etikett** `DMT32240M035_07WTZ4`, Datumscode `180814` (August 2018).
  Aufdruck „5V ONLY“: Das Modul läuft mit 5 V.
- **Maschinenkabel:** Vier Adern (braun, gelb, weiß, grün) sind **direkt auf
  Lötpads** am rechten Rand gelötet und mit Heißkleber gesichert, kein Stecker.
  Beschriftung der Pads von oben: `GND GND GND NC TXD RXD`, darunter zwei vom
  Kleber verdeckte Pads (vermutlich die +5 V). Welche Farbe auf welchem Pad
  liegt, ist auf dem Foto nicht sicher zu erkennen und muss durchgemessen werden.
- **Direkt neben diesen Pads sitzt ein 16-poliger IC.** Auf DWIN-Modulen ist
  das sehr wahrscheinlich der RS-232-Pegelwandler (MAX3232-Klasse). **Die
  Leitung zum Mainboard kann also RS-232 sein (±5…12 V) statt TTL.** Vor dem
  Anklemmen unbedingt messen (siehe unten).
- **Unten am Rand** sitzt ein kleiner, unbenutzter Steckverbinder mit eigener
  Beschriftung. Vermutlich ist das der DWIN-Standard-Anschluss für die
  Benutzerschnittstelle.
- **Touch:** Das Flachkabel `CDQ9439-3.5-A` trägt einen **eigenen großen
  Controller-Chip** und geht auf einen 6-poligen Stecker. Das ist ein
  **kapazitiver** Touch mit I²C-Controller, nicht resistiv. Das Flachkabel ist
  über den Halter der Knopfzelle geknickt.
- **Halter für eine Knopfzelle** unter dem Touch-Flachkabel, **leer**. Die
  Uhrzeit stimmt trotzdem. Also stellt vermutlich das Mainboard die Uhr, etwa
  über Register `20` (RTC) oder über VPs. Das sollte im Mitschnitt kurz nach
  dem Einschalten auftauchen.
- **microSD-Schacht** unten rechts, **leer**. Das ist normal, die Karte wird
  nur für Updates gebraucht. Darüber spielt DWIN neue Oberflächen ein
  (`DWIN_SET`). Ein Bezzera-Update-Paket für das Display, etwa vom
  Kundendienst, enthielte die Dateien `13…bin` und `14…bin` und damit die
  komplette VP-Karte.
- Eine unbestückte Reihe von 5 Pads in der Mitte ist vermutlich ein Programmier-
  oder Debug-Anschluss.

Der Standby-Bildschirm zeigt „press to start“, ein Schraubenschlüssel-Symbol
(Servicemenü), Uhrzeit/Datum und zwei Versionen: **„FW: 2.1“** (rot,
vermutlich das Mainboard) und **„… 2.0“** (blau, teilweise verdeckt,
vermutlich die Displaysoftware). Auch das Einschalten aus dem Standby
geht über den Touch. Der Tastencode für „press to start“ ist also der erste,
den man braucht.

Auf dem Hauptbildschirm zeigt das Display Brühtemperatur, Dampftemperatur, zwei
Druckanzeigen (Brühgruppe 0–10 bar, Dampfkessel 0–2,5 bar), den Wasserstand
(„Niveau Wasser“ min–max) sowie Uhrzeit und Datum. Diese Werte muss das
Mainboard regelmäßig per `82` schreiben. Das sind die ersten VPs, nach denen
man im Mitschnitt sucht.

### 2.2 Pegel messen, bevor irgendetwas angeklemmt wird

Maschine an, Multimeter (DC) mit Schwarz auf ein `GND`-Pad, Rot nacheinander
auf `TXD` und `RXD`:

| gemessen im Ruhezustand | bedeutet | anklemmen über |
|---|---|---|
| ca. +3,3 V | TTL 3,3 V | 1 kΩ in Reihe direkt an GPIO16/17 |
| ca. +5 V | TTL 5 V | Spannungsteiler 10k/20k |
| **ca. −5 bis −12 V** | **RS-232** | **MAX3232-Modul** (nur dessen RX-Eingänge nutzen), niemals direkt |

Bei RS-232 liegt der Ruhepegel **negativ**. Genau das zerstört einen
ESP32-Eingang sofort. Der MAX3232 dreht die Logik auch wieder richtig herum,
der Sniffer bleibt dann auf „nicht invertiert“.

### 2.3 Sniffer anklemmen

Firmware: [`sniffer/duo_sniffer/duo_sniffer.ino`](sniffer/duo_sniffer/duo_sniffer.ino),
Arduino-IDE, Board „ESP32 Dev Module“. Der Sniffer sendet nichts, das Display
läuft normal weiter. Startwert ist 115200 Baud.

```
Datenleitung 1 ──[10k]──┬── GPIO16 (Kanal A)      Spannungsteiler nur bei 5-V-Pegel:
                        └──[20k]── GND            5 V · 20/(10+20) = 3,3 V
Datenleitung 2 ──[10k]──┬── GPIO17 (Kanal B)
                        └──[20k]── GND
GND Maschine ──────────────── GND ESP32
```

Bei 3,3-V-Pegel reichen 1-kΩ-Widerstände in Reihe. Den ESP32 per USB vom
Laptop versorgen, der dabei am Akku hängt, damit keine Masseschleife über das
Netzteil entsteht.

### 2.4 Mitschneiden

Seriellen Monitor mit **921600 Baud** öffnen, zum Beispiel
`pio device monitor -b 921600 | tee mitschnitt.log`. Dann:

1. `scan` eingeben und prüfen, ob Kanal A/B „normal“ und ~115200 Baud zeigt.
   Wenn nicht, mit `b <baud>` und `i` anpassen.
2. Die Zeilen sollten mit `5a a5` beginnen.
3. Bei jeder Aktion am Display **vorher** eine Markierung setzen:

| Markierung (`m ...`) | Aktion |
|---|---|
| `m kalt eingeschaltet` | Mitschnitt vor dem Einschalten starten: Bootsequenz, Seitenaufbau |
| `m ruhe` | 30 s nichts tun: was das Mainboard regelmäßig schreibt (Ist-Temperaturen) |
| `m bruehtemp 93.0 -> 93.5` | Brühtemperatur einen Schritt hoch, dann wieder runter |
| `m dampf 125 -> 126` | Dampftemperatur ändern |
| `m hebel hoch` / `m hebel runter` | Bezug starten/stoppen (Chrono) |
| `m menue einstellungen` | Menü öffnen, Passwort 1901 eingeben |
| `m standby an` / `m standby aus` | Standby |

### 2.5 Auswerten

[`tools/duo_sniff.py`](tools/duo_sniff.py), nur Python-Standardbibliothek:

```bash
python3 tools/duo_sniff.py dgus mitschnitt.log             # jeder DGUS-Rahmen, Tabelle aller VPs
python3 tools/duo_sniff.py dgus mitschnitt.log --changes   # nur neue Werte, Seitenwechsel, Tasten
python3 tools/duo_sniff.py stats mitschnitt.log            # falls kein 5A A5 auftaucht
```

`dgus` setzt Rahmen auch dann richtig zusammen, wenn der Sniffer sie auf
mehrere Zeilen verteilt oder mehrere in eine packt. Es gibt die Markierungen
an der richtigen Stelle mit aus. So sieht man direkt, welche VP-Adresse sich
nach `m bruehtemp ...` ändert und welcher Tastencode vom Display kam.

Tests: `python3 -m unittest discover -s tests`.

## 3. Danach

Wenn die Karte der VP-Adressen steht (Ist-/Solltemperaturen, Chrono,
Tastencodes, Seiten):

- **Nur lesen (Home Assistant):** Der passive Sniffer reicht. Er dekodiert
  die `82`-Rahmen des Mainboards und veröffentlicht die Werte.
- **Steuern:** ESP32 als Man-in-the-Middle. Leitung auftrennen, zwei UARTs
  (Mainboard-Seite und Display-Seite), alle Rahmen durchreichen. Eingaben wie
  „Brühtemperatur +“ als eigene `83`-Rahmen Richtung Mainboard einspeisen,
  genau so, wie das Display sie schickt. Das Mainboard prüft dann selbst die
  Grenzen (89–96 °C, 100–130 °C), weil es denselben Weg wie ein Fingerdruck geht.
- **Die ganze VP-Karte aus dem Display holen:** Die Konfiguration liegt im
  Display-Flash (`13…bin` Touch, `14…bin` Variablen). Mini DGUS kann über die
  Register `40`–`48` (LibOP) Flash-Bereiche in den VP-Speicher lesen, der sich
  dann per `83` auslesen lässt. Damit ließe sich die komplette Karte ohne Raten
  gewinnen. **Vorsicht:** Derselbe Mechanismus kann auch *schreiben* und das
  Display unbrauchbar machen. Nur mit dem DWIN-DGUS-Handbuch (Kapitel
  „DGUS Register“, 0x40–0x48) daneben und nur im Lesemodus. Wenn überhaupt,
  dann an einem ausgebauten Display mit eigener 5-V-Versorgung.

## Quellen

- [andrivet/ADVi3pp](https://github.com/andrivet/ADVi3pp), `Marlin/src/advi3pp/core/dgus.h` und `dgus.cpp`: Mini-DGUS-Befehle, Register, 115200 Baud; `LCD-Panel/DGUS-root/DWIN_SET/CONFIG.txt`: Beispiel für R0–RA
- [Sébastien Andrivet: DWIN Mini DGUS Display Development Guide (non-official)](https://sebastien.andrivet.com/en/posts/dwin-mini-dgus-display-development-guide-non-official/)
- DWIN DGUS Development Guide [v4.0 (2014)](https://cdn.papouch.com/data/user-content/old_eshop/files/DIS_DMT48270T043_3WT/dwin-dgus-dev-guide_v40_2014.pdf), [v4.3 (2015)](https://whiteelectronics.pl/img/cms/DWIN_DGUS_DEV_GUIDE_V43_2015.pdf): Kapitel 4 „DGUS Register (0x80/0x81)“
- [dwinhmi/DWIN_DGUS_HMI](https://github.com/dwinhmi/DWIN_DGUS_HMI): offizielle DWIN-Bibliothek für DGUS II (Seitenwechsel über VP 0x0084)
- [antondlr/gicar-serial](https://github.com/antondlr/gicar-serial), [magnusnordlander/lelit-bianca-protocol](https://github.com/magnusnordlander/lelit-bianca-protocol): Gicar-Protokolle
- [1st-line: Matrix/Duo Software-Kompatibilität](https://www.1st-line.com/technical-support/bezzera-technical-support/bezzera-matrix-duo-software-compatibility-changes/): Versionspaare Mainboard/Display
