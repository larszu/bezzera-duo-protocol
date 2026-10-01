<p align="right"><b>Deutsch</b> · <a href="versionen.en.md">English</a></p>

# Firmware-Versionen von Mainboard und Display

Stufen wie in [`stand.md`](stand.md): ✅ an dieser Maschine gemessen · 💡 aus
Händlerangaben oder Folgerung.

## Diese Maschine (Duo DE, 2018)

| | | Beleg |
|---|---|---|
| ✅ | Mainboard meldet **FW 2.1** (VP `0x0063` = 21 beim Start) | `boot.sr`, Startbild „FW: 2.1“ |
| ✅ | Display-Projekt **„TFT 2.0“** (Startbild) | Foto Seite 90 |
| ✅ | DWIN-Betriebssoftware im Display: Register `0x00` = `0x22` | gelesen; das ist DWINs Version, nicht Bezzeras |
| ✅ | Aufkleber auf dem Mainboard: `7661047PR`, Produktion `1809` | Foto |
| ✅ | Beide arbeiten zusammen | Maschine läuft |
| 💡 | Laut Tabelle unten gehört `7661047PR` zu FW 1.2, FW 2.1 aber zu `7661047.02PR`. Entweder trägt der Aufkleber nur die Grundnummer, oder das Mainboard wurde nachträglich aktualisiert | Widerspruch Aufkleber ↔ gemeldete Version |

## Versionen laut Händlern (💡)

