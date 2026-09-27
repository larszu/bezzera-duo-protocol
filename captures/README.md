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

Die Seitenfotos liegen verkleinert unter [`docs/seiten/`](../docs/seiten/).
