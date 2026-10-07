<p align="right"><b>Deutsch</b></p>

# Display: Originalseiten und was eigene Seiten besser können

Grundlage: Seitenkatalog [seiten.md](seiten.md), Fotos in `captures/seiten`,
Messungen am Display 2.2. Originalseiten werden nicht verändert: Ihre Bilder
lassen sich nicht auslesen, und Ersatzdisplays unterscheiden sich. Verbesserungen
entstehen auf den freien Plätzen (96–99, 196–199, 296–299) und erreicht man über
eine zusätzliche Touch-Fläche auf einer Originalseite (wie die Brühkurve auf der
Startseite).

## Vergleich

| Original | Schwäche | Eigene Seite / Bridge | Stand |
|---|---|---|---|
| Ausgabeseite x06: Zeiger Pumpendruck 0–10 bar, große Zahl (Zeit) | nur der Augenblick, kein Verlauf, kein Gewicht | **Brühkurve 196**: Druck- und Temperaturkurve, Zeit, Gewicht, Höchstdruck, Ø Temperatur, Tara; bleibt beim Bezug offen | erledigt |
| Startseite x01: zwei Zeiger, Druckskala 0–2,5 bar | Zeiger schwer abzulesen, Sollwert nicht sichtbar | Taste oben rechts zur Brühkurve; Sollwert und „bereit“ zeigt die Bogenanzeige in der App | erledigt |
| Wartung x54/x55: Zähler Bezüge, Filtertage | nur Zahl, keine Erinnerung | Rückspül-Zähler und Erinnerung in der App, Rückspülen wird erkannt; ntfy-Meldung „Wartung“ | erledigt |
| Wash x64–x69: Rückspülen | Ablauf nur auf dem Display | Rückspül-Anleitung in der App (Reiter Maschine) | erledigt |
| Datum/Uhrzeit x58–x62: fünf Seiten, je Feld ±1 | mühsam | Uhr von Handy stellen (App), solange die Maschine an ist | Display 2.2: Mainboard nimmt es nicht an, offen |
| Profile gibt es nicht | Temperatur und Vorbrühen je Bohne umstellen ist Handarbeit | Profile in der App, „auf die Maschine“ schreibt die Werte | erledigt |
| Alarme x02–x04, x56, x77, x89 | Text ohne Hilfe | ntfy-Meldung „Alarm“ mit Seitentext | erledigt |

## Weitere Ideen (nicht umgesetzt)

- **Übersichtsseite Zähler/Wartung** (197): Bezüge gesamt, seit Rückspülen, Filtertage, nächster Termin; Werte liefert die Bridge (`0x0312`, `0x0313`).
- **Profil-Schnellwahl** (198): vier Tasten „Bridge-Aktion 10–13“ wenden Profil 1–4 an.
- Alles weitere bewusst in der App statt am Display: mehr Platz, keine SD-Karte nötig.
