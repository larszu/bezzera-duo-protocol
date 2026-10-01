# Display sichern und eigene Seiten

Stand: 🧪 gebaut, ungetestet. Alles hier läuft über die Bridge und braucht kein SDK.

## Sichern (nur lesend)

Die Bridge-Firmware liest selbst (USB-Befehl `S`, [`sichern.h`](../bridge/duo_bridge/sichern.h)):
Leseauftrag an Register `0x40` (Bibliothek) bzw. `0x56` (Datenbank), warten bis
das Register wieder 0 ist, 32 Worte aus VP `0x1000` holen. Jeder Block wird
zweimal gelesen, nur gleiche Lesungen gehen raus. Der Rechner sammelt nur ein.

```sh
python3 tools/display_sichern.py suche   # welche Bibliotheken 0–127 Inhalt haben
python3 tools/display_sichern.py alles   # alle mit Inhalt, je 256 KB, + Bildproben 128/201/296
```

Ablage: `flash/sicherung-<Datum>/` mit `manifest.json` (SHA-256), nicht im Repo.
Während der Sicherung steht die Bridge in Modus 1 (beantwortet das Mainboard
selbst), danach wieder im alten Modus. Bei 115200 Baud dauert eine Bibliothek
rund eine Minute.

## Freie Seiten

Das Display hat 300 Seiten; 96–99, 196–199 und 296–299 sind leer. Eine eigene
Seite braucht drei Teile:

| Teil | Wo | Ohne SDK schreibbar? |
|---|---|---|
| Hintergrundbild | Bildspeicher; ab Bild 128 über die Datenbank (Register `0x56`, Modus `0x50`) | 💡 vermutlich ja, für 196–199 und 296–299; 96–99 nicht |
| Touchbereiche | Bibliothek 13 | 💡 laut Handbuch nur Bibliotheken `0x40–0x7F` per LibOP schreibbar → eher nein |
| Variablenanzeigen | Bibliothek 14 | wie 13 |

Ohne Touch-/Variableneinträge zeigt eine kopierte Seite nur das Bild. Für
Tasten auf der neuen Seite braucht es SD-Karte (`DWIN_SET` mit 13/14.bin) oder
das DGUS-SDK (nur Windows); die Bridge kann Touch aber auch selbst auswerten,
wenn das Display Koordinaten meldet — offen.

## Test: Seite kopieren

Erst nach vollständiger Sicherung und mit Freigabe:

```sh
python3 tools/seite_kopieren.py 201 296 --sicherung flash/sicherung-…            # Trockenlauf
python3 tools/seite_kopieren.py 201 296 --sicherung flash/sicherung-… --wirklich
```

Das Werkzeug bricht ab, wenn das Ziel in der Sicherung nicht leer ist, die
Quelle leer gelesen wird (dann stimmt die Annahme zur Bildlage nicht) oder ein
zurückgelesener Block abweicht.

**Risiko:** Schreiben ins Display-Flash kann das Display unbrauchbar machen.
Ein Ersatzdisplay bereitzuhalten ist sinnvoll.
