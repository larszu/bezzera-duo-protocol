<p align="right"><a href="stand.md">Deutsch</a> · <b>English</b></p>

# What is certain and what is not

Every statement in this repo belongs to exactly one of three levels:

| Mark | Level | Meaning |
|---|---|---|
| ✅ | **tested** | observed on the real Duo DE (built 2018) or the real display: capture, measurement, photo, read from flash or tried out. The evidence is listed. |
| 🧪 | **built, not yet tested** | useful and implemented (compiles, partly checked in the simulation `tools/web_lokal.py --demo`), but never tried on the machine or with real add-on hardware |
| 💡 | **idea / assumption** | derived from the manual, photos, datasheets, dealer information or analogy. Neither measured nor built, or built but the effect on the machine is unknown. |

“According to the manual” or “according to the datasheet” is 💡 as long as nobody
has observed it on this machine. All captures in [`captures/`](../captures/) come
from the **cold machine with an empty tank**; a shot has never been on the line.

---

## Display and hardware

| | Statement | Evidence |
|---|---|---|
| ✅ | The display is a DWIN **DMT32240M035_07WTZ4**, date code 180814, “5V ONLY” | photo of the back |
| ✅ | Wires at the display: GND brown, TXD yellow, RXD white, VCC green; at mainboard connector CN6: red +5 V, pink mainboard → display, green display → mainboard, black GND | continuity-tested, `neukabel.sr` |
| ✅ | TTL levels: display TXD +3.2 V, mainboard TX +4.8 V, no RS-232 | multimeter |
| ✅ | Display firmware (register `0x00`) = `0x22`, boot screen “TFT 2.0” | read |
| ✅ | Memory is two TSOP-48 chips, no SPI flash; coin-cell holder empty, clock still correct | photo, observed |
| ✅ | Touch works: wiggling the 6-pin connector makes the display beep; last real touch at x=286, y=234 (“OK”) | observed, touch registers read |
| ✅ | Touch controller **SiS9252** on flex cable `CDQ9439-3.5-A` | photo (marking hard to read) |
| ✅ | Mainboard by **PRO.EL.IND**, MCU MC9S08PA32, RTC M41T56, label pinout (relays, probes, PRESS, CAP. SENS, KEYBOARD) | photos |
| ✅ | The mainboard sets the display clock at startup | `boot.sr` |
| 💡 | The time comes from the M41T56 | part on the photo |
| 💡 | CNPR is the BDM programming header; 5 pads on the display are a debug header | analogy |
| 💡 | The LCD panel is an LQ035NC111 | label cut off |
| 💡 | The inline connector is a Molex Mini-Fit Jr. (4.2 mm) | keying on the photo, pitch not measured |
| 💡 | Replacement touch panels with FocalTech/Goodix controllers will not work | assumption about the display firmware |
| 💡 | Everything else in [`bauteile.md`](bauteile.md) (German): “belegt” there only means “confirmed by manufacturer or dealer pages”, not measured on this machine | research |

## Protocol

| | Statement | Evidence |
|---|---|---|
| ✅ | DWIN Mini DGUS with header **`C6 A5`**, 115200 8N1, no CRC | all captures |
| ✅ | Boot sequence: page 90, connection test VP `0x0063` = 21, set clock, page 101, VP `0x0000` = 1, page 103 when the tank is empty | `boot.sr` |
| ✅ | The mainboard reads VP `0x0000` and `0x0001` every 100 ms | `boot.sr`, `home.sr`, `tank.sr` |
| ✅ | VP `0x0000` = 5 after “OK” on „Bitte Tank füllen“ (please fill the tank) and stays there | `home.sr`, `tank.sr` |
| ✅ | When going to standby the mainboard itself writes page 100 and VP `0x0000` = 0; in standby it sets the clock every second | bridge log 2026-09-30 |
| ✅ | VP `0x0050` every ~300 ms, word 3 coffee boiler °C, word 4 service boiler °C | captures, matched with test values on the display |
| ✅ | VP `0x0063` = mainboard firmware × 10 (21 → “FW: 2.1”) | `boot.sr`, boot screen |
| 💡 | Dealer version table and reasons for incompatibilities: [`versionen.en.md`](versionen.en.md) | dealer information, conclusion |
| ✅ | Word 2 becomes 1 after page 103, word 7 = 3, word 5 was 1 once for 1 s; words 0, 1, 6, 8 always 0 | `boot.sr` |
| 💡 | Meaning of words 2, 5 and 7 | — |
| 🧪 | Word 5 = pump pressure (0.5 bar per unit), word 6 = service boiler pressure (0.25 bar per unit), all words in [`variablen.en.md`](variablen.en.md) | `14.bin` (gauge configuration); word 6 = 6 at 127 °C matches 1.5 bar; no shot captured yet |
| ✅ | The display reports nothing on its own; it changes pages on touch without telling the mainboard | captures, key type `FDxx` in flash |
| ✅ | Register `0x4F` and touch registers `0x05`–`0x07` do not trigger a key press | tried (`tools/tasten_scan.py`) |
| ✅ | The mainboard reads VP `0x0002` on settings pages and answers 1 with the settings block `0x0020`–`0x002D` | bridge log 2026-09-30 |

