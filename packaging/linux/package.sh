#!/usr/bin/env bash
# Packs the Linux VST3 into a tarball.
#
#   packaging/linux/package.sh <version> <output-dir>
set -euo pipefail

VERSION="$1"
OUT="$2"
VST3="build/DAWProjectManager_artefacts/Release/VST3/DAW Project Manager.vst3"

mkdir -p "$OUT"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

DIR="$WORK/DAW Project Manager Plugin $VERSION"
mkdir -p "$DIR"
cp -a "$VST3" "$DIR/"
cp packaging/linux/INSTALL.txt "$DIR/INSTALL.txt"
cp packaging/linux/install.sh "$DIR/install.sh"
chmod +x "$DIR/install.sh"

tar -C "$WORK" -czf "$OUT/DAW_Project_Manager_Plugin_Linux_x86_64_v$VERSION.tar.gz" "DAW Project Manager Plugin $VERSION"
ls -la "$OUT"
