<p align="right"><a href="pinbelegung.md">Deutsch</a> · <b>English</b></p>

# Pinout: ESP32-S3-ETH and classic ESP32

The firmware uses **the same GPIO numbers** on both boards. When moving from one
board to the other the wires stay on the same signals; only position and
silkscreen differ.

| Signal | GPIO | Waveshare ESP32-S3-ETH | classic ESP32 (DevKit, 30 pins) | Wire |
|---|---|---|---|---|
| RX from display | **15** | pin "15" | **D15** (IO15/G15), right row, 13th from top | display **yellow** |
| TX to mainboard | **16** | pin "16" | **RX2** (GPIO16), right row, 10th from top | mainboard **green** (spare cable yellow) |
| RX from mainboard | **17** | pin "17" | **TX2** (GPIO17), right row, 9th from top | mainboard **pink** (white) via 10 kΩ, 20 kΩ to GND |
| TX to display | **18** | pin "18" | **D18** (IO18), right row, 7th from top | display **white** |
| Ground | GND | "GND" | **GND**, right row, 14th from top (or bottom left) | mainboard black + display brown |
| 5 V (only without machine) | – | "5V" | **VIN**, left row bottom | display green, only without machine |

Classic DevKit, antenna up, USB down; right row from top: D23, D22, TX0, RX0,
D21, D19, **D18**, D5, **TX2**, **RX2**, D4, D2, **D15**, **GND**, 3V3. Other
variants (38 pins, DevKitC) carry the same GPIO names elsewhere: follow the
labels, not the position.

Classic ESP32 caveats: "RX2"/"TX2" are just names (GPIO16 sends to the
mainboard, GPIO17 receives); boards with PSRAM (WROVER) use GPIO16/17 internally
and do not work; keep the 10 kΩ/20 kΩ divider on the mainboard input; no
Ethernet, no Matter, updates only via the flash page.

Switching boards: machine off, move each wire to the same GPIO on the new board,
flash it via the [flash page](https://larszu.github.io/bezzera-duo-protocol/),
set Wi-Fi, password and profiles again.
