<p align="right"><b>Deutsch</b> · <a href="variablen.en.md">English</a></p>

# Variablen des Displays (aus `14.bin`, Stand 2026-09-30)

Die Variablen-Konfiguration `14.bin` liegt im Display-Flash, 2 KB je Seite,
je Eintrag 32 Byte: `5A typ SP(2) len(2) VP(2) x(2) y(2) …`. Gelesen mit
LibOP wie `13.bin` (`tools/libop_lesen.py`), bisher die englischen Seiten
0–37. Der Auszug selbst liegt nicht im Repo. Stufen wie in
[`stand.md`](stand.md): ✅ an der Maschine beobachtet · 🧪 aus dem Flash
gelesen, Bedeutung gefolgert · 💡 Vermutung.

## Status VP `0x0050`, 9 Worte, Mainboard schreibt alle ~300 ms

| Wort | VP | Anzeige laut 14.bin | Bedeutung | |
|---|---|---|---|---|
| 0 | `0x0050` | Icon unter Kaffee °C, 3 Icons | Heizzustand Kaffeekessel (Linie blau/rot/grau) | ✅ 0 kalt, 2 beim Aufheizen; welche Zahl welche Farbe ist: 💡 |
| 1 | `0x0051` | Icon unter Service °C, 3 Icons | Heizzustand Servicekessel | ✅ wie Wort 0 |
| 2 | `0x0052` | Icon oben rechts, 2 Icons | **Warndreieck Tank leer** | ✅ 1 nach „Bitte Tank füllen“, 0 nach dem Füllen |
| 3 | `0x0053` | Zahl (105,118) | Kaffeekessel °C | ✅ |
| 4 | `0x0054` | Zahl (178,118) | Servicekessel °C | ✅ (127 °C bei 1,5 bar) |
| 5 | `0x0055` | Zeiger links, Wert 0–20 auf 98°–265° | **Pumpendruck**, 0,5 bar je Einheit | 🧪 6 nach einem Bezug = 3 bar |
| 6 | `0x0056` | Zeiger rechts, Wert 0–10 | **Druck Servicekessel**, 0,25 bar je Einheit | 🧪 6 bei 127 °C = 1,5 bar, passt zur Dampftabelle |
| 7 | `0x0057` | Icon rechts (263,70), 5 Icons | Wasserstand Tank | 🧪 immer 3 gesehen |
| 8 | `0x0058` | Icon (151,115), 2 Icons | Einheit °C/°F | 🧪 |

## Weitere Variablen

| VP | Seite | Bedeutung | |
|---|---|---|---|
| `0x0000`, `0x0001` | alle | Tastencode, Mainboard liest alle 100 ms; 1 Start, 0 Standby, 5 OK auf Alarm, 7 Kaffee-Einstellungen, 20 Menü, 22 Einstellungen 2, 23 Datum/Uhrzeit | ✅ |
| `0x0002` | Einstellseiten | „OK“ = 1; das Mainboard antwortet mit dem Einstellungsblock `0x0020`–`0x002D` und der Seite | ✅ |
| `0x0004` / `0x0005` | 35 / 44 | Passwort Einstellungen / Technik als Zahl | 🧪 Eingabetyp aus 13.bin |
| `0x0020` | 18 | Wassereingang 0 Tank, 1 Festwasser | ✅ |
| `0x0021`–`0x0024` | 49 | LED Körper: Helligkeit, R, G, B | 🧪 |
| `0x0025` | 16 | LED Körper ein/aus | ✅ |
| `0x0026` | 16 | Sprache 0 EN, 1 DE, 2 IT; nach OK schaltet das Mainboard in den Seitenblock der Sprache | ✅ |
| `0x0027` / `0x0028` | 17 / 18 | Bezüge seit Wartung / Tage seit Filterwechsel | 🧪 |
| `0x0029` / `0x002A` | 17 | Lichter Helligkeit / ein-aus | 🧪 |
| `0x002B` | 16 | Einheit 0 °C, 1 °F | 🧪 |
| `0x002C` / `0x002D` | 19 | Auto Ein/Aus / Passwort ein-aus | 🧪 |
| `0x002E`–`0x0032` | 57–62 | Uhr: Stunde, Minute, Tag, Monat, Jahr (2-stellig); OK (`0x0002` = 1) übernimmt sie ins Mainboard | ✅ „Uhr von diesem Gerät stellen“ funktioniert |
| `0x0053`, `0x0055`, `0x0059` | 6 | Ausgabezähler: Temperatur, Pumpendruck, **Sekunden seit Bezugsstart** | ✅ 0x0059 = 12 nach einem Bezug |
| `0x005A` | 7, 9 | Kesselpriorität 0 Kaffee, 1 Services, 2 Kein | 🧪 |
| `0x005B` | 7 | CRONO (Ausgabezähler) ein/aus | 🧪 |
| `0x005C` | 10 | Vorbrühen, 0–50 Zehntelsekunden | 🧪 |
| `0x005E` / `0x005F` | 7 / 8 | Kaffeekessel / Servicekessel ein-aus | 🧪 |
| `0x0060` / `0x0061` | 7 / 8 | Sollwert Kaffee / Tee °C | 🧪 |
| `0x0063` | 90 | Firmware Mainboard × 10 | ✅ |
| `0x0076`–`0x0079` | 23, 24 | PID Kaffeekessel P, I, D, Band | ✅ gelesen |
| `0x1000` | – | Puffer der Bridge für LibOP | – |
