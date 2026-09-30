<p align="right"><b>Deutsch</b> · <a href="stand.en.md">English</a></p>

# Was ist sicher, was nicht

Jede Aussage in diesem Repo gehört zu genau einer von drei Stufen:

| Zeichen | Stufe | heißt |
|---|---|---|
| ✅ | **getestet** | an der echten Duo DE (Baujahr 2018) oder am echten Display beobachtet: Mitschnitt, Messung, Foto, aus dem Flash gelesen oder ausprobiert. Beleg steht dabei. |
| 🧪 | **gebaut, ungetestet** | sinnvoll und umgesetzt (kompiliert, teils in der Simulation `tools/web_lokal.py --demo` geprüft), aber nie an der Maschine oder mit echter Zusatzhardware ausprobiert |
| 💡 | **Idee / Vermutung** | abgeleitet aus Handbuch, Fotos, Datenblättern, Händlerangaben oder Analogie. Weder gemessen noch gebaut, oder gebaut, aber die Wirkung an der Maschine ist unbekannt. |

„Laut Handbuch“ oder „laut Datenblatt“ ist 💡, solange es an dieser Maschine
niemand beobachtet hat. Alle Mitschnitte in [`captures/`](../captures/) stammen
von der **kalten Maschine mit leerem Tank**; ein Bezug war noch nie auf der Leitung.

---

## Display und Hardware

| | Aussage | Beleg |
|---|---|---|
| ✅ | Display ist ein DWIN **DMT32240M035_07WTZ4**, Datumscode 180814, „5V ONLY“ | Foto Rückseite |
| ✅ | Adern am Display: GND braun, TXD gelb, RXD weiß, VCC grün; am Mainboard-Stecker CN6: rot +5 V, rosa Mainboard → Display, grün Display → Mainboard, schwarz GND | durchgemessen, `neukabel.sr` |
| ✅ | Pegel TTL: Display-TXD +3,2 V, Mainboard-TX +4,8 V, kein RS-232 | Multimeter |
| ✅ | Display-Firmware (Register `0x00`) = `0x22`, Startbild „TFT 2.0“ | gelesen |
| ✅ | Speicher zwei TSOP-48, kein SPI-Flash; Knopfzellen-Halter leer, Uhr stimmt trotzdem | Foto, beobachtet |
| ✅ | Touch lebt: Beim Wackeln am 6-poligen Stecker piept das Display; letzte echte Berührung bei x=286, y=234 („OK“) | beobachtet, Touch-Register gelesen |
| ✅ | Touch-Controller **SiS9252** auf dem Flachkabel `CDQ9439-3.5-A` | Foto (Aufdruck schwer lesbar) |
| ✅ | Mainboard von **PRO.EL.IND**, MCU MC9S08PA32, RTC M41T56, Aufkleber-Belegung (Relais, Fühler, PRESS, CAP. SENS, KEYBOARD) | Fotos |
| ✅ | Das Mainboard stellt beim Start die Uhr des Displays | `boot.sr` |
| 💡 | Die Uhr kommt aus dem M41T56 | Bauteil auf dem Foto |
| 💡 | CNPR ist der BDM-Programmieranschluss, 5 Pads auf dem Display ein Debug-Anschluss | Analogie |
| 💡 | LCD-Panel ist LQ035NC111 | Aufkleber abgeschnitten |
| 💡 | Zwischenstecker ist Molex Mini-Fit Jr. (4,2 mm) | Kodierung auf dem Foto, Raster nicht gemessen |
| 💡 | Ersatz-Touchpanels mit FocalTech-/Goodix-Controller laufen nicht | Annahme zur Display-Firmware |
| 💡 | Alles weitere in [`bauteile.md`](bauteile.md): dort heißt „belegt“ nur „von Hersteller- oder Händlerseiten bestätigt“, nicht an dieser Maschine gemessen | Recherche |

## Protokoll

