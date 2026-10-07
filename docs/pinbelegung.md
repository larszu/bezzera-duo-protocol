<p align="right"><b>Deutsch</b> · <a href="pinbelegung.en.md">English</a></p>

# Pinbelegung: ESP32-S3-ETH und klassischer ESP32

Die Firmware nutzt auf beiden Boards **dieselben GPIO-Nummern**
(`bridge/duo_bridge/duo_bridge.ino`, `PIN_…`). Beim Umbau von einem Board aufs
andere bleiben die Kabel also an denselben Signalen, nur die Lage und die
Beschriftung der Stifte sind anders.

| Signal | GPIO | Waveshare ESP32-S3-ETH | klassischer ESP32 (DevKit, 30 Pins) | Kabel |
|---|---|---|---|---|
| RX vom Display | **15** | Stift „15“ | **D15** (auch IO15/G15), rechte Reihe, 13. von oben | Display **gelb** |
| TX zum Mainboard | **16** | Stift „16“ | **RX2** (GPIO16), rechte Reihe, 10. von oben | Mainboard **grün** (Ersatzkabel gelb) |
| RX vom Mainboard | **17** | Stift „17“ | **TX2** (GPIO17), rechte Reihe, 9. von oben | Mainboard **rosa** (weiß) über 10 kΩ, 20 kΩ nach GND |
| TX zum Display | **18** | Stift „18“ | **D18** (IO18), rechte Reihe, 7. von oben | Display **weiß** |
| Masse | GND | „GND“ | **GND**, rechte Reihe, 14. von oben (oder links unten) | Mainboard schwarz + Display braun |
| 5 V (nur ohne Maschine) | – | „5V“ | **VIN**, linke Reihe unten | Display grün, nur im Betrieb ohne Maschine |

Lage beim klassischen DevKit: Antenne oben, USB unten. Rechte Reihe von oben:
D23, D22, TX0, RX0, D21, D19, **D18**, D5, **TX2**, **RX2**, D4, D2, **D15**,
**GND**, 3V3. Andere Board-Varianten (38 Pins, „ESP32-WROOM-32 DevKitC“) tragen
dieselben GPIO-Namen an anderer Stelle: nach der Beschriftung gehen, nicht nach
der Position.

## Achtung beim klassischen ESP32

- **„RX2“/„TX2“ sind nur Namen:** Die Firmware nutzt GPIO16 als Sender zum
  Mainboard und GPIO17 als Empfänger. Kabel nach der Tabelle oben stecken, nicht
  nach RX/TX auf dem Aufdruck.
- **Boards mit PSRAM (WROVER)** belegen GPIO16/17 intern. Dort funktioniert die
  Bridge nicht; ein WROOM-Board ohne PSRAM nehmen (`esptool chip-id` zeigt
  „Embedded PSRAM“ nicht an).
- **Spannungsteiler** am Eingang vom Mainboard (10 kΩ zur Leitung, 20 kΩ nach GND)
  wie beim S3: die Maschine sendet 5 V, der ESP32 verträgt 3,3 V.
- Kein Ethernet, kein Matter, Update nur über die Flash-Seite
  ([flashen.md](flashen.md)).
- Wenig RAM: ntfy als `http://ntfy.sh/…` eintragen (ohne Verschlüsselung, sonst
  fehlt Speicher). Kalender und Telegram brauchen TLS und können scheitern.

## Umbau S3 → klassisch (oder zurück)

1. Maschine aus.
2. Kabel am alten Board abziehen und nach der Tabelle am neuen anstecken:
   gleiche GPIO-Nummer, gleiche Farbe.
3. Neues Board über die [Flash-Seite](https://larszu.github.io/bezzera-duo-protocol/)
   flashen; sie erkennt das Board selbst.
4. Einstellungen (WLAN, Passwort, Profile) liegen im Board und müssen einmal neu
   gesetzt werden.
