#!/usr/bin/env bash
# Build the prototype for an Xbox in Developer Mode and pack it as a signed .appx.
# Usage: bash tools/build-xbox.sh
# Needs LLVM_MINGW_ROOT (an unpacked llvm-mingw release; lba2cc's
# scripts/dev/build-uwp.sh downloads the pinned one), osslsigncode and openssl.
# Output: out/xbox/lba2menu-xbox-x64.appx
#
# The package is its own app, "LBA2 menu prototype" (LBA2CC.MenuProto), beside
# lba2cc's: the game, its settings and saves are not touched. It reads the menu's
# game data (ress.hqr, screen.hqr) from its own LocalState\Game.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LBA2CC="$ROOT/lba2cc"
: "${LLVM_MINGW_ROOT:?set LLVM_MINGW_ROOT to an unpacked llvm-mingw release}"
export LLVM_MINGW_ROOT
command -v osslsigncode >/dev/null || { echo "osslsigncode not found" >&2; exit 1; }

BUILD="$ROOT/build-uwp"
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$LBA2CC/cmake/llvm-mingw-x86_64-uwp.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DPLATFORM_BACKEND=uwp >/dev/null
cmake --build "$BUILD" --target lba2menu
python3 -I "$LBA2CC/scripts/ci/check-uwp-build.py" imports --toolchain "$LLVM_MINGW_ROOT" "$BUILD/lba2menu.exe"

# Windows updates a package in place only to a higher version: days since
# 2026-11-01, then the minute of the day.
now=$(date -u +%s)
days=$(( (now - $(date -u -j -f %Y-%m-%d 2026-11-01 +%s 2>/dev/null || date -u -d 2026-11-01 +%s)) / 86400 ))
minute=$(( (now % 86400) / 60 ))
VERSION="0.1.$days.$minute"

OUT="$ROOT/out/xbox"
STAGE="$OUT/stage"
rm -rf "$OUT" && mkdir -p "$STAGE"
cp "$BUILD/lba2menu.exe" "$STAGE/"
cp -R "$LBA2CC/tools/uwp/Assets" "$STAGE/"
cat >"$STAGE/AppxManifest.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         IgnorableNamespaces="uap">
  <Identity Name="LBA2CC.MenuProto" Publisher="CN=lba2cc-dev" Version="$VERSION" ProcessorArchitecture="x64" />
  <Properties>
    <DisplayName>LBA2 menu prototype</DisplayName>
    <PublisherDisplayName>LBALab</PublisherDisplayName>
    <Logo>Assets\\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Universal" MinVersion="10.0.17763.0" MaxVersionTested="10.0.22621.0" />
  </Dependencies>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Applications>
    <Application Id="App" Executable="lba2menu.exe" EntryPoint="Lba2cc.App">
      <uap:VisualElements DisplayName="LBA2 menu prototype" Description="The lba2cc Options menu prototype"
                          BackgroundColor="#000000"
                          Square150x150Logo="Assets\\Square150x150Logo.png"
                          Square44x44Logo="Assets\\Square44x44Logo.png">
        <uap:SplashScreen Image="Assets\\SplashScreen.png" />
      </uap:VisualElements>
    </Application>
  </Applications>
</Package>
EOF

KEYS="$OUT/keys"
mkdir -p "$KEYS" && chmod 700 "$KEYS"
trap 'rm -rf "$KEYS"' EXIT
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$KEYS/key.pem" -out "$KEYS/cert.pem" \
    -days 30 -subj "/CN=lba2cc-dev" -addext extendedKeyUsage=codeSigning \
    -addext keyUsage=digitalSignature -addext basicConstraints=critical,CA:FALSE 2>/dev/null
APPX="$OUT/lba2menu-xbox-x64.appx"
python3 -I "$LBA2CC/tools/uwp/pack_appx.py" "$STAGE" "$OUT/unsigned.appx"
osslsigncode sign -certs "$KEYS/cert.pem" -key "$KEYS/key.pem" -in "$OUT/unsigned.appx" -out "$APPX" >/dev/null
osslsigncode verify -in "$APPX" -CAfile "$KEYS/cert.pem" >/dev/null
rm -f "$OUT/unsigned.appx"
echo "version $VERSION: $APPX"