| | Aussage | Beleg |
|---|---|---|
| ✅ | DWIN Mini DGUS mit Kopf **`C6 A5`**, 115200 8N1, ohne CRC | alle Mitschnitte |
| ✅ | Einschaltablauf: Seite 90, Verbindungstest VP `0x0063` = 21, Uhr stellen, Seite 101, VP `0x0000` = 1, bei leerem Tank Seite 103 | `boot.sr` |
| ✅ | Mainboard liest VP `0x0000` und `0x0001` alle 100 ms | `boot.sr`, `home.sr`, `tank.sr` |
| ✅ | VP `0x0000` = 5 nach „OK“ auf „Bitte Tank füllen“ und bleibt stehen | `home.sr`, `tank.sr` |
| ✅ | Beim Wechsel in den Standby schreibt das Mainboard selbst Seite 100 und VP `0x0000` = 0; im Standby stellt es jede Sekunde die Uhr | Bridge-Protokoll 2026-09-30 |
| ✅ | VP `0x0050` alle ~300 ms, Wort 3 Kaffeekessel °C, Wort 4 Servicekessel °C | Mitschnitte, mit Testwerten auf dem Display zugeordnet |
| ✅ | VP `0x0063` = Firmware Mainboard × 10 (21 → „FW: 2.1“) | `boot.sr`, Startbild |
| 💡 | Versionstabelle der Händler und Gründe für Inkompatibilitäten: [`versionen.md`](versionen.md) | Händlerangaben, Folgerung |
| ✅ | Wort 2 wird nach Seite 103 zu 1, Wort 7 = 3, Wort 5 einmal 1 s lang 1; Wort 0, 1, 6, 8 immer 0 | `boot.sr` |
| 💡 | Bedeutung von Wort 2, 5 und 7 | — |
| 🧪 | Wort 5 = Pumpendruck (0,5 bar je Einheit), Wort 6 = Druck Servicekessel (0,25 bar je Einheit), alle Worte in [`variablen.md`](variablen.md) | `14.bin` (Zeiger-Konfiguration); Wort 6 = 6 bei 127 °C passt zu 1,5 bar; Bezug noch nicht mitgeschnitten |
| ✅ | Das Display meldet nichts von sich aus; Seitenwechsel per Touch macht es ohne Meldung ans Mainboard | Mitschnitte, Tastentyp `FDxx` im Flash |
| ✅ | Register `0x4F` und die Touch-Register `0x05`–`0x07` lösen keinen Tastendruck aus | ausprobiert (`tools/tasten_scan.py`) |
| ✅ | Das Mainboard liest VP `0x0002` auf Einstellseiten und antwortet auf 1 mit dem Einstellungsblock `0x0020`–`0x002D` | Bridge-Protokoll 2026-09-30 |

## Seiten und Tasten

| | Aussage | Beleg |
|---|---|---|
| ✅ | 300 Seiten in drei Sprachen, Fotos und Katalog | `docs/seiten/`, `docs/seiten.md` |
| ✅ | 1070 Tasten mit Fläche, Folgeseite, Funktion, Wert | aus dem Display-Flash gelesen, `docs/tasten.json` |
| ✅ | „Für Start drücken“ schreibt VP `0x0000` = 1 (DE, IT), „Standby“ im Seitenmenü = 0 (alle Sprachen) | Flash |
| ✅ | Englische Seite 9: ein Byte im Flash beschädigt (`F3` statt `FE`) | Flash, zweimal gelesen |
| 💡 | Deshalb wirkt dort die Taste „Coffee“ nicht | Folgerung |
| ✅ | Seite x06 ist der Ausgabezähler: Zeiger Pumpendruck (VP `0x0055`), Sekunden seit Bezugsstart (VP `0x0059`), Temperatur | `14.bin` + Bezug 2026-09-30 (`0x0059` = 12) |

## ESP32-Bridge

| | Aussage | Beleg |
|---|---|---|
| ✅ | Durchreichen und Mitschneiden an der laufenden Maschine | an der Maschine betrieben |
| ✅ | Die Firmware mit Weboberfläche startet auf dem ESP32-S3-ETH, eigenes WLAN `duo-bridge`; das Heim-WLAN (Vodafone) sieht der ESP32 nicht, er kann nur 2,4 GHz | USB-Ausgabe, WLAN-Scan |
| ✅ | Mit falschem Teiler an GPIO17 (10k/20k vertauscht) kommt Datensalat; Display-braun/-grün vertauscht verpolt das Display | passiert |
| 🧪 | Display-Emulation (Modus 1): antwortet Byte für Byte wie das Display | Testbefehle `M`/`D` |
| ✅ | Ohne Antwortleitung des Displays startet die Maschine nicht weiter; mit Display parallel zum ESP32 stürzt sie ab | beobachtet |
| 💡 | Ursache war der verpasste Verbindungstest VP `0x0063` | Folgerung |
| 🧪 | Hybridmodus (Modus 2) und Tastendruck-Injektion (`o`, `w` auf VP `0x0000`) | kompiliert |
| ✅ | Das Mainboard reagiert auf einen von außen gesetzten Tastenwert wie auf einen echten Druck: Start (VP `0x0000` = 1 + Seite 101 → 57 ms später Statusworte und Seite 103), Standby (= 0 + Seite 100 → 145 ms später bestätigt das Mainboard Seite 100), OK auf Alarm (= 5), Einstellungen-OK (`0x0002` = 1 → Einstellungsblock) | Bridge-Protokoll 2026-09-30 |
| ✅ | Weboberfläche mit Display-Nachbau am echten Display: Live-Werte, Seitenwechsel, VPs aus dem Display gelesen (PID-Werte `0x0076`–`0x0078`) | Screenshot vom 2026-09-27 |
| 🧪 | Lokale Weboberfläche über USB (`tools/web_lokal.py`) | Skript |
| ✅ | Tests der Python-Werkzeuge (`python3 -m unittest discover -s tests`) | 26 Tests grün |

