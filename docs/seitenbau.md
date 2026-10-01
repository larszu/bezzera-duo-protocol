# Eigene Display-Seiten

Stand: 🧪 gebaut, am Display ungetestet. Formate aus dem DGUS-Handbuch V4.3 und
dem ausgelesenen Display-Flash. Der Seitenbau reproduziert die Touch-Datei
byte-genau.

## Seitenbau

**[Seitenbau öffnen](https://larszu.github.io/bezzera-duo-protocol/seitenbau/)**
oder offline `tools/seitenbau/index.html` im Browser. Er läuft unter Windows, Mac und Linux und
braucht weder Installation noch SDK.

![Seitenbau](screenshots/seitenbau.png)

1. Display sichern: `python3 tools/display_sichern.py alles` ([Anleitung](display_sichern.md)).
2. Im Seitenbau `lib_013.bin` bis `lib_018.bin` aus der Sicherung laden (Variablen: je 64 Seiten eine Bibliothek).
3. Platz wählen (96–99, 196–199, 296–299), Hintergrund, Texte, Tasten und Zahlen setzen.
4. **DWIN_SET als ZIP** exportieren, den Ordner auf eine leere FAT32-SD-Karte kopieren,
   bei ausgeschalteter Maschine ins Display stecken, einschalten, warten, Karte ziehen, neu starten.

Die ZIP enthält das Hintergrundbild (`<seite>.bmp`, 24 Bit) und die vollständigen
`13.bin` (Touch) und `14.bin` (Anzeigen) aus der Sicherung. Darin sind nur die
Einträge der gewählten Seite ersetzt.

| Element | landet in | Wirkung |
|---|---|---|
| Text, Fläche, Knopfgrafik | Hintergrundbild | statisch, beliebige Schrift des Rechners |
| Taste: Seite wechseln | 13.bin, `Pic_Next` | das Display wechselt selbst |
| Taste: Bridge-Aktion | 13.bin, Tastencode `FD05` auf VP `0x0300` | die Bridge holt den Wert ab: 1 Ein, 2 Standby, 3 Bezug stoppen, 4 Tara, 10–29 Profil 1–20 |
| Taste: Tastencode | 13.bin, `FD05` auf VP `0x0000` | wie eine Originaltaste ans Mainboard |
| Zahl | 14.bin, Datenvariable `0x10`, Schrift aus einer vorhandenen Anzeige | Werte des Mainboards (`0x0053` …) oder der Bridge (`0x0310` …) |
| Zeichenfläche | 14.bin, Basic Graphics `0x21` | die Bridge zeichnet hinein (Linien `0x0002`, Rahmen `0x0003`, Flächen `0x0004`; Bild ausschneiden `0x0006` geht nicht) |

Werte der Bridge (nur solange eine eigene Seite zu sehen ist, alle 500 ms):
`0x0310` Gewicht g×10, `0x0311` Bezugszeit s×10, `0x0312` Bezüge seit Rückspülen,
`0x0313` Bezüge gesamt, `0x0314` Uhrzeit hhmm, `0x0315` aktives Profil.

**Risiko:** Die SD-Karte überschreibt Touch- und Anzeigekonfiguration aller
Seiten. Stammen 13/14.bin aus einer fehlerhaften Sicherung, sind auch die
Originalseiten betroffen. Dann mit der unveränderten Sicherung zurückspielen.

## Brühkurve (Beispiel, erprobt am Display 2.2)

Seite 196 zeigt Druck und Temperatur des laufenden bzw. letzten Bezugs, dazu
Gewicht, Bezugszeit, Höchstdruck und mittlere Brühtemperatur. Auf der Startseite
(1/101/201) führt eine Taste oben rechts dorthin; die Bridge zeichnet sie im Stil
der Menütaste. Bauen aus der eigenen Sicherung:

```
B=tools/seitenbau/beispiele
python3 tools/sd_paket.py flash/sicherung-… --ziel /Volumes/DWIN \
  --seite $B/bruehkurve.json --seite $B/start_1.json --seite $B/start_101.json \
  --seite $B/start_201.json --config R2=05
```

`R2=05` schaltet das Piepen beim Antippen aus. Nutzbar sind nur VPs bis `0x1FFF`
(ab `0x2000` Empfangspuffer, über `0x3FFF` gespiegelt); die Bridge nutzt `0x0300`
(Tasten), `0x0310`–`0x0317` (Werte), `0x0400`/`0x0480` (Kurve), `0x0500`/`0x0580`
(Startseiten-Taste).

## DGUS-SDK auf dem Mac

Für eigene Seiten nicht nötig. Wer Schriften oder Symbolbibliotheken bauen will:
`bash tools/dgus_sdk_mac.sh` richtet Wine und das SDK 5.10 ein, `… start` startet es.

## Lizenz

GPL-2.0 ([LICENSE](../LICENSE)).