## Pages and keys

| | Statement | Evidence |
|---|---|---|
| ✅ | 300 pages in three languages, photos and catalogue | `docs/seiten/`, `docs/seiten.md` |
| ✅ | 1070 keys with area, next page, function, value | read from the display flash, `docs/tasten.json` |
| ✅ | „Für Start drücken“ (press to start) writes VP `0x0000` = 1 (DE, IT), “Standby” in the side menu writes 0 (all languages) | flash |
| ✅ | English page 9: one byte damaged in flash (`F3` instead of `FE`) | flash, read twice |
| 💡 | Therefore the “Coffee” key does not work there | conclusion |
| ✅ | Page x06 is the dispensing counter: pump pressure gauge (VP `0x0055`), seconds since shot start (VP `0x0059`), temperature | `14.bin` + shot on 2026-09-30 (`0x0059` = 12) |

## ESP32 bridge

| | Statement | Evidence |
|---|---|---|
| ✅ | Pass-through and capturing on the running machine | operated on the machine |
| ✅ | The firmware with web UI starts on the ESP32-S3-ETH, own Wi-Fi `duo-bridge`; the ESP32 does not see the home Wi-Fi (Vodafone), it only supports 2.4 GHz | USB output, Wi-Fi scan |
| ✅ | A wrong divider on GPIO17 (10k/20k swapped) gives garbage; swapping display brown/green reverses the display's polarity | happened |
| 🧪 | Display emulation (mode 1): answers byte for byte like the display | test commands `M`/`D` |
| ✅ | Without the display's reply line the machine does not continue booting; with the display in parallel to the ESP32 it crashes | observed |
| 💡 | The cause was the missed connection test VP `0x0063` | conclusion |
| 🧪 | Hybrid mode (mode 2) and key injection (`o`, `w` on VP `0x0000`) | compiles |
| ✅ | The mainboard reacts to a key value set from outside like to a real press: start (VP `0x0000` = 1 + page 101 → status words and page 103 after 57 ms), standby (= 0 + page 100 → mainboard confirms page 100 after 145 ms), OK on alarm (= 5), settings OK (`0x0002` = 1 → settings block) | bridge log 2026-09-30 |
| ✅ | Web UI with display replica on the real display: live values, page changes, VPs read from the display (PID values `0x0076`–`0x0078`) | screenshot of 2026-09-27 |
| 🧪 | Local web UI over USB (`tools/web_lokal.py`) | script |
| ✅ | Tests of the Python tools (`python3 -m unittest discover -s tests`) | 26 tests pass |

## Beyond the display

| | Statement | Evidence |
|---|---|---|
| ✅ | Remote on/off (VP `0x0000` = 1/0 plus page x01/x00) | 2026-09-30 on the machine, also from the web UI |
| ✅ | Set the clock from phone/computer (VP `0x002E`–`0x0032`, then `0x0002` = 1) | 2026-09-30, display clock follows |
| ✅ | The machine only heats when word 2 (tank empty) = 0; after filling and off/on the pump fills the boiler, then both boilers heat (words 0/1 = 2) | 2026-09-30 |
| 🧪 | On/standby state from the key value instead of the page | built |
| 🧪 | Alarm report from the page | built; ✅ that the mainboard switches to page 103 itself (`boot.sr`) |
| 🧪 | History of VP `0x0050` in the ESP32 (24 h with PSRAM) | built, UI checked in the simulation |
| 🧪 | Pressure curves | built; pressure word unknown (💡 above) |
| 🧪 | Home Assistant via MQTT discovery | built; never run against a broker |
| 🧪 | Bluetooth scales Acaia, Bookoo, Felicita, Decent | built after [AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE), BooKoo and Decent docs; not tested with any real scale |
| 🧪 | Wi-Fi scales (poll a URL, `POST /api/waage`, MQTT) | built |
| 🧪 | Shot detection from the first drops, learning the stop offset | built, checked in the simulation |
| 🧪 | Stop output: short key press, only during a running shot (≥ 0.5 g/s) | built |
| 💡 | A second press on the continuous-dispense button stops dispensing | manual 5.4, not tried on this machine |
| 💡 | Stop button via PhotoMOS relay in parallel to the continuous-dispense button | concept; keypad cable pinout unknown |
| 💡 | Detecting a shot from page x06 | built (🧪), but relies on the 💡 assumption about page x06 |
| 💡 | Standby in the middle of a shot stops the pump | idea only, not built |

## Open, not built

| | Statement |
|---|---|
| 💡 | Continuous operation: web UI password, switchable own Wi-Fi, OTA, watchdog, power from the machine |
| 💡 | Read the variable configuration `14.bin` from flash (shows which VP is displayed where) |
| 💡 | Capture the SiS9252 touch controller over I²C and emulate it |

---

**The capture that would clarify most:** a full shot with a filled tank, started
with the continuous-dispense button. It settles the pressure word, page x06, the
VP of the dispensing time and whether word 5 is the pump.