## Mehr als das Display

| | Aussage | Beleg |
|---|---|---|
| ✅ | Ein/Aus aus der Ferne (VP `0x0000` = 1/0 plus Seite x01/x00) | 2026-09-30 an der Maschine, auch aus der Weboberfläche |
| ✅ | Uhr vom Handy/Rechner stellen (VP `0x002E`–`0x0032`, dann `0x0002` = 1) | 2026-09-30, Display-Uhr übernimmt |
| ✅ | Maschine heizt nur, wenn Wort 2 (Tank leer) = 0 ist; nach Füllen und Aus/Ein füllt die Pumpe den Kessel, dann heizen beide Kessel (Wort 0/1 = 2) | 2026-09-30 |
| 🧪 | Zustand an/Standby aus dem Tastenwert statt aus der Seite | gebaut |
| 🧪 | Alarmmeldung aus der Seite | gebaut; ✅ dass das Mainboard Seite 103 selbst schaltet (`boot.sr`) |
| 🧪 | Verlauf von VP `0x0050` im ESP32 (24 h mit PSRAM) | gebaut, Oberfläche in der Simulation geprüft |
| 🧪 | Druckkurven | gebaut; Druckwort unbekannt (💡 oben) |
| 🧪 | Home Assistant per MQTT-Discovery | gebaut; nie gegen einen Broker gelaufen |
| 🧪 | Bluetooth-Waagen Acaia, Bookoo, Felicita, Decent | gebaut nach [AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE), BooKoo- und Decent-Doku; mit keiner echten Waage getestet |
| 🧪 | Brühprofile: speichern in der Bridge, auf die Maschine schreiben über Seite x07/x08 + OK, Kontrolle durch erneutes Öffnen | gebaut, Test an der Maschine offen |
| 🧪 | WLAN-Waagen (URL abfragen, `POST /api/waage`, MQTT) | gebaut |
| 🧪 | Bezugserkennung an den ersten Tropfen, Vorlauf lernen | gebaut, in der Simulation geprüft |
| 🧪 | Stopp-Ausgang: kurzer Tastendruck, nur bei laufendem Bezug (≥ 0,5 g/s) | gebaut |
| 💡 | Ein zweiter Druck auf die Dauerausgabe-Taste stoppt die Ausgabe | Handbuch 5.4, an dieser Maschine nicht ausprobiert |
| 💡 | Stopp-Taste per PhotoMOS-Relais parallel zur Dauerausgabe-Taste | Konzept; Belegung des Tastenfeld-Kabels unbekannt |
| 💡 | Bezug erkennen an Seite x06 | gebaut (🧪), beruht aber auf der 💡-Vermutung zu Seite x06 |
| 💡 | Standby mitten im Bezug stoppt die Pumpe | nur Idee, nicht gebaut |

## Offen, nicht gebaut

| | Aussage |
|---|---|
| 💡 | Dauerbetrieb: Passwort für die Weboberfläche, abschaltbares eigenes WLAN, OTA, Watchdog, Versorgung aus der Maschine |
| 💡 | Variablen-Konfiguration `14.bin` aus dem Flash lesen (zeigt, welche VP wo angezeigt wird) |
| 💡 | Touch-Controller SiS9252 per I²C mitschneiden und nachbilden |

---

**Nächster Mitschnitt, der am meisten klärt:** ein ganzer Bezug mit vollem Tank,
gestartet mit der Dauerausgabe-Taste. Er entscheidet über das Druckwort, Seite
x06, die VP der Ausgabedauer und ob Wort 5 die Pumpe ist.
