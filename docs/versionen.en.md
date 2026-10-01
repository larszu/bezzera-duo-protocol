<p align="right"><a href="versionen.md">Deutsch</a> · <b>English</b></p>

# Firmware versions of mainboard and display

Levels as in [`stand.en.md`](stand.en.md): ✅ measured on this machine · 💡
from dealer information or conclusion.

## This machine (Duo DE, 2018)

| | | Evidence |
|---|---|---|
| ✅ | Mainboard reports **FW 2.1** (VP `0x0063` = 21 at startup) | `boot.sr`, boot screen “FW: 2.1” |
| ✅ | Display project **“TFT 2.0”** (boot screen) | photo of page 90 |
| ✅ | DWIN operating software in the display: register `0x00` = `0x22` | read; this is DWIN's version, not Bezzera's |
| ✅ | Label on the mainboard: `7661047PR`, production `1809` | photo |
| ✅ | The two work together | the machine runs |
| 💡 | According to the table below, `7661047PR` belongs to FW 1.2, but FW 2.1 to `7661047.02PR`. Either the label only carries the base number or the mainboard was updated later | label ↔ reported version mismatch |

## Versions according to dealers (💡)

From the overview at [1st-line](https://www.1st-line.com/technical-support/bezzera-technical-support/bezzera-matrix-duo-software-compatibility-changes/)
(page only reachable after a browser check, content read via search results)
and spare parts dealers:

| Mainboard | Part number | Display | Part number | Changes |
|---|---|---|---|---|
| 1.2 | 7661047PR | 1.1 | 5963169.01 | first release |
| 2.0 | 7661047.01PR | 2.0 | 5963169.02 | Auto ON/OFF fixed, boiler fill timeout 15 → 30 s, machine starts by itself after 5 s (no press on the standby screen) |
| **2.1** | 7661047.02PR | **2.0** | 5963169.03 | group heating corrected — **this machine** |
| 2.2 | 7661047.03PR | 2.2 | 5963201.01 | steam boiler not filled or heated during a shot, stand-by bugs fixed |
| 2.3 | 7661047.04PR | 2.2 | 5963201.01 | bug in the test procedure fixed |
| 2.4 | 7661047.05PR | 2.2 | 5963201.01 | German version pre-infusion: 1 s during brewing, 2 s during auto-learn — made consistent |

Further information:
- [Avola Coffeesystems](https://www.avola-coffeesystems.de/bezzera-elektronikbox-duo-matrix-sw-v-2-4-elektronik/8281091): electronics firmware 2.0 and later is compatible with displays 2.0 and later; electronics 1.x or 3.x are not.
- Display assemblies: [5963202R for software 1.1](https://www.espresso.co.nz/parts-care/display-assembly-with-integrated-touchscreen-v1-1-duo-matrix-bezzera-5963202r/), [5963201.01R for software 2.2](https://www.espresso.co.nz/parts/display-assembly-with-integrated-touchscreen-v2-2-duo-matrix-bezzera-5963201-01r/); both with separately mounted glass (successors of 5963187R).
- A mainboard version 3.x is mentioned but not described anywhere.

## Where the incompatibilities come from (💡, concluded from the protocol)

The display has no logic of its own. But mainboard firmware and display
project share an **implicit contract** that nothing checks:

1. **Page numbers.** The mainboard switches pages itself (90, 101, 103, alarms). If the display project has a different page or none under that number, it shows the wrong thing.
2. **Key codes.** Every key writes a fixed value to VP `0x0000` (e.g. 7 = coffee settings, 20 = menu). New mainboard functions need new codes and new pages in the display; old displays never send them.
3. **Variable addresses.** Temperatures, setpoints and settings live at fixed VPs (`0x0050`, `0x005A`–`0x005F`, `0x0070`–`0x007E` …). If one version moves a VP, the other side reads or shows the wrong value.
4. **Sequences.** Mainboard 2.0 starts by itself after 5 s instead of waiting for the standby press; display project 2.0 adapts its page flow. A display 1.1 on a mainboard 2.x expects a different sequence.
5. **Header `C6 A5` instead of `5A A5`.** A stock DWIN display does not talk to this mainboard at all; the frame header is part of the display project's configuration. A replacement display 2.2 (version `0x25` in register `0x00`) uses `5A A5` instead and stays silent on mainboard 2.1: pages appear, touch beeps, but no values, because the mainboard never gets an answer to its connection test (item 6). The bridge detects the display header itself and translates in both directions (`?` shows `display_kopf=5A`).

   | | Display 2.0 | Replacement display 2.2 |
   |---|---|---|
   | Frame header (R3, RA) | `C6 A5` | `5A A5` |
   | Registers `0x10`–`0x1C` | – | `07 07 04 5A 00 FF 40 20 0A FF A5 00 00` |
   | Baud (R1) | 115200 | 115200 (`07`) |

   R3 cannot be rewritten at runtime (writing `0x13` and CONFIG_EN `0x1D` have no effect, measured). Without the bridge a 2.2 display therefore needs a changed configuration via SD card, tested on 2026-10-01:

   1. Back up first: `python3 tools/display_sichern.py alles` (needs the bridge, ~30 min).
   2. SD card FAT32 with 4 KB clusters, at most 8 GB or a 4 GB partition (`diskutil partitionDisk diskN MBR FAT32 DWIN 4G "Free Space" LEER R`).
   3. Copy the folder [`tools/display_kopf_c6/DWIN_SET`](../tools/display_kopf_c6/DWIN_SET/CONFIG.TXT) to the root. It only contains `CONFIG.TXT`: the previous values with R3 = `C6`. Pages and images stay untouched.
   4. Insert the card into the display with the machine on, pull it after a few seconds, switch the machine off and on.

   Afterwards register `0x13` reads `C6`, also after the restart, and display and mainboard talk without the bridge.
6. **Connection test VP `0x0063`.** The mainboard writes its version there and reads it back. From everything captured, it only checks that a display answers, not its version. A mismatched pair therefore probably boots and only then misbehaves.

Displays 2.0 and 2.2 probably differ in pages and keys for the new functions
(such as the steam boiler during a shot). Reading the touch configuration of a
2.2 display with `tools/libop_lesen.py` and comparing it with
[`tasten.json`](tasten.json) (project 2.0) would show the exact differences.

**For the bridge this means:** it was measured against mainboard 2.1 and
display 2.0. Page numbers, key codes and VPs may differ in other versions.
