# Custom display pages and Doom

Status: 🧪 built, untested on the display. Formats from the DGUS guide V4.3 and
the dumped display flash. The page builder reproduces the touch file byte for byte.

## Page builder

**[Open the page builder](https://larszu.github.io/bezzera-duo-protocol/seitenbau/)**
or open `tools/seitenbau/index.html` offline. It runs on Windows, Mac and Linux, with no install and no SDK.

![Page builder](screenshots/seitenbau.png)

1. Back up the display: `python3 tools/display_sichern.py alles` ([guide](display_sichern.en.md)).
2. Load `lib_013.bin` to `lib_016.bin` from the backup into the builder.
3. Pick a slot (96–99, 196–199, 296–299; display 2.2 uses 198, 199, 298, 299 itself) and add a background, texts, buttons and numbers.
4. Export **DWIN_SET als ZIP**, copy the folder to an empty FAT32 SD card, insert it with the
   machine off, switch on, wait, remove the card, restart.

The ZIP holds the background (`<page>.bmp`, 24-bit) and complete `13.bin`
(touch) and `14.bin` (displays) from the backup with only the chosen page's entries replaced.

| Element | goes to | effect |
|---|---|---|
| text, shape, button graphic | background image | static, any font on the computer |
| button: change page | 13.bin `Pic_Next` | the display switches itself |
| button: bridge action | 13.bin key code `FD05` on VP `0x6100` | the bridge picks it up: 1 on, 2 standby, 3 stop brew, 4 tare, 5 Doom, 10–29 profile 1–20 |
| button: key code | 13.bin `FD05` on VP `0x0000` | like an original button, to the mainboard |
| number | 14.bin data variable `0x10`, font copied from an existing display | mainboard values (`0x0053` …) or bridge values (`0x6110` …) |
| drawing area | 14.bin basic graphics `0x21` | the bridge draws into it (Doom) |

Bridge values (only while a custom page is shown, every 500 ms):
`0x6110` weight g×10, `0x6111` brew time s×10, `0x6112` shots since backflush,
`0x6113` total shots, `0x6114` time hhmm, `0x6115` active profile.

**Risk:** the SD card overwrites the touch and display configuration of every
page. If 13/14.bin come from a bad backup, the original pages suffer too.
Restore from the unmodified backup.

## Doom

Tap the logo text on the home page (right of the menu button) ten times quickly,
on the display or in the web UI replica.

Requirements:
- **Doom page:** page builder → *Vorlage: Doom-Seite* → slot 297 → ZIP → SD card.
  It holds two drawing areas: VP `0x2000` (160×100 bitmap) and `0x6000` (zoom).
- **WAD file:** web UI → Diagnose → Doom → upload, e.g. the shareware `doom1.wad`
  (4 MB). It lives on the ESP32 only, never in the repo. Without a WAD you get the Doom fire.

Touch: top left menu (hold 3 s = back to the machine), top middle Enter, top
right Use; left/right turn, middle forward, below fire, bottom back. Keyboard in the web UI.

While Doom runs, the bridge answers the mainboard itself and remembers the page
it wants. A machine alarm, 3 minutes without touch, or "Zurück zur Maschine" ends
Doom. The game stays paused and the next easter egg resumes it. "Quit" in the
Doom menu needs a bridge restart afterwards.

| Item | Status |
|---|---|
| Doom (doomgeneric) on the ESP32-S3, tables in PSRAM | 🧪 compiles |
| bitmap command `0x000F`, zoom `0x0010` | 💡 per the guide; zoom behaviour needs a test |
| frame rate at 115200 baud | 💡 roughly 0.5–2 fps (only changed pixels are sent) |
| turbo: display briefly at 921600 baud (register R1, `0xA5` = not saved) | 🧪 off; power cycle restores 115200 |
| touch via registers `0x05`–`0x0A` | 💡 per the guide |

## DGUS SDK on a Mac

Not needed for custom pages. To build fonts or icon libraries:
`bash tools/dgus_sdk_mac.sh` sets up Wine and SDK 5.10, `… start` launches it.

## License

Doom comes from [doomgeneric](https://github.com/ozkl/doomgeneric) (GPL-2.0,
`bridge/duo_bridge/src/doom`, adapted: palette, PSRAM, exit). The whole project
is GPL-2.0 ([LICENSE](../LICENSE)).
