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

### Das Mainboard (Fotos 2026-09-27, Duo DE)

**Hersteller ist PRO.EL.IND (Italien), nicht Gicar.** Deckelaufkleber:
`SDEDB` / `BZ1PTE` / `7661047PR`, Produktionsaufkleber `1809` (September 2018,
passend zum Display von August 2018). Versorgung 12 V DC, Sicherung 3,15 A.

| Bauteil | Was es ist | Bedeutung |
|---|---|---|
| NXP/Freescale **MC9S08PA32** (LQFP-64, Aufdruck „M9S8PA32A VLH“) | 8-Bit-Mikrocontroller S08, 32 KB Flash, läuft mit 3,3 oder 5 V | die ganze Maschinenlogik. Keine Spur von einem RS-232-Wandler auf dem Board, also spricht die MCU ziemlich sicher TTL |
| **CNPR**, 4-polige Stiftleiste | vermutlich der BDM-Programmieranschluss der S08 (BKGD, RESET, VDD, GND) | Firmware-Update ab Werk. Auslesen ist bei gesetztem Security-Bit gesperrt, Finger weg |
| ST **M41T56** + Quarz + **eingelötete Knopfzelle** (gelb) | I²C-Echtzeituhr mit 56 Byte NVRAM | erklärt, warum die Uhr trotz leerem Batteriefach im Display stimmt: Das Mainboard stellt sie |
| RECOM-DC/DC-Wandler | 12 V → 5 V | vermutlich die 5 V für das Display („5V ONLY“) |
| ULN2003 + 5 Omron-Relais, TLP3063 (Optotriac mit Nulldurchgang) | Relais- und Lasttreiber | Heizung Gruppe, Magnetventile, Pumpe |
| Stecker **CN6 „DISPLAY“** | 4 Adern (rot, rosa, grün, schwarz) | die Leitung zum Display, hier wird mitgeschnitten |

Belegung laut Deckelaufkleber:

- **Relaisausgänge 1–7:** 1 Heizung Gruppe, 2 EV Wassernetz, 3 EV Tank,
  4 EV Füllen (Dampfkessel), 5 EV Gruppe, 6 Pumpe, 7 Common
- **Fühler:** NTC Gruppe, NTC Kaffee(kessel), NTC Dampf(kessel), also die
  drei PIDs der DE. Dazu SSR Kaffee und SSR Dampf.
- **PRESS.:** Drucksensor (die Druckanzeige im Display)
- **CAP. SENS:** kapazitiver Füllstandssensor (die Anzeige „Niveau Wasser“)
- **5-poliger Klemmblock:** Durchflussmesser (1 −, 2 +, 3 OUT1),
  4 Microswitch (Hebel), 5 S.LIV (Füllstandssonde Dampfkessel)
- **LED FRONT, LED RETRO** (12 V), **KEYBOARD** (graues Flachbandkabel)

1st-line führt `7661047PR` als Mainboard „1.2“. Das Display meldet aber
„FW: 2.1“. Entweder wurde die Firmware später aktualisiert, oder die Zahlen
bedeuten nicht dasselbe. Für den Mitschnitt spielt das keine Rolle.

### Frühere Hypothese: Gicar-Protokolle

