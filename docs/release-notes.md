Firmware für die Bezzera-Duo-Bridge, für zwei Boards.

**Installieren ohne Vorkenntnisse:** https://larszu.github.io/bezzera-duo-protocol/ in Chrome oder Edge öffnen,
ESP32 per USB anstecken, „Installieren“. Die Seite erkennt das Board selbst.

| Datei | Wofür |
|---|---|
| `duo_bridge-esp32s3-<version>.bin` | Waveshare ESP32-S3-ETH, erstes Flashen per USB (ab 0x0) |
| `duo_bridge-esp32s3-update-<version>.bin` | ESP32-S3: Update über die Weboberfläche (Diagnose → Firmware) |
| `duo_bridge-esp32-<version>.bin` | klassischer ESP32 (4 MB), Flashen per USB (ab 0x0) |
| `bauanleitung-de-<version>.pdf`, `build-guide-en-<version>.pdf` | Bauanleitung: Flashen, Verkabelung, Pinbelegung, Ersatz-Display, Fernzugriff |
| `display_kopf_c6.zip` | Ersatz-Display 2.2 zeigt keine Werte: Ordner auf SD-Karte, siehe README |

Pinbelegung beider Boards: [docs/pinbelegung.md](https://github.com/larszu/bezzera-duo-protocol/blob/main/docs/pinbelegung.md).
Der klassische ESP32 kann alles außer Matter, Ethernet und Update per Weboberfläche.

Neu: klassischer ESP32 als zweites Board, Installer für beide, Brühkurve im neuen Layout
(Raster für Druck und Temperatur, Infokästen), Brühkurve bleibt beim Bezug offen,
Recherche zum Mainboard und zu verwandten Maschinen (docs/mainboard.md).
