#!/usr/bin/env bash
# Installs DPM Companion for the current user (~/.vst3), or system-wide with
# --system (/usr/local/lib/vst3, needs sudo).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/.vst3"
if [ "${1:-}" = "--system" ]; then
  DEST="/usr/local/lib/vst3"
fi

mkdir -p "$DEST"
rm -rf "$DEST/DPM Companion.vst3"
cp -a "$HERE/DPM Companion.vst3" "$DEST/"
echo "Installed to $DEST/DPM Companion.vst3. Rescan plug-ins in your DAW."
