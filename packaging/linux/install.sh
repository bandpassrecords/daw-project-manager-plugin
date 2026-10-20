#!/usr/bin/env bash
# Installs the DAW Project Manager plug-in.
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
rm -rf "$DEST/DAW Project Manager.vst3"
cp -a "$HERE/DAW Project Manager.vst3" "$DEST/"
echo "Installed to $DEST/DAW Project Manager.vst3. Rescan plug-ins in your DAW."
