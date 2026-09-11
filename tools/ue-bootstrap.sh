#!/usr/bin/env bash
# Runs Scripts/bootstrap.py headlessly against the installed engine.
# Usage: tools/ue-bootstrap.sh [UE_ROOT]     (default: newest /Users/Shared/Epic Games/UE_*)
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$REPO/unreal/VerticalGeofenceSim/VerticalGeofenceSim.uproject"
SCRIPT="$REPO/unreal/VerticalGeofenceSim/Scripts/bootstrap.py"
UE_ROOT="${1:-$(ls -d "/Users/Shared/Epic Games"/UE_* 2>/dev/null | sort -V | tail -1)}"
EDITOR_CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
[ -x "$EDITOR_CMD" ] || { echo "UnrealEditor-Cmd not found under $UE_ROOT (install still running?)"; exit 1; }
echo "engine: $UE_ROOT"
echo "project: $PROJ"
"$EDITOR_CMD" "$PROJ" -run=pythonscript -script="$SCRIPT" -unattended -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 \
  | grep -E 'bootstrap|Error|error:|Warning: \[|Python' || true
echo "exit: ${PIPESTATUS[0]}"
