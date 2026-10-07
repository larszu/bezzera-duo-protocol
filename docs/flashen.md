<p align="right"><b>Deutsch</b> · <a href="flashen.en.md">English</a></p>

# Firmware auf den ESP32 bringen und einrichten

Die Bridge läuft auf zwei Boards:

| Board | Was geht |
|---|---|
| **Waveshare ESP32-S3-ETH** (ESP32-S3, 8 MB PSRAM, 16 MB Flash, Ethernet), empfohlen | alles: Matter, Netzwerkkabel, Update über die Weboberfläche, 24 h Verlauf |
| **klassischer ESP32** (ESP32-D0WD/WROOM-32, 4 MB Flash, USB über CP2102/CH340) | alles außer Matter, Netzwerkkabel und Update über die Weboberfläche; Verlauf 5 min |

Die Flash-Seite erkennt das Board selbst. Drei Wege, vom einfachsten zum flexibelsten.

## 1. Im Browser (empfohlen)

1. Am Rechner **Chrome oder Edge** öffnen (Safari, Firefox und Handys können
   kein Web Serial).
2. Die Flash-Seite öffnen: **https://larszu.github.io/bezzera-duo-protocol/**
3. ESP32 per USB-C anstecken, **Installieren** klicken, Anschluss wählen
   (meist „USB JTAG/serial debug unit“).
4. Nach dem Flashen fragt die Seite nach dem Heim-WLAN (Improv). Netz wählen,
   Passwort eingeben.
5. Die Bridge ist danach unter **http://duo.local** erreichbar.

Die Seite baut GitHub bei jeder Änderung an `bridge/` neu
(`.github/workflows/firmware.yml`); zu jedem Versions-Tag gibt es die Datei
zusätzlich als Release.

## 2. Mit Arduino CLI (aus dem Quelltext)

```bash
brew install arduino-cli                       # macOS; Linux/Windows: arduino.github.io/arduino-cli
arduino-cli core install esp32:esp32@3.3.12
bash tools/firmware_bauen.sh                   # baut build/s3 und build/esp32
esptool --chip esp32s3 --port /dev/cu.usbmodem…  write-flash 0x0 build/s3/duo_bridge.ino.merged.bin
esptool --chip esp32   --port /dev/cu.usbserial… write-flash 0x0 build/esp32/duo_bridge.ino.merged.bin
```

Der klassische ESP32 wird aus einer Kopie ohne `partitions.csv` gebaut
(`PartitionScheme=huge_app`, 3 MB Programm, kein zweiter Programmplatz).

- `FlashSize=16M,PartitionScheme=custom`: die eigene Aufteilung
  `bridge/duo_bridge/partitions.csv` (zwei App-Bereiche à 6 MB, genug für Bluetooth
  und Matter). Der Einstellungsspeicher liegt an derselben Stelle wie vorher,
  gespeicherte Einstellungen bleiben beim Wechsel erhalten.
- `PSRAM=opi`: der ESP32-S3R8 dieses Boards. Ohne PSRAM läuft alles, nur der
  Verlauf reicht dann 5 Minuten statt 24 Stunden.
- `CDCOnBoot=cdc`: Ausgabe und Befehle über den USB-C-Anschluss.

## 3. Mit esptool (fertige Datei aus einem Release)

```bash
pip install esptool
esptool --chip esp32s3 --port /dev/cu.usbmodem…  write-flash 0x0 duo_bridge-esp32s3-vX.Y.bin
esptool --chip esp32   --port /dev/cu.usbserial… write-flash 0x0 duo_bridge-esp32-vX.Y.bin
```

Update eines laufenden ESP32-S3 ohne Kabel: Weboberfläche → Diagnose → Firmware,
Datei `duo_bridge-esp32s3-update-vX.Y.bin`.

## Wenn der ESP32 nicht erkannt wird

- Anderes USB-Kabel: Viele Kabel laden nur.
- Download-Modus erzwingen: **BOOT** gedrückt halten, **RESET** kurz drücken,
  BOOT loslassen, dann flashen. Danach einmal RESET.
- macOS fragt beim ersten Anstecken „Zubehör verbinden?“ – erlauben.

## Ersteinrichtung ohne Rechner

1. Handy mit dem WLAN **duo-bridge** verbinden, Passwort **espresso1**.
2. Die Seite öffnet sich als Anmeldeportal von selbst (sonst irgendeine
   Adresse aufrufen, z. B. http://espresso.maschine).
3. **Einstellungen → Heim-WLAN → Netze suchen**, Netz antippen, Passwort.
4. Unter **Zugang** das WLAN-Passwort der Bridge ändern und ein Passwort für
   die Seite setzen (Benutzer `duo`). Das Standardpasswort steht öffentlich in
   diesem Repo.

Per USB geht es auch: `n <ssid> <passwort>` (siehe README, Befehle über USB).

## Danach

- Verkabelung zur Maschine: README, Abschnitt ESP32-Bridge → Verkabelung.
- Waage: **Einstellungen → Waage → Bluetooth-Waage → Waage suchen**, Waage antippen.
- Von unterwegs: [`fernzugriff.md`](fernzugriff.md).
