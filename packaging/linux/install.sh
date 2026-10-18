#!/usr/bin/env bash
# Installs DPM Companion.
#
#   ./install.sh               for the current user: ~/.vst3
#   sudo ./install.sh --system for everyone: /usr/local/lib/vst3
#   ./install.sh <folder>      anywhere else — then add that folder to your
#                              DAW's VST3 plug-in folders
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
case "${1:-}" in
  "")       DEST="$HOME/.vst3" ;;
  --system) DEST="/usr/local/lib/vst3" ;;
  -h|--help)
    sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'
    exit 0 ;;
  *)        DEST="$1" ;;
esac

mkdir -p "$DEST"
rm -rf "$DEST/DPM Companion.vst3"
cp -a "$HERE/DPM Companion.vst3" "$DEST/"
echo "Installed to $DEST/DPM Companion.vst3. Rescan plug-ins in your DAW."
