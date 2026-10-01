#!/usr/bin/env bash
# Tailscale-Subnet-Router fuer den Fernzugriff auf die Duo-Bridge.
# Auf einem Linux-Rechner im Heimnetz ausfuehren, der dauerhaft laeuft
# (Heimserver, Raspberry Pi, NAS mit Linux). Macht das Heimnetz ueber
# Tailscale erreichbar, die Bridge selbst braucht dafuer nichts.
#
#   sudo ./tailscale_heimnetz.sh              # Heimnetz automatisch erkennen
#   sudo ./tailscale_heimnetz.sh 192.168.0.0/24
#
# Danach: Route in der Tailscale-Verwaltung freigeben, Tailscale-App aufs
# Handy, Bridge ueber ihre IP-Adresse oeffnen (docs/fernzugriff.md).
set -euo pipefail

if [[ $EUID -ne 0 ]]; then echo "Bitte mit sudo ausfuehren."; exit 1; fi

netz="${1:-}"
if [[ -z "$netz" ]]; then
  # Netz der Standardroute, z. B. 192.168.0.0/24
  dev=$(ip route show default | awk '{print $5; exit}')
  netz=$(ip -o -f inet addr show "$dev" | awk '{print $4; exit}')
  netz=$(python3 -c "import ipaddress,sys; print(ipaddress.ip_interface('$netz').network)")
fi
echo "Heimnetz: $netz"

if ! command -v tailscale >/dev/null; then
  echo "Installiere Tailscale ..."
  curl -fsSL https://tailscale.com/install.sh | sh
fi

# Weiterleitung einschalten (dauerhaft)
cat > /etc/sysctl.d/99-tailscale.conf <<CONF
net.ipv4.ip_forward = 1
net.ipv6.conf.all.forwarding = 1
CONF
sysctl -p /etc/sysctl.d/99-tailscale.conf >/dev/null

tailscale up --advertise-routes="$netz" --accept-dns=false

cat <<HINWEIS

Fertig. Jetzt noch:
  1. https://login.tailscale.com/admin/machines -> dieses Geraet -> "Edit route settings"
     -> $netz freigeben.
  2. Tailscale-App aufs Handy, mit demselben Konto anmelden.
  3. Im Browser die IP der Bridge oeffnen (Einstellungen -> Heim-WLAN in der
     Weboberflaeche). duo.local geht ueber Tailscale nicht.
  4. In der Bridge ein Passwort fuer die Seite setzen, falls noch nicht geschehen.
HINWEIS
