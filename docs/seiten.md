# Seitenkatalog des Displays (Duo DE, Display-Projekt „TFT 2.0“)

Aufgenommen 2026-09-27 mit `tools/seiten_foto.py`: Display allein an der
ESP32-Bridge, jede Seite per `80 03 00 NN` angewählt und fotografiert.
Fotos: [`docs/seiten/seite_NNN.jpg`](seiten/) (verkleinert auf 1024 px),
Übersichtsbögen mit je 40 Seiten: [`uebersicht_000.jpg`](seiten/uebersicht_000.jpg)
bis [`uebersicht_280.jpg`](seiten/uebersicht_280.jpg), zeilenweise 8 Seiten.

**Aufbau:** drei Sprachen zu je 100 Seiten.

| Bereich | Sprache |
|---|---|
| 0–95 | Englisch |
| 100–195 | Deutsch |
| 200–295 | Italienisch |
| 96–99, 196–199, 296–299 | leer (weiß) |

Die Seite in einer anderen Sprache liegt fast immer bei +100 bzw. +200
(Englisch 3 „Please fill water tank“ = Deutsch 103 „Bitte Tank füllen“ =
Italienisch 203 „Riempire serbatoio“). **Ausnahmen:** Im Technikmenü sind
die Sprachen unterschiedlich belegt (Englisch 24–26 sind Varianten des
Technikmenüs mit markierter Zeile, Deutsch 124–126 sind PID-Kessel Kaffee
Band, PID-Kessel Services, PID-Kessel Services Band), ebenso bei 84–86
(Deutsch 184 ist „Gruppentemperatur“, Englisch 84 „Insert cap“). Die Tabelle
unten gilt für Englisch.

Beim Einschalten zeigt das Mainboard Seite 90 (Startbild mit „TFT 2.0“ und
„FW: x.y“ aus VP `0x0063`), dann 101 und bei leerem Tank 103.

## Seiten (englische Nummern)

| Seite | Inhalt |
|---|---|
| 0 | Standby: Logo, „press to start“, Uhr, Schraubenschlüssel |
| 1 | Startbildschirm: Zeiger für beide Kessel, Temperaturen, Druckskala 0–2,5 bar, Uhr |
| 2 | Alarm: Loading time out, restart loading |
| 3 | Alarm: Please fill water tank |
| 4 | Alarm: NTC failure |
| 5, 6 | Startbildschirm, Varianten. 6 hat nur den Zeiger Pumpendruck 0–10 bar und eine große Zahl in der Mitte: vermutlich der Ausgabezähler (Pumpendruck und Ausgabedauer während des Bezugs, Handbuch 5.4.4) |
| 7 | Coffee settings: Boiler an/aus, Priority, Temperatur, Group, Wetting |
| 8 | Tea settings: Temperatur |
| 9, 11 | Boiler priority: Coffee / Service / None |
| 10 | Wetting: Sekunden |
| 12, 13 | Coffee/Tea settings ohne Wert |
| 14 | Startbildschirm mit Seitenmenü |
| 15 | Bildschirm 10 s gesperrt zum Reinigen |
| 16–19 | Settings 1–4: Language, Units, LED body RGB, Lights, Sensor calibration, Maintenance, Water filter, Water source, Date & time, Auto on/off, Password |
| 20–22 | Technician menu: Machine layout E61/BZ, PID group, PID coffee boiler, PID service boiler, Level probes, Password, Loading time out, Total brewings, Reset |
| 24–26 | Technician menu, Varianten mit markierter Zeile |
| 23 | PID coffee boiler: P, I, D |
| 27–30 | Level probes: 50K / 150K / 400K / 1M |
| 31–33 | Loading time out: 60 / 90 / 120 s |
| 34 | Attention: Confirm to reset the machine (ESC/OK) |
| 35, 36 | Press OK to type password / Wrong password |
| 37 | DGUS-Standardbild (unbenutzt) |
| 38, 43 | Zifferntastatur für Passwort |
| 39 | Password: Setting password / Technician password |
| 40–42 | Passwort zurücksetzen / New password saved |
| 44, 45 | Press OK to type password / Wrong password |
| 46–48 | Language: Italiano, English, Deutsch |
| 49 | LED body RGB |
| 50 | Lights (Helligkeit) |
| 51–53 | Sensor calibration: Tank leeren, läuft, OK |
| 54 | Maintenance: number of erogations |
| 55 | Water filter: number of days |
| 56 | Alarm: Necessary maintenance |
| 57 | leer |
| 58–62 | Date & time (je ein Feld markiert) |
| 63 | Startbildschirm mit Seitenmenü |
| 64–69 | Wash: Art wählen (fast/slow), Blindsieb, Hebel, Washing…, fertig |
| 70–76 | Auto on/off, je Wochentag |
| 77 | Alarm: No volumetric signal, volumetric dosage off |
| 78 | Startbildschirm (andere Maschinenvariante) |
| 79 | Technician menu (Variante) |
| 80, 81 | PID group: P/I/D, Band |
| 82 | Settings: Auto on/off, Password, Group temp |
| 83 | Startbildschirm mit einem Zeiger (Einkreiser/BZ?) |
| 84 | Insert cap |
| 85–88 | Wash (zweite Variante) |
| 89 | Alarm: Change water filter |
| 90 | Startbild (Logo, TFT-Version, FW des Mainboards) |
| 91 | Warning: Washing process interrupted |
| 92–95 | Passwort: aktuelles eingeben, neues eingeben, gespeichert, falsch |

## Variablen, die auf den Bildern zu sehen sind

Vor dem Durchlauf wurde VP `0x0050` mit 10, 11, …, 18 gefüllt:

| VP | Wort | angezeigt auf | Bedeutung |
|---|---|---|---|
| `0x0050` | 3 | Startbildschirm, unter dem Kaffee-Symbol | Temperatur Kaffeekessel °C |
| `0x0050` | 4 | Startbildschirm, unter dem Dampf-Symbol | Temperatur Servicekessel °C |
| `0x0063` | – | Seite 90, „FW: 2.1“ bei Wert 21 | Firmware Mainboard × 10 |
