#!/bin/sh
# Life Framework example server: copies config.json into the profile, then starts the server.
# Set LF_REFORGER_SERVER to the folder that holds ArmaReforgerServer if it is not ~/reforger-server.
# Extra arguments are passed to the server, e.g.  ./start.sh -loadSessionSave
HERE="$(cd "$(dirname "$0")" && pwd)"
SERVER="${LF_REFORGER_SERVER:-$HOME/reforger-server}"
if [ ! -x "$SERVER/ArmaReforgerServer" ]; then
  echo "ArmaReforgerServer not found in $SERVER. Set LF_REFORGER_SERVER to your server folder." >&2
  exit 1
fi
mkdir -p "$HERE/profile/profile/LifeFramework"
cp "$HERE/config.json" "$HERE/profile/profile/LifeFramework/config.json"
exec "$SERVER/ArmaReforgerServer" -config "$HERE/server.json" -profile "$HERE/profile" -maxFPS 60 "$@"
