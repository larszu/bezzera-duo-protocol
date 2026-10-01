# Custom display pages

Status: 🧪 built, untested on the display. Formats from the DGUS guide V4.3 and
the dumped display flash. The page builder reproduces the touch file byte for byte.

## Page builder

**[Open the page builder](https://larszu.github.io/bezzera-duo-protocol/seitenbau/)**
or open `tools/seitenbau/index.html` offline. It runs on Windows, Mac and Linux, with no install and no SDK.

![Page builder](screenshots/seitenbau.png)

1. Back up the display: `python3 tools/display_sichern.py alles` ([guide](display_sichern.en.md)).
2. Load `lib_013.bin` to `lib_018.bin` from the backup into the builder (variables: 64 pages per library).
3. Pick a slot (96–99, 196–199, 296–299) and add a background, texts, buttons and numbers.
4. Export **DWIN_SET als ZIP**, copy the folder to an empty FAT32 SD card, insert it with the
   machine off, switch on, wait, remove the card, restart.

The ZIP holds the background (`<page>.bmp`, 24-bit) and complete `13.bin`
(touch) and `14.bin` (displays) from the backup with only the chosen page's entries replaced.

| Element | goes to | effect |
|---|---|---|
| text, shape, button graphic | background image | static, any font on the computer |
| button: change page | 13.bin `Pic_Next` | the display switches itself |
| button: bridge action | 13.bin key code `FD05` on VP `0x0300` | the bridge picks it up: 1 on, 2 standby, 3 stop brew, 4 tare, 10–29 profile 1–20 |
| button: key code | 13.bin `FD05` on VP `0x0000` | like an original button, to the mainboard |
| number | 14.bin data variable `0x10`, font copied from an existing display | mainboard values (`0x0053` …) or bridge values (`0x0310` …) |
| drawing area | 14.bin basic graphics `0x21` | the bridge draws into it (lines `0x0002`, frames `0x0003`, filled areas `0x0004`; image cut `0x0006` does not work) |

Bridge values (only while a custom page is shown, every 500 ms):
`0x0310` weight g×10, `0x0311` brew time s×10, `0x0312` shots since backflush,
`0x0313` total shots, `0x0314` time hhmm, `0x0315` active profile.

**Risk:** the SD card overwrites the touch and display configuration of every
page. If 13/14.bin come from a bad backup, the original pages suffer too.
Restore from the unmodified backup.

## Brew curve (example, tested on display 2.2)

Page 196 shows pressure and temperature of the running or last shot, plus weight,
shot time, peak pressure and mean brew temperature. On the home page (1/101/201) a
button at the top right leads there; the bridge draws it in the style of the menu
button. Build from your own backup:

```
B=tools/seitenbau/beispiele
python3 tools/sd_paket.py flash/sicherung-… --ziel /Volumes/DWIN \
  --seite $B/bruehkurve.json --seite $B/start_1.json --seite $B/start_101.json \
  --seite $B/start_201.json --config R2=05
```

`R2=05` turns off the touch beep. Only VPs up to `0x1FFF` are usable (receive
buffer from `0x2000`, mirrored above `0x3FFF`); the bridge uses `0x0300` (buttons),
`0x0310`–`0x0317` (values), `0x0400`/`0x0480` (curve), `0x0500`/`0x0580` (home button).

## DGUS SDK on a Mac

Not needed for custom pages. To build fonts or icon libraries:
`bash tools/dgus_sdk_mac.sh` sets up Wine and SDK 5.10, `… start` launches it.

## License

GPL-2.0 ([LICENSE](../LICENSE)).
