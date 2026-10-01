# Backing up the display and custom pages

Status: 🧪 built, untested. Everything runs through the bridge, no SDK needed.

## Backup (read-only)

The bridge firmware reads by itself (USB command `S`, [`sichern.h`](../bridge/duo_bridge/sichern.h)):
read request to register `0x40` (library) or `0x56` (database), wait until the
register is 0 again, fetch 32 words from VP `0x1000`. Each block is read twice;
only matching reads are emitted. The computer only collects.

```sh
python3 tools/display_sichern.py suche   # which libraries 0–127 have content
python3 tools/display_sichern.py alles   # all with content, 128 KB each, + image probes 128/201/296
```

Stored in `flash/sicherung-<date>/` with `manifest.json` (SHA-256), not in the repo.
During the backup the bridge is in mode 1 (answers the mainboard itself) and
returns to its previous mode afterwards. At 115200 baud one library takes about a minute.

## Free pages

The display has 300 pages; 96–99, 196–199 and 296–299 are empty. A page needs three parts:

| Part | Where | Writable without SDK? |
|---|---|---|
| Background image | image memory; from image 128 via the database (register `0x56`, mode `0x50`) | 💡 probably yes for 196–199 and 296–299; not 96–99 |
| Touch areas | library 13 | 💡 per the manual only libraries `0x40–0x7F` are LibOP-writable → probably not |
| Variable displays | library 14 | same as 13 |

Without touch/variable entries a copied page only shows the image. Buttons on
it need an SD card (`DWIN_SET` with 13/14.bin) or the DGUS SDK (Windows only).

## Test: copy a page

Only after a full backup and with explicit go:

```sh
python3 tools/seite_kopieren.py 201 296 --sicherung flash/sicherung-…            # dry run
python3 tools/seite_kopieren.py 201 296 --sicherung flash/sicherung-… --wirklich
```

The tool aborts if the target isn't empty in the backup, the source reads empty
(then the image-location assumption is wrong) or a read-back block differs.

**Risk:** writing display flash can brick the display. Keep a spare at hand.
