#!/bin/bash
# DWIN DGUS SDK 5.10 (Windows) auf dem Mac unter Wine einrichten.
# Fuer eigene Seiten reicht der Seitenbau im Browser (tools/seitenbau);
# das SDK braucht nur, wer Schriften oder Symbolbibliotheken bauen will.
#
#   bash tools/dgus_sdk_mac.sh          # einrichten
#   bash tools/dgus_sdk_mac.sh start    # SDK starten
#
# Wine kommt als fertiger Build von Gcenx (GitHub). Das Homebrew-Paket
# wine-stable ist seit 09/2026 gesperrt, weil es Gatekeeper nicht besteht;
# deshalb wird hier fuer genau diese App das Quarantaene-Attribut entfernt.
# SDK-Quelle: Spiegel von S. Andrivet (DWIN bietet 5.10 nicht mehr an).
set -euo pipefail
WINE_VERSION=11.18
APP="$HOME/Applications/Wine Devel.app"
WINE="$APP/Contents/Resources/wine/bin/wine"
export WINEPREFIX="$HOME/.wine-dgus" WINEDEBUG=-all
SDK_URL=https://sebastien.andrivet.com/static/DGUS_5.10_Setup.zip
SDK_SHA256=4b173df3c512f1714a4f6cf999592f831a5452b34ee52245827063a4ffddc9ba

if [[ "${1:-}" == "start" ]]; then
  EXE=$(find "$WINEPREFIX/drive_c" -iname "DGUS*.exe" ! -iname "*setup*" | head -1)
  [[ -n "$EXE" ]] || { echo "SDK nicht gefunden, erst ohne 'start' einrichten"; exit 1; }
  exec "$WINE" "$EXE"
fi

[[ "$(uname -m)" == arm64 ]] && ! /usr/bin/pgrep -q oahd && softwareupdate --install-rosetta --agree-to-license
if [[ ! -x "$WINE" ]]; then
  tmp=$(mktemp -d)
  curl -fL -o "$tmp/wine.tar.xz" "https://github.com/Gcenx/macOS_Wine_builds/releases/download/$WINE_VERSION/wine-devel-$WINE_VERSION-osx64.tar.xz"
  tar -xf "$tmp/wine.tar.xz" -C "$tmp"
  mkdir -p "$HOME/Applications"
  mv "$tmp/Wine Devel.app" "$APP"
  xattr -dr com.apple.quarantine "$APP"
fi
tmp=$(mktemp -d)
curl -fL -o "$tmp/sdk.zip" "$SDK_URL"
echo "$SDK_SHA256  $tmp/sdk.zip" | shasum -a 256 -c -
unzip -oq "$tmp/sdk.zip" -d "$tmp"
# Inno-Setup-Installer: still, ohne Fenster, nach C:\DGUS
"$WINE" "$tmp/DGUS_5.10_Setup.exe" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR="C:\\DGUS"
echo "Fertig. Starten mit: bash tools/dgus_sdk_mac.sh start"
