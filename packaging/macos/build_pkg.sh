#!/usr/bin/env bash
# Builds the macOS installer and zip from a Release build.
#
#   packaging/macos/build_pkg.sh <version> <output-dir>
#
# Signs and notarizes when the Apple secrets are in the environment (the same
# names and the same Developer ID certificate the app's release job uses; see
# .github/REQUIRED_SECRETS.md). Without them the bundles are ad-hoc signed —
# enough for Apple Silicon to load them, though Gatekeeper will warn.
set -euo pipefail

VERSION="$1"
OUT="$2"
ARTEFACTS="build/DAWProjectManager_artefacts/Release"
VST3="$ARTEFACTS/VST3/DAW Project Manager.vst3"
AU="$ARTEFACTS/AU/DAW Project Manager.component"
IDENTIFIER="com.bandpassrecords.dpm.plugin"

mkdir -p "$OUT"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

sign_bundle() {
  local bundle="$1"
  if [ -n "${APPLE_DEVELOPER_ID:-}" ]; then
    codesign --force --deep --timestamp --options runtime --sign "$APPLE_DEVELOPER_ID" "$bundle"
  else
    codesign --force --deep --sign - "$bundle"
  fi
  codesign --verify --strict --verbose=2 "$bundle"
}

can_notarize() {
  [ -n "${APPLE_DEVELOPER_ID:-}" ] && [ -n "${APPLE_ID:-}" ] \
    && [ -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ] && [ -n "${APPLE_TEAM_ID:-}" ]
}

notarize() {
  xcrun notarytool submit "$1" --apple-id "$APPLE_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD" \
    --team-id "$APPLE_TEAM_ID" --wait --timeout 30m
}

# --- Bundles -------------------------------------------------------------------
STAGE="$WORK/root/Library/Audio/Plug-Ins"
mkdir -p "$STAGE/VST3" "$STAGE/Components"
ditto "$VST3" "$STAGE/VST3/DAW Project Manager.vst3"
ditto "$AU" "$STAGE/Components/DAW Project Manager.component"

sign_bundle "$STAGE/VST3/DAW Project Manager.vst3"
sign_bundle "$STAGE/Components/DAW Project Manager.component"

if can_notarize; then
  # Notarize the bundles themselves too, so the zip's copies are stapled.
  ditto -c -k --keepParent "$STAGE/VST3/DAW Project Manager.vst3" "$WORK/vst3.zip"
  ditto -c -k --keepParent "$STAGE/Components/DAW Project Manager.component" "$WORK/au.zip"
  notarize "$WORK/vst3.zip"
  notarize "$WORK/au.zip"
  xcrun stapler staple "$STAGE/VST3/DAW Project Manager.vst3"
  xcrun stapler staple "$STAGE/Components/DAW Project Manager.component"
fi

# --- Zip (drag-and-drop install) -------------------------------------------------
ZIPDIR="$WORK/zip/DAW Project Manager Plugin $VERSION"
mkdir -p "$ZIPDIR"
ditto "$STAGE/VST3/DAW Project Manager.vst3" "$ZIPDIR/DAW Project Manager.vst3"
ditto "$STAGE/Components/DAW Project Manager.component" "$ZIPDIR/DAW Project Manager.component"
cp packaging/macos/INSTALL.txt "$ZIPDIR/INSTALL.txt"
(cd "$WORK/zip" && ditto -c -k --keepParent "DAW Project Manager Plugin $VERSION" "DAW_Project_Manager_Plugin_macOS_v$VERSION.zip")
mv "$WORK/zip/DAW_Project_Manager_Plugin_macOS_v$VERSION.zip" "$OUT/"

# --- Installer package -----------------------------------------------------------
# Plug-in bundles must not be "relocated" to wherever the installer finds an
# older copy with the same bundle id (a dev build in ~/Library, say).
pkgbuild --analyze --root "$WORK/root" "$WORK/components.plist"
count=$(/usr/libexec/PlistBuddy -c "Print" "$WORK/components.plist" | grep -c "BundleIsRelocatable" || true)
for ((i = 0; i < count; i++)); do
  /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/components.plist"
done

pkgbuild --root "$WORK/root" --install-location / --component-plist "$WORK/components.plist" \
  --identifier "$IDENTIFIER" --version "$VERSION" --scripts packaging/macos/scripts \
  "$WORK/component.pkg"

PKG="$OUT/DAW_Project_Manager_Plugin_macOS_Installer_v$VERSION.pkg"

# A distribution that lets the user choose where it goes: "Install for all
# users of this computer" (/Library/Audio/Plug-Ins) or "Install for me only"
# (~/Library/Audio/Plug-Ins, no administrator password). macOS installers can
# choose between those domains only, not an arbitrary folder — and those two
# are where every host looks.
productbuild --synthesize --package "$WORK/component.pkg" "$WORK/distribution.xml"
perl -0pi -e 's|(<installer-gui-script[^>]*>)|$1\n    <title>DAW Project Manager Plugin</title>\n    <domains enable_localSystem="true" enable_currentUserHome="true" enable_anywhere="false"/>|' \
  "$WORK/distribution.xml"
grep -q 'enable_currentUserHome="true"' "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$WORK/unsigned.pkg"

if [ -n "${APPLE_INSTALLER_ID:-}" ]; then
  productsign --timestamp --sign "$APPLE_INSTALLER_ID" "$WORK/unsigned.pkg" "$PKG"
  if can_notarize; then
    notarize "$PKG"
    xcrun stapler staple "$PKG"
  fi
else
  mv "$WORK/unsigned.pkg" "$PKG"
fi

echo "Built:"
ls -la "$OUT"
