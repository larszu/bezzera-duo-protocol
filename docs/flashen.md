<p align="right"><b>Deutsch</b> · <a href="flashen.en.md">English</a></p>

# Firmware auf den ESP32 bringen und einrichten

Die Bridge läuft auf einem **Waveshare ESP32-S3-ETH** (ESP32-S3 mit 8 MB PSRAM,
16 MB Flash, Ethernet). Drei Wege, vom einfachsten zum flexibelsten.

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
FQBN=esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs,PSRAM=opi
arduino-cli compile --fqbn $FQBN bridge/duo_bridge
arduino-cli upload  --fqbn $FQBN -p /dev/cu.usbmodem… bridge/duo_bridge   # Windows: -p COM5
```

- `PartitionScheme=min_spiffs`: Mit Bluetooth passt die Firmware nicht in die
  Standardaufteilung. Beim ersten Wechsel der Aufteilung gehen gespeicherte
  Einstellungen verloren.
- `PSRAM=opi`: der ESP32-S3R8 dieses Boards. Ohne PSRAM läuft alles, nur der
  Verlauf reicht dann 30 Minuten statt 24 Stunden.
- `CDCOnBoot=cdc`: Ausgabe und Befehle über den USB-C-Anschluss.

## 3. Mit esptool (fertige Datei aus einem Release)

```bash
pip install esptool
esptool --chip esp32s3 --port /dev/cu.usbmodem… write-flash 0x0 duo_bridge-vX.Y.bin
```

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
