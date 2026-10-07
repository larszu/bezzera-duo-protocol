<p align="right"><a href="flashen.md">Deutsch</a> · <b>English</b></p>

# Getting the firmware onto the ESP32 and setting it up

The bridge runs on two boards:

| Board | What works |
|---|---|
| **Waveshare ESP32-S3-ETH** (ESP32-S3, 8 MB PSRAM, 16 MB flash, Ethernet), recommended | everything: Matter, Ethernet, update via the web UI, 24 h history |
| **classic ESP32** (ESP32-D0WD/WROOM-32, 4 MB flash, USB via CP2102/CH340) | everything except Matter, Ethernet and update via the web UI; 5 min history |

The flash page detects the board by itself. Build both locally with
`bash tools/firmware_bauen.sh`. Three ways, from simplest to most flexible.

## 1. In the browser (recommended)

1. Open **Chrome or Edge** on a computer (Safari, Firefox and phones have no Web Serial).
2. Open the flash page: **https://larszu.github.io/bezzera-duo-protocol/**
3. Plug in the ESP32 via USB-C, click **Installieren** (install), pick the port
   (usually “USB JTAG/serial debug unit”).
4. After flashing the page asks for your home Wi-Fi (Improv). Pick the network, enter the password.
5. The bridge is then reachable at **http://duo.local**.

GitHub rebuilds the page on every change to `bridge/`
(`.github/workflows/firmware.yml`); each version tag also gets the file as a release.

## 2. With Arduino CLI (from source)

```bash
brew install arduino-cli                       # macOS; Linux/Windows: arduino.github.io/arduino-cli
arduino-cli core install esp32:esp32@3.3.12
FQBN=esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=custom,PSRAM=opi
arduino-cli compile --fqbn $FQBN bridge/duo_bridge
arduino-cli upload  --fqbn $FQBN -p /dev/cu.usbmodem… bridge/duo_bridge   # Windows: -p COM5
```

- `FlashSize=16M,PartitionScheme=custom`: the own layout
  `bridge/duo_bridge/partitions.csv` (two 6 MB app slots, room for Bluetooth and
  Matter). The settings storage stays where it was, stored settings survive the change.
- `PSRAM=opi`: the ESP32-S3R8 on this board. Without PSRAM everything works,
  the history just covers 30 minutes instead of 24 hours.
- `CDCOnBoot=cdc`: output and commands over the USB-C port.

## 3. With esptool (ready-made file from a release)

```bash
pip install esptool
esptool --chip esp32s3 --port /dev/cu.usbmodem… write-flash 0x0 duo_bridge-vX.Y.bin
```

## If the ESP32 is not detected

- Try another USB cable: many only charge.
- Force download mode: hold **BOOT**, tap **RESET**, release BOOT, then flash. Press RESET afterwards.
- macOS asks “Allow accessory to connect?” the first time – allow it.

## First setup without a computer

1. Connect your phone to the Wi-Fi **duo-bridge**, password **espresso1**.
2. The page opens as a sign-in portal by itself (otherwise open any address, e.g. http://espresso.maschine).
3. **Einstellungen → Heim-WLAN → Netze suchen** (settings → home Wi-Fi → search), tap your network, enter the password.
4. Under **Zugang** (access) change the bridge's Wi-Fi password and set a password for the page
   (user `duo`). The default password is public in this repo.

Over USB: `n <ssid> <password>` (see README, USB commands).

## Afterwards

- Wiring to the machine: README, ESP32 bridge → wiring.
- Scale: **Einstellungen → Waage → Bluetooth-Waage → Waage suchen**, tap the scale.
- From outside: [`fernzugriff.en.md`](fernzugriff.en.md).
