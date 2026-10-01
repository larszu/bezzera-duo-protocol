Firmware für die Bezzera-Duo-Bridge (Waveshare ESP32-S3-ETH).

| Datei | Wofür |
|---|---|
| `duo_bridge-<version>.bin` | Erstes Flashen per USB ([Anleitung](https://github.com/larszu/bezzera-duo-protocol/blob/main/docs/flashen.md), Flash-Seite im Browser) |
| `duo_bridge-update-<version>.bin` | Update über die Weboberfläche: Diagnose → Firmware |
| `display_kopf_c6.zip` | Ersatz-Display 2.2 zeigt keine Werte: Ordner auf SD-Karte, siehe README |

Neu in dieser Version: Ersatz-Display 2.2 (Kopf-Übersetzung, SD-Umstellung),
Brühkurve auf dem Display und in der App, Startseiten-Taste, Touch-Piepen aus,
Display-Sicherung, SD-Paketbauer (`tools/sd_paket.py`), Firmware-Update per
Web, Fernzugriff als eigenes Tailscale-Gerät, ntfy, einstellbarer Web-Benutzer.