Händler nennen die Steuerung „Gicar PID controller“
([Whole Latte Love](https://www.wholelattelove.com/products/bezzera-matrix-mn-dual-boiler-espresso-machine)).
**Bei dieser Duo DE (2018) stimmt das nicht, das Board ist von PRO.EL.IND.**
Die dokumentierten Gicar-Protokolle
([antondlr/gicar-serial](https://github.com/antondlr/gicar-serial),
[lelit-bianca-protocol](https://github.com/magnusnordlander/lelit-bianca-protocol))
sind damit hinfällig. Das Skript erkennt sie trotzdem weiterhin (`gicar`, `stats`),
falls eine andere Baureihe doch ein Gicar-Board hat.

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
  **kapazitiver** Touch mit I²C-Controller, nicht resistiv. Der Chip ist ein
  **SiS9252** (Silicon Integrated Systems). Aufdruck schwer lesbar, etwa
  `9252 / SiS… / PXD0431 Green / 1246DA`. Am 6-poligen Kabelende steht `VDD`
  bei Pin 6. Ersatz-Touchpanels aus dem Handel haben fast immer FocalTech- oder
  Goodix-Controller und passen deshalb vermutlich nicht. Das Flachkabel ist
  über den Halter der Knopfzelle geknickt.
- **LCD-Panel** hinter dem Touch: Aufkleber `LQ035NC1…` (abgeschnitten),
  vermutlich das verbreitete 3,5"-QVGA-Panel LQ035NC111 mit 54-poligem
  RGB-Flachkabel. Ein Standardteil.
- **Befund 2026-09-27:** Beim Wackeln am 6-poligen Touch-Stecker piept das
  Display beim Drücken zeitweise. Der Touch selbst lebt also, der Fehler ist ein
  Wackelkontakt am Stecker bzw. Flachkabel.
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

Am einfachsten misst man am Mainboard-Stecker **CN6 „DISPLAY“**: vier Adern,
rot, rosa, grün, schwarz. Erwartung: schwarz = GND, rot = +5 V, rosa und grün
= die beiden Datenleitungen. Die Farben sind geraten, also nachmessen. Weil die
MCU keinen RS-232-Wandler hat, ist **TTL mit 5 V am wahrscheinlichsten**.

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

### 2.3 Einfachster Weg: Logic Analyzer (24 MHz, 8 Kanäle, „fx2lafw“)

Ein billiger USB-Logic-Analyzer reicht für den ersten Mitschnitt, ganz ohne
ESP32-Firmware.

1. Software: [PulseView](https://sigrok.org/wiki/Downloads) (sigrok). Unter
   Windows vorher mit Zadig den WinUSB-Treiber für das Gerät installieren.
2. Anschluss: `GND` an die GND-Ader, `CH0`/`D0` an die erste Datenader,
   `CH1`/`D1` an die zweite. **Erst messen:** Diese Analyzer vertragen meist
   bis 5 V, aber **keine negativen Spannungen** (RS-232). Wer bei 5 V sicher
   gehen will, schaltet 1 kΩ in Reihe.
3. PulseView: Gerät „fx2lafw“, **2 MHz**, Samples so wählen, dass es für
   20–60 s reicht (z. B. 100 M). Aufnehmen, dabei die Aktionen notieren
   (Uhrzeit ab Start), dann **als `.sr` speichern**.
4. Umwandeln und auswerten:

```bash
python3 tools/sr2log.py mitschnitt.sr > mitschnitt.log   # UART-Dekodierung, Baudrate automatisch
python3 tools/duo_sniff.py dgus mitschnitt.log
```

`sr2log.py` erkennt die Baudrate aus dem kürzesten Puls. Optionen: `--a`/`--b`
für andere Kanäle, `--baud` fest, `--invert` bei invertierter Leitung.

### 2.4 Dauerhaft: ESP32-Sniffer


**Einkaufsliste:**

- ESP32-Devkit mit klassischem ESP32 (z. B. „ESP32 DevKitC V4“, ESP32-WROOM-32), USB-Kabel
- 2 × 10 kΩ und 2 × 20 kΩ (für die zwei Spannungsteiler bei 5-V-Pegel)
- Steckbrett und Dupont-Kabel
- für den Abgriff an **CN6 „DISPLAY“**: je nach Rastermaß des Steckers eine
  4-polige „Stacking“-Buchsenleiste mit langen Beinen (2,54 mm), die zwischen
  Stecker und Stiftleiste gesteckt wird, oder dünne Nadeln/Dupont-Stifte, die
  von hinten neben die Adern in das Steckergehäuse geschoben werden

**Abgriff:** am Mainboard-Stecker CN6 (Adern rot, rosa, grün, schwarz). Nur
die zwei Datenadern und GND werden angezapft, **+5 V (vermutlich rot) bleibt
frei**. Der ESP32 bekommt Strom über USB vom Laptop. Das Display bleibt
angeschlossen und läuft normal weiter.


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

### 2.5 Mitschneiden

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

### 2.6 Auswerten

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
