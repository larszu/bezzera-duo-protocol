<p align="right"><b>Deutsch</b> · <a href="fernzugriff.en.md">English</a></p>

# Maschine von unterwegs bedienen

Die Bridge ist ein kleiner Webserver im Heimnetz. Von außen kommt man nur über
einen sicheren Tunnel hinein; **nie** einen Port im Router freigeben – die
Bridge kann die Maschine einschalten.

## Vorher: Passwort setzen

**Einstellungen → Zugang → Passwort für diese Seite.** Ohne Passwort darf jeder
im Netz die Maschine bedienen.

## Weg 1: Tailscale über ein Gerät, das ohnehin läuft (empfohlen)

Tailscale direkt auf dem ESP32 gibt es nicht ausgereift (zu wenig Speicher für
WireGuard plus Tailscale-Steuerung). Stattdessen reicht ein Rechner im
Heimnetz als **Subnet-Router**, z. B. der Heimserver, ein Raspberry Pi oder ein
NAS:

Am einfachsten mit dem Skript aus diesem Repo, es erkennt das Heimnetz selbst:

```bash
sudo tools/tailscale_heimnetz.sh
```

Oder von Hand:

```bash
# auf dem Gerät im Heimnetz (Linux)
curl -fsSL https://tailscale.com/install.sh | sh
echo 'net.ipv4.ip_forward = 1' | sudo tee /etc/sysctl.d/99-tailscale.conf && sudo sysctl -p /etc/sysctl.d/99-tailscale.conf
sudo tailscale up --advertise-routes=192.168.0.0/24    # eigenes Heimnetz eintragen
```

1. In der Tailscale-Verwaltung (login.tailscale.com → Machines) beim Gerät
   „Edit route settings“ die Route freigeben.
2. Auf dem Handy die Tailscale-App installieren, anmelden.
3. Im Browser die **IP-Adresse** der Bridge öffnen (steht unter Einstellungen →
   Heim-WLAN). `duo.local` geht über Tailscale nicht, weil mDNS nur im eigenen
   Netz funktioniert.

Tipp: Im Router der Bridge eine feste IP geben, dann bleibt das Lesezeichen gültig.

## Weg 1b: Maschine als eigenes Gerät im Tailscale-Netz (Docker)

Statt das ganze Heimnetz freizugeben, steht die Bridge als eigenes Gerät
„bezzera“ in der Tailscale-App, mit fester Adresse für ein Symbol auf dem
Home-Bildschirm. Auf einem Rechner im Heimnetz mit Docker:

```bash
sudo docker run -d --name bezzera-tailscale --restart unless-stopped \
  -e TS_HOSTNAME=bezzera -e TS_STATE_DIR=/var/lib/tailscale -e TS_USERSPACE=true \
  -v /opt/bezzera-tailscale/state:/var/lib/tailscale tailscale/tailscale:latest
sudo docker logs bezzera-tailscale          # Anmeldelink öffnen und bestätigen
sudo docker exec bezzera-tailscale tailscale serve --bg --tcp 80 tcp://<IP der Bridge>:80
sudo docker exec bezzera-tailscale tailscale ip -4   # Adresse für das Handy
```

`--tcp` statt `--http`, damit die Seite unter dem Namen und unter der
100.x-Adresse antwortet. Auf dem iPhone: Tailscale verbinden, Adresse in
Safari öffnen, Teilen → Zum Home-Bildschirm. Der Bridge im Router eine feste
IP geben.

## Weg 2: Home Assistant

Läuft Home Assistant mit Fernzugriff (Nabu Casa oder eigener Tunnel), ist die
Bridge dort als Gerät „Bezzera Duo“ mit Schalter „Maschine“ eingebunden
(Einstellungen → Home Assistant). Ein- und Ausschalten geht dann über die
Home-Assistant-App von überall, ganz ohne zweiten Tunnel.

## Weg 3: Kalender

Wer nur zu festen Zeiten Kaffee will: **Maschine → Ein- und Ausschalten →
Kalender**. Termine mit dem Stichwort im Titel schalten die Maschine vorher ein.
Der Kalender wird über das Internet gelesen, das geht auch, wenn man selbst
unterwegs ist.
