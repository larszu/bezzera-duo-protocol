#!/usr/bin/env bash
# Firmware fuer beide Boards bauen (lokal und in .github/workflows/firmware.yml):
#   s3    Waveshare ESP32-S3-ETH: 16 MB Flash, PSRAM, Ethernet, Matter, Update per Web
#   esp32 klassischer ESP32 (ESP32-D0WD, 4 MB): ohne Matter, Ethernet und Update per Web
# Ergebnis: build/<board>/duo_bridge.ino.merged.bin (komplett ab 0x0) und .ino.bin (nur Programm)
#   bash tools/firmware_bauen.sh            # beide
#   bash tools/firmware_bauen.sh esp32      # nur eins
set -euo pipefail
cd "$(dirname "$0")/.."
BOARDS=("${@:-s3 esp32}")
[[ $# -eq 0 ]] && BOARDS=(s3 esp32)
# Versionsnummer in die Firmware (Weboberflaeche, Update-Hinweis); Standard: Git-Stand
FW_VERSION="${FW_VERSION:-main-$(git rev-parse --short HEAD 2>/dev/null || echo dev)}"
PROP="compiler.cpp.extra_flags=-DFW_VERSION=\"${FW_VERSION}\""
for b in "${BOARDS[@]}"; do
  case "$b" in
    s3)
      arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
        --build-property "$PROP" --output-dir build/s3 bridge/duo_bridge ;;
    esp32)
      # partitions.csv im Sketch gilt fuer 16 MB und wuerde immer genommen: Kopie ohne sie bauen
      t=$(mktemp -d)/duo_bridge; mkdir -p "$t"
      cp bridge/duo_bridge/*.ino bridge/duo_bridge/*.h "$t"/
      arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --build-property "$PROP" --output-dir build/esp32 "$t" ;;
    *) echo "unbekanntes Board: $b (s3|esp32)" >&2; exit 1 ;;
  esac
done