Aus der Übersicht von [1st-line](https://www.1st-line.com/technical-support/bezzera-technical-support/bezzera-matrix-duo-software-compatibility-changes/)
(Seite nur mit Browser-Prüfung erreichbar, Inhalt über Suchergebnisse gelesen)
und Ersatzteilhändlern:

| Mainboard | Teilenummer | Display | Teilenummer | Änderungen |
|---|---|---|---|---|
| 1.2 | 7661047PR | 1.1 | 5963169.01 | erste Ausgabe |
| 2.0 | 7661047.01PR | 2.0 | 5963169.02 | Auto ON/OFF repariert, Füll-Timeout Kessel 15 → 30 s, Maschine startet nach 5 s von selbst (ohne Druck auf den Standby-Bildschirm) |
| **2.1** | 7661047.02PR | **2.0** | 5963169.03 | Heizung der Gruppe korrigiert — **diese Maschine** |
| 2.2 | 7661047.03PR | 2.2 | 5963201.01 | Dampfkessel wird während des Bezugs nicht gefüllt und nicht geheizt, Fehler im Standby behoben |
| 2.3 | 7661047.04PR | 2.2 | 5963201.01 | Fehler im Prüfablauf behoben |
| 2.4 | 7661047.05PR | 2.2 | 5963201.01 | Vorbrühen der deutschen Fassung: beim Bezug 1 s, beim Einlernen 2 s — vereinheitlicht |

Weitere Angaben:
- [Avola Coffeesystems](https://www.avola-coffeesystems.de/bezzera-elektronikbox-duo-matrix-sw-v-2-4-elektronik/8281091): „Elektronik-Firmware ab Version 2.0 ist mit Displays ab Version 2.0 kompatibel“, Elektronik mit Version 1.x oder 3.x nicht.
- Display-Baugruppen: [5963202R für Software 1.1](https://www.espresso.co.nz/parts-care/display-assembly-with-integrated-touchscreen-v1-1-duo-matrix-bezzera-5963202r/), [5963201.01R für Software 2.2](https://www.espresso.co.nz/parts/display-assembly-with-integrated-touchscreen-v2-2-duo-matrix-bezzera-5963201-01r/); beide mit getrennt montiertem Glas (Nachfolger von 5963187R).
- Eine Mainboard-Version 3.x wird erwähnt, aber nirgends beschrieben.

## Woher die Inkompatibilitäten kommen (💡, aus dem Protokoll gefolgert)

Das Display hat keine eigene Logik. Mainboard-Firmware und Display-Projekt
teilen sich aber einen **stillen Vertrag**, der nirgends geprüft wird:

1. **Seitennummern.** Das Mainboard schaltet Seiten selbst (90, 101, 103, Alarme). Hat das Display-Projekt unter dieser Nummer eine andere oder keine Seite, zeigt es Falsches.
2. **Tastencodes.** Jede Taste schreibt einen festen Wert in VP `0x0000` (z. B. 7 = Kaffee-Einstellungen, 20 = Menü). Neue Funktionen im Mainboard brauchen neue Codes und neue Seiten im Display, alte Displays schicken sie nicht.
3. **Variablenadressen.** Temperaturen, Sollwerte und Einstellungen liegen auf festen VPs (`0x0050`, `0x005A`–`0x005F`, `0x0070`–`0x007E` …). Verschiebt eine Version eine VP, liest oder zeigt die andere Seite den falschen Wert.
4. **Abläufe.** Mainboard 2.0 startet nach 5 s von selbst statt auf den Standby-Druck zu warten; das Display-Projekt 2.0 passt seine Seitenfolge dazu. Ein Display 1.1 an einem Mainboard 2.x erwartet einen anderen Ablauf.
5. **Kopf `C6 A5` statt `5A A5`.** Ein DWIN-Display „ab Werk“ spricht mit diesem Mainboard gar nicht; der Rahmenkopf steckt in der Konfiguration des Display-Projekts. Ein Ersatz-Display 2.2 (Version `0x25` in Register `0x00`) spricht dagegen `5A A5` und bleibt am Mainboard 2.1 stumm: Seiten erscheinen, Touch piept, aber keine Werte, weil das Mainboard beim Verbindungstest (Punkt 6) nie eine Antwort bekommt. Die Bridge erkennt den Kopf des Displays selbst und übersetzt in beide Richtungen (`?` zeigt `display_kopf=5A`).

   | | Display 2.0 | Ersatz-Display 2.2 |
   |---|---|---|
   | Rahmenkopf (R3, RA) | `C6 A5` | `5A A5` |
   | Register `0x10`–`0x1C` | – | `07 07 04 5A 00 FF 40 20 0A FF A5 00 00` |
   | Baud (R1) | 115200 | 115200 (`07`) |

   R3 lässt sich im Betrieb nicht umschreiben (Schreiben auf `0x13` und CONFIG_EN `0x1D` wirken nicht, gemessen). Ohne Bridge braucht ein 2.2-Display deshalb eine geänderte Konfiguration per SD-Karte, erprobt am 01.10.2026:

   1. Vorher sichern: `python3 tools/display_sichern.py alles` (braucht die Bridge, ~30 min).
   2. SD-Karte FAT32 mit 4-KB-Clustern, höchstens 8 GB oder eine 4-GB-Partition (`diskutil partitionDisk diskN MBR FAT32 DWIN 4G "Free Space" LEER R`).
   3. Ordner [`tools/display_kopf_c6/DWIN_SET`](../tools/display_kopf_c6/DWIN_SET/CONFIG.TXT) ins Wurzelverzeichnis kopieren. Er enthält nur `CONFIG.TXT`: die bisherigen Werte, R3 = `C6`. Seiten und Bilder bleiben unberührt.
   4. Karte bei eingeschalteter Maschine ins Display, nach wenigen Sekunden ziehen, Maschine aus und ein.

   Danach meldet Register `0x13` den Wert `C6`, auch nach dem Neustart, und Display und Mainboard sprechen ohne Bridge miteinander.
6. **Verbindungstest VP `0x0063`.** Das Mainboard schreibt seine Version dorthin und liest sie zurück. Nach allem, was mitgeschnitten ist, prüft es damit nur, ob überhaupt ein Display antwortet, nicht dessen Version. Eine unpassende Kombination startet deshalb vermutlich und verhält sich erst dann falsch.

Die Displays 2.0 und 2.2 unterscheiden sich vermutlich in Seiten und Tasten
für die neuen Funktionen (etwa Dampfkessel während des Bezugs). Wer die
Touch-Konfiguration eines 2.2-Displays mit `tools/libop_lesen.py` ausliest,
kann sie mit [`tasten.json`](tasten.json) (Projekt 2.0) vergleichen und sieht
die Unterschiede genau.

**Für die Bridge heißt das:** Sie ist gegen Mainboard 2.1 und Display 2.0
gemessen. Seitennummern, Tastencodes und VPs können bei anderen Versionen
abweichen.
