# Mitschnitte (Duo DE, 2026-09-27)

Rohdaten des Logic Analyzers (sigrok `.sr`, D0 = Display-TXD gelb,
D1 = Mainboard-TX weiß, abgegriffen an den Display-Pads). Auswerten mit

```bash
python3 tools/sr2log.py --baud 115200 captures/boot.sr > boot.log
python3 tools/duo_sniff.py dgus --changes boot.log
python3 tools/duo_live.py --replay captures/boot.sr
```

| Datei | Inhalt |
|---|---|
| `boot.sr` | 45 s ab Einschalten, 2 MHz: Startbild 90, Uhr stellen, Seite 101/103, dann Betrieb |
| `home.sr` | 3 s nach „OK“ auf „Bitte Tank füllen“. **Lief mit 1 MHz, ist aber mit 2 MHz beschriftet** (Abtastraten-Falle des Analyzers, README 2.3): mit `--baud 230400` dekodieren |
| `tank.sr` | 3 s nach Wackeln am Touch-Kabel, Display zeigt wieder „Bitte Tank füllen“ |
| `probe.sr` | Testaufnahme ohne Signal |
| `neukabel.sr` | Ersatzkabel mit Displayfarben direkt am Display, Analyzer parallel: D1 = TX Mainboard, D2 = TX Display (D0 ohne Daten) |
| `cnpr_einschalten.sr` | 25.10.2026, 35 s, 4 MHz, Ersatzboard an 12 V aus/ein, Masse an CNPR P: D0 = C (blau), D1 = N (grün), D3 = R (orange), siehe docs/mainboard.md |
| `usbdm_selbsttest.sr` | 25.10.2026, USBDM-Klon ohne Board, D0–D5 = Flachband-Ader 1–6: Sync-Pulse ~1 ms nur auf Ader 2 (BKGD), Ader 1 = GND |
| `adern.sr` | 5 s, 2 MHz, beim Zuordnen der Adern: auf D0 wiederholt das Mainboard alle 200 ms den Verbindungstest (VP `0x0063` = 21 schreiben und lesen), D1 ohne Daten |

Die Seitenfotos liegen verkleinert unter [`docs/seiten/`](../docs/seiten/).
