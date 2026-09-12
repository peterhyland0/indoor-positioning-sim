#!/usr/bin/env bash
# Cook + stage a Development Mac build and copy the complete .app to build/Mac/.
# (UAT's -archive only copies the app shell on Mac; the staged app is the full one.)
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$(ls -d "/Users/Shared/Epic Games"/UE_* | sort -V | tail -1)}"
PROJ="$REPO/unreal/VerticalGeofenceSim/VerticalGeofenceSim.uproject"
CONFIG="${1:-Development}"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJ" -platform=Mac -clientconfig="$CONFIG" \
  -cook -build -stage -pak -nop4 -utf8output -unattended
rm -rf "$REPO/build/Mac"; mkdir -p "$REPO/build/Mac"
cp -R "$REPO/unreal/VerticalGeofenceSim/Saved/StagedBuilds/Mac/VerticalGeofenceSim.app" "$REPO/build/Mac/"
echo "packaged: $REPO/build/Mac/VerticalGeofenceSim.app"
echo "run headless: build/Mac/VerticalGeofenceSim.app/Contents/MacOS/VerticalGeofenceSim -nullrhi -unattended -SimRunSeconds=900 -SimSpeed=20 -SimSessionsDir=\$PWD/sessions/out"
