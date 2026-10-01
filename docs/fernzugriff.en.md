<p align="right"><a href="fernzugriff.md">Deutsch</a> · <b>English</b></p>

# Using the machine from outside your home

The bridge is a small web server on your home network. Only a secure tunnel
should reach it from outside; **never** forward a router port – the bridge can
switch the machine on.

## First: set a password

**Einstellungen → Zugang → Passwort für diese Seite** (settings → access →
password for this page). Without it anyone on the network can operate the machine.

## Option 1: Tailscale via a device that is always on (recommended)

There is no mature Tailscale for the ESP32 itself (not enough memory for
WireGuard plus the Tailscale control plane). A computer on the home network
acting as a **subnet router** is enough, e.g. a home server, Raspberry Pi or NAS:

Easiest with the script from this repo, it detects the home network itself:

```bash
sudo tools/tailscale_heimnetz.sh
```

Or by hand:

```bash
# on the device in the home network (Linux)
curl -fsSL https://tailscale.com/install.sh | sh
echo 'net.ipv4.ip_forward = 1' | sudo tee /etc/sysctl.d/99-tailscale.conf && sudo sysctl -p /etc/sysctl.d/99-tailscale.conf
sudo tailscale up --advertise-routes=192.168.0.0/24    # your home network
```

1. In the Tailscale admin (login.tailscale.com → Machines) approve the route under “Edit route settings”.
2. Install the Tailscale app on the phone and sign in.
3. Open the bridge's **IP address** in the browser (shown under Einstellungen → Heim-WLAN).
   `duo.local` does not work over Tailscale because mDNS only works on the local network.

Tip: give the bridge a fixed IP in the router so the bookmark keeps working.

## Option 1b: the machine as its own device in Tailscale (Docker)

Instead of exposing the whole home network, the bridge appears as its own
device "bezzera" in the Tailscale app, with a fixed address for a home-screen
icon. On a computer in the home network with Docker:

```bash
sudo docker run -d --name bezzera-tailscale --restart unless-stopped \
  -e TS_HOSTNAME=bezzera -e TS_STATE_DIR=/var/lib/tailscale -e TS_USERSPACE=true \
  -v /opt/bezzera-tailscale/state:/var/lib/tailscale tailscale/tailscale:latest
sudo docker logs bezzera-tailscale          # open the login link and confirm
sudo docker exec bezzera-tailscale tailscale serve --bg --tcp 80 tcp://<bridge IP>:80
sudo docker exec bezzera-tailscale tailscale ip -4   # address for the phone
```

`--tcp` instead of `--http` so the page answers under the name and the 100.x
address. On the iPhone: connect Tailscale, open the address in Safari, Share →
Add to Home Screen. Give the bridge a fixed IP in the router.

## Option 2: Home Assistant

If Home Assistant has remote access (Nabu Casa or your own tunnel), the bridge
is there as device “Bezzera Duo” with the switch “Maschine”. Switching on and
off then works from anywhere in the Home Assistant app, without a second tunnel.

## Option 3: Calendar

For fixed coffee times: **Maschine → Ein- und Ausschalten → Kalender**. Events
with the keyword in the title switch the machine on beforehand. The calendar is
read over the internet, so this works while you are away too.
