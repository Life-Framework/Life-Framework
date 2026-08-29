#!/usr/bin/env bash
# server/scripts/launch-test.sh - boot the headless test server on Linux.
# Usage: SERVER_EXE=/path/to/ArmaReforgerServer ./server/scripts/launch-test.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXE="${SERVER_EXE:-/home/steam/reforger/ArmaReforgerServer}"

"$EXE" \
  -config "$ROOT/server/configs/test-server.json" \
  -profile "$ROOT/server/profile/test" \
  -addonsDir "$ROOT/addons" \
  -addons LifeFramework \
  -maxFPS 60