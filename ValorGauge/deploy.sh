#!/usr/bin/env bash
# Deploy one Valor DLL to the game. Only ONE build may be installed at a
# time: both hook the same game functions from separate MinHook instances,
# so the second loader either stands down or corrupts trampolines.
#
# Usage:
#   ./deploy.sh mod    install ValorGauge.dll (gameplay mod), remove probe
#   ./deploy.sh probe  install ValorProbe.dll (research),  remove mod
#   ./deploy.sh off    remove both
set -euo pipefail

GAME="${GAME:-/e/SteamLibrary/steamapps/common/Monster Hunter World}"
PLUGINS="$GAME/nativePC/plugins"
DIR="$(cd "$(dirname "$0")" && pwd)"
MOD="$DIR/build/Release/ValorGauge.dll"
PROBE="$DIR/build/Release/ValorProbe.dll"

case "${1:-}" in
  mod)
    rm -f "$PLUGINS/ValorProbe.dll"
    cp "$MOD" "$PLUGINS/ValorGauge.dll"
    echo "installed: ValorGauge.dll (mod); ValorProbe.dll removed"
    ;;
  probe)
    rm -f "$PLUGINS/ValorGauge.dll"
    cp "$PROBE" "$PLUGINS/ValorProbe.dll"
    echo "installed: ValorProbe.dll (research); ValorGauge.dll removed"
    ;;
  off)
    rm -f "$PLUGINS/ValorGauge.dll" "$PLUGINS/ValorProbe.dll"
    echo "removed both Valor DLLs"
    ;;
  *)
    echo "usage: $0 mod|probe|off" >&2
    exit 1
    ;;
esac
