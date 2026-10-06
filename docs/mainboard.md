<p align="right"><b>Deutsch</b></p>

# Mainboard: Plattform, verwandte Maschinen, Firmware

Stand der Recherche vom 06.10.2026. Was hier steht, ist belegt (Quelle oder
Messung) oder als Vermutung gekennzeichnet.

## Pro.El.Ind als Plattform

Das Mainboard der Duo DE stammt von **PRO.EL.IND** (Italien), MCU NXP
**MC9S08PA32** im LQFP-64 ([README](../README.md#das-mainboard)). Pro.El.Ind
entwickelt Hardware und Firmware im Auftrag verschiedener
Kaffeemaschinenhersteller, aus einem Katalog angepasst oder ganz nach Wunsch.

**Arbeitshypothese:** gemeinsame Hardware- und Firmware-Basis (MCU, Treiber,
Anbindung des DWIN-Displays), je Hersteller eigene Konfiguration und Oberfläche.
Gleiche Bauteilfamilien und gleiches Display-Protokoll sind wahrscheinlich,
gleiche komplette Firmware, Pinbelegung oder VP-Adressen nicht. Das muss jede
weitere Maschine einzeln zeigen.

## Rocket R58 (Cinquantotto): dieselbe Protokollfamilie

Im [Kaffee-Netz-Thread „Rocket R58 Cinquantotto USB-Protokoll“](https://www.kaffee-netz.de/threads/rocket-r58-cinquantotto-usb-protokoll.139064/)
(2021–2024, Hauptanalyse von *HanDeKe*) steht für die R58:

- Elektronik von Pro.El.Ind, MCU **MC9S08PA32**, UART an den Pins 23/24
- Display als „Remote-Display“, Maschine ist Master und fragt ab
- 115200 Baud, 8N1; ältere Displays mit 6-poligem Stecker, neuere mit USB-B,
  dahinter trotzdem serielle Daten
- Rahmenkopf `90 165` = **`5A A5`**, „Nachrichtentyp 131“ = **`0x83`**

Gegen das Modell dieses Repos gerechnet:

| R58-Rahmen (dezimal) | als DWIN gelesen |
|---|---|
| `90 165 6 131 16 0 3 104 100` | `5A A5 06` · `83 1000 03` (3 Worte ab VP `0x1000` lesen) · `68 64` |
| `90 165 12 131 16 0 3 0 x 0 y 0 0 216 224` | Antwort des Displays: `83 1000 03` + 3 Worte + 2 Byte |

**Die letzten zwei Byte sind eine CRC16 (Modbus) über `83 10 00 03`:
berechnet `0x6468`, im Rahmen `68 64`** (geprüft 06.10.2026). Die R58 nutzt also
denselben DWIN-DGUS-Befehlssatz wie die Duo, mit Standardkopf `5A A5` und
**eingeschalteter CRC** (DWIN-Register R2, Bit 4). Die Duo hat Kopf `C6 A5` und
keine CRC. Die Vermutung im Thread („CRC16“) stimmt; „Nachrichtentyp 131“ ist der
DGUS-Befehl „VP lesen“, und das Display meldet Werte nur, wenn die Maschine fragt.

Die lange R58-Nachricht (35 Byte) ließ sich aus der abgerufenen Fassung nicht
nachrechnen (CRC passt nicht); dafür braucht es die Bytes aus dem Originalbeitrag.

## Firmware sichern über BDM (Plan, noch nicht versucht)

Der S08 hat eine Ein-Draht-Debugschnittstelle (BDM). Laut NXP-Referenzhandbuch
MC9S08PA60/PA32 (Rev. 3, Tabelle 2-1), LQFP-64:

| Signal | Pin (64) | Pin (48) | Pin (44) |
|---|---|---|---|
| PTA4/BKGD/MS | 64 | 48 | 44 |
| PTA5/RESET | 63 | 47 | 43 |
| VDD | 7, 41 | 5, 31 | 5, 28 |
| VSS | 10, 13, 40 | 8, 11, 30 | 8, 11, 27 |
| EXTAL/XTAL | 11/12 | 9/10 | 9/10 |

**CNPR** (4 Pole, unbestückt) passt zu GND, VDD, BKGD, RESET. Bestätigen per
Durchgangsprüfung, stromlos.

**Wichtig zur Sicherung (Security):** Ist die Flash-Sicherung gesetzt
(`NV_FSEC[SEC]`), liest BDM den Flash nicht aus. Der Backdoor-Schlüssel lässt
sich laut Handbuch **nicht** über BDM eingeben, nur aus laufendem Code. Bleibt
dann nur das **Komplettlöschen**, und danach ist das Mainboard leer. Deshalb:

- nur am **alten Ersatz-Mainboard** versuchen, nie an der Maschine im Betrieb
- nur lesen; jede Abfrage eines Programmers „Gerät gesichert, löschen?“ ablehnen
- zuerst den Sicherungszustand abfragen, dann erst Speicher lesen

Gelingt der Dump: 32 KB Flash + 256 B EEPROM in Ghidra (HCS08), Ziel ist die
Zuordnung MCU-Pin → Stecker → Funktion und die Zustandsmaschine der Maschine.
Offen in [Issue „Firmware über BDM sichern“](https://github.com/larszu/bezzera-duo-protocol/issues).

## Korrekturen zu einer KI-Recherche (ChatGPT, Oktober 2026)

- „R58 ohne CRC“: falsch, die R58 nutzt CRC16 (siehe oben).
- „MC9S08PA32 im 48-Pin-Gehäuse“: bei der Duo LQFP-64 (Foto, README).
- „Backdoor-Schlüssel über BDM entsperren“: laut Handbuch nicht möglich.
- Druck- oder Flussregelung wie Gaggiuino: die Duo schaltet die Pumpe nur ein und
  aus (Relais); Regelung bräuchte Hardware (Dimmer/Pumpensteuerung).
