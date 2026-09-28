# Bauteile im Detail

Fotos und Online-Recherche, 2026-09-28. Übersicht im [README](../README.md#-hardware).

Viele Herstellerseiten waren bei der Recherche nicht abrufbar. **Belegt**
heißt hier: von Händler- oder Herstellerseiten in Suchergebnissen bestätigt.
**Vermutet** heißt: eigene Einschätzung, am Teil zu prüfen.

**Weißer Zwischenstecker im Displaykabel.** Einreihig, 4-polig, Naturfarbe,
Rastnase auf einer Seite. Die Kammern sind kodiert: zwei eckig, zwei mit
abgeschrägter Oberkante. Diese Kodierung haben nur Molex Mini-Fit Jr.
(4,2 mm) und Micro-Fit 3.0 (3,0 mm) samt Nachbauten. JST VH kodiert anders.
Vermutet: **Mini-Fit Jr.**, weil Micro-Fit fast immer schwarz ist. Prüfen mit
dem Messschieber über drei Teilungen: 12,6 mm = Mini-Fit Jr., 9,0 mm =
Micro-Fit 3.0.

| | Mini-Fit Jr. (4,2 mm) | Micro-Fit 3.0 (3,0 mm) |
|---|---|---|
| Buchsengehäuse 1×4 | 39-01-4040 | 43645-0400 |
| Steckergehäuse 1×4 | 39-01-4046 | 43640-0400 |
| Buchsenkontakt | 5556, z. B. 39-00-0038 | 43030-xxxx |
| Stiftkontakt | 5558, z. B. 39-00-0041 | 43031-xxxx |

Das komplette Kabel gibt es als Ersatzteil: **Bezzera 7663518**
„Matrix/Duo Display connecting cable“ (u. a. bei 1st-line, rund 22 USD).

**Touch-Controller SiS9252.** Belegt: Controller für projiziert-kapazitive
Panels bis 10,1", 32-Bit-RISC-Kern, I²C, eingebauter UART
([sis.com](https://www.sis.com/Product_9252_PH.aspx)). Linux kennt die
Familie als `sis_i2c` („SiS 9200 family“, Devicetree `sis,9200-ts`, im
Beispiel Adresse `0x5c`). Ein öffentliches Datenblatt gibt es nicht.
Vermutet: Der 6-polige Anschluss trägt GND, VDD, SCL, SDA, INT und RST,
`VDD` an Pin 6 ist aufgedruckt, der Rest muss gemessen werden (SCL/SDA mit
Pull-ups, INT geht bei Berührung auf Low).

**Touch-Flachkabel.** Außer `CDQ9439-3.5-A` steht dort `HLT/18/32`, auf dem
Chip zusätzlich `A3`. Vermutet: Herstellerkürzel und Datumscode
(Kalenderwoche 32/2018, passt zum Modul-Datumscode `180814`), `A3` die
Chip-Revision. Weder `CDQ9439` noch ein Touch-Hersteller „HLT“ sind online zu
finden. Das Panel ist also kein Handelsteil.

**Ersatz für den Touch.** Die Z4-Variante verkauft DWIN nicht frei, das
Touch-Panel ist nirgends als Ersatzteil gelistet. DWIN liefert eigene
kapazitive Panels mit GT911 und hat einen eigenen Touch-Controller (TPS04).
Vermutet: Die Mini-DGUS-Firmware des Moduls kennt nur den verbauten
SiS-Controller. Ein Panel mit FocalTech- oder Goodix-Chip läuft dann nicht
ohne passende Firmware. Vor einem Kauf bei DWIN mit der vollen Typnummer
anfragen. Nahe Verwandte im Handel: DMT32240M035_17WT, DMG32240C035_03WTC.

**Wackelkontakt am Touch-Stecker reparieren.** Vermutet: 6-poliger
ZIF-Stecker, meist 0,5 mm Raster (6 Kontakte ≈ 2,5 mm breit), seltener
1,0 mm. Riegel öffnen, Kontakte am Flachkabel mit Isopropanol reinigen,
gerade bis zum Anschlag einstecken. Hält es nicht, das Flachkabelende mit
Kapton-Band hinterkleben, damit es dicker wird, oder den Stecker tauschen
(Heißluft, Flussmittel). Ein Riss im Flachkabel lässt sich mit 0,1-mm-Lackdraht
brücken.

**LCD-Panel LQ035NC111.** Belegt: Innolux (früher Chimei Innolux/CMO),
3,5" (genau 3,45"), 320×240, a-Si-TFT, TN, transmissiv. Aktive Fläche
70,08 × 52,56 mm, ca. 300 cd/m², Kontrast 400:1. Treiber-IC HX8238, 3,3 V.
Schnittstelle Digital-RGB 24 Bit oder 8 Bit seriell, dazu SPI, über 54-poliges
Flachkabel. Backlight 6 weiße LEDs in Reihe mit externem Treiber (vermutet
etwa 18–20 V bei 20 mA). Die Varianten 111, 121 und 211 werden als
austauschbar gehandelt, Ersatz kostet etwa 10–30 €.
[Datenblatt](https://www.data-modul.com/sites/default/files/products/LQ035NC111_specification_12007119.pdf).
DWIN selbst führt das Datenblatt in seinem Katalog, das Panel ist also sehr
wahrscheinlich Standard in DWINs 3,5"-Modulen. Die Schutzfolie mit
„…hnology“, einem chinesischen Schriftzeichen und dem Stempel „PX 9“
stammt vermutlich von einem Zwischenhändler oder Konfektionär. Eine
Zuordnung war nicht möglich. Auf dem Aufkleber beginnt der Barcode mit
`PK035…`.

**Typnummer DMT32240M035_07WTZ4** nach DWINs
[Namensschema](https://www.dwin-global.com/naming-convention/):

| Teil | Bedeutung | |
|---|---|---|
| DM | DWIN Smart-LCM | belegt |
| T | 65K Farben (16 Bit) | belegt |
| 32240 | 320×240 | belegt |
| M | M-Serie, Mini DGUS | vermutet |
| 035 | 3,5" | belegt |
| _07 | Hardwarevariante | belegt |
| W | erweiterter Temperaturbereich | belegt |
| T | mit Touch (älteres Schema ohne C/R, hier kapazitiv, siehe SiS9252) | belegt |
| Z4 | Kundenversion Nr. 4, vermutlich für Bezzera | vermutet |

Die Geschwister 03WT und 03W laufen mit 3,3–6 V (empfohlen 5 V, 0,5 A),
UART auf TTL-Pegel mit 1200–691200 Baud und 64 Helligkeitsstufen.
