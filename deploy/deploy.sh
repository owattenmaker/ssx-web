#!/bin/sh
# Build the hosting bundle here (the prep machine holds the extracted assets) and update the host (docs/hosting.md;
# host, folder and label from deploy/.env.local). Usage: deploy/deploy.sh [--no-assets]
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
. "$ROOT/deploy/env.sh"
HOST=$SSX_HOST H=$SSX_HOST_ROOT L=$SSX_LABEL
cd "$ROOT/web"
npm run online:build
rsync -a --delete server/ "$HOST:$H/app/web/server/"
rsync -a --delete dist-online/ "$HOST:$H/app/web/dist-online/"
[ "${1:-}" = "--no-assets" ] || rsync -a --delete --exclude=.DS_Store public/assets/ "$HOST:$H/app/web/public/assets/"
# gzip copies of the game data (server/precompress.mjs; only new/changed files are compressed)
ssh "$HOST" "cd ~/$H/app/web && /opt/homebrew/bin/node server/precompress.mjs public precompressed"
rsync -a "$ROOT/deploy/ssx-server.sb" "$HOST:$H/etc/ssx-server.sb"
ssh "$HOST" "sed \"s|__HOME__|\$HOME|g\" > ~/Library/LaunchAgents/$L.server.plist" < "$ROOT/deploy/$L.server.plist"
ssh "$HOST" "launchctl bootout gui/\$(id -u)/$L.server 2>/dev/null; launchctl bootstrap gui/\$(id -u) ~/Library/LaunchAgents/$L.server.plist && sleep 2 && tail -3 ~/$H/logs/server.log"
