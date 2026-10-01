#!/bin/sh
# Deploy code + game data to the host with no mixed state for players (docs/hosting.md). Unlike deploy.sh, nothing live
# changes until everything is uploaded and verified:
#   1. build dist-online; stage code, server and public/assets beside the live copies on the host (*.next; unchanged
#      assets are hard-linked to the live ones, so a stage costs only the changed files)
#   2. verify every staged file against the local one by md5
#   3. swap all three in with mv (the previous versions stay as *.prev for a rollback), restart the server agent,
#      refresh the gzip copies
#   4. if the game data changed: bump CACHE_GEN in deploy/edge-worker.js and `wrangler deploy` it (SSX_NO_EDGE=1 skips)
# Usage: deploy/deploy-staged.sh            Rollback: deploy/deploy-staged.sh --rollback
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
. "$ROOT/deploy/env.sh"   # SSX_HOST, SSX_HOST_ROOT, SSX_LABEL from deploy/.env.local (the environment wins)
HOST=$SSX_HOST
APP=$SSX_HOST_ROOT/app/web
restart="launchctl kickstart -k gui/\$(id -u)/$SSX_LABEL.server; sleep 3; tail -1 ~/$SSX_HOST_ROOT/logs/server.log"

if [ "${1:-}" = "--rollback" ]; then
  ssh "$HOST" "set -e; cd ~/$APP; for d in dist-online server public/assets; do [ -d \$d.prev ] || { echo \"no \$d.prev\"; exit 1; }; done
    for d in dist-online server public/assets; do mv \$d \$d.bad && mv \$d.prev \$d && mv \$d.bad \$d.prev; done
    $restart; grep -o 'main-[A-Za-z0-9_-]*\.js' dist-online/index.html"
  # The game data went back too: retire the edge's copies of the undone version (stale-while-revalidate would mix them).
  if [ -z "${SSX_NO_EDGE:-}" ]; then
    gen=$(sed -n 's/^const CACHE_GEN = \([0-9][0-9]*\);.*/\1/p' "$ROOT/deploy/edge-worker.js")
    sed -i '' "s/^const CACHE_GEN = $gen;/const CACHE_GEN = $((gen + 1));/" "$ROOT/deploy/edge-worker.js"
    (cd "$ROOT/deploy" && npx --yes wrangler@4 deploy) | grep -E 'Deployed|Current Version|rror' || echo "edge worker deploy FAILED: run cd deploy && npx wrangler@4 deploy"
    echo "edge cache generation $gen -> $((gen + 1))"
  fi
  echo "rolled back (the undone version is now *.prev)"; exit 0
fi

cd "$ROOT/web"
# Only a core that passed every PS2 capture ships: web/test-ps2-captures.mjs stamps the sha256 of the core a full pass ran
# on. Agents rebuild runtime/core.wasm at any moment, so an unstamped core runs the captures first.
stamp=node_modules/.cache/ssx-tests/captures-ok-core
core_ok() { [ -f "$stamp" ] && [ "$(shasum -a 256 runtime/core.wasm | cut -d' ' -f1)" = "$(cat "$stamp")" ]; }
if ! core_ok; then
  echo "core $(shasum -a 256 runtime/core.wasm | cut -c1-8) not capture-verified: running the PS2 captures"
  node test-ps2-captures.mjs >/dev/null 2>&1 || { echo "PS2 captures FAILED on this core; nothing deployed"; exit 1; }
  core_ok || { echo "the core changed during the captures; run again"; exit 1; }
fi
npm run online:build >/dev/null
[ "$(shasum -a 256 dist-online/assets/core-*.wasm | cut -d' ' -f1)" = "$(cat "$stamp")" ] || { echo "the built core is not the capture-verified one (rebuilt mid-deploy?); run again"; exit 1; }
echo "core $(cut -c1-8 "$stamp") capture-verified"
# The host runs web/server/ on its own (server.next), so the server must start from that layout alone: copy it to a temp
# dir and start it on a free loopback port. A module it imports from outside server/ took the site down on 2026-09-30.
smoke=$(mktemp -d); mkdir -p "$smoke/web"; cp -R server "$smoke/web/server"
MP_HOST=127.0.0.1 MP_PORT=0 PORT=0 MP_GATE_INSECURE=1 node "$smoke/web/server/mp-server.mjs" > "$smoke/log" 2>&1 & spid=$!
i=0; while [ $i -lt 20 ] && ! grep -q 'multiplayer server on' "$smoke/log" && kill -0 $spid 2>/dev/null; do sleep 0.5; i=$((i + 1)); done
if grep -q 'multiplayer server on' "$smoke/log"; then kill $spid 2>/dev/null; wait $spid 2>/dev/null || true; rm -rf "$smoke"; echo "server starts from web/server alone"
else kill $spid 2>/dev/null || true; echo "the server does not start from web/server alone; nothing deployed:"; tail -15 "$smoke/log"; rm -rf "$smoke"; exit 1; fi
ssh "$HOST" "cd ~/$APP && rm -rf dist-online.next server.next public/assets.next && mkdir dist-online.next server.next"
rsync -a --delete dist-online/ "${HOST}:${APP}/dist-online.next/"
rsync -a --delete server/ "${HOST}:${APP}/server.next/"
rsync -a --delete --exclude=.DS_Store --link-dest=../assets/ public/assets/ "${HOST}:${APP}/public/assets.next/"

# Every staged file must equal the local one (an rsync that silently went elsewhere, a file changed mid-copy, ...).
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
for pair in dist-online:dist-online.next server:server.next public/assets:public/assets.next; do
  here=${pair%%:*}; there=${pair##*:}
  (cd "$here" && find . -type f ! -name .DS_Store -print0 | xargs -0 md5 -r | sort -k2) > "$tmp/l"
  ssh "$HOST" "cd ~/$APP/$there && find . -type f ! -name .DS_Store -print0 | xargs -0 md5 -r | sort -k2" > "$tmp/r"
  if ! cmp -s "$tmp/l" "$tmp/r"; then
    echo "staged $there differs from local $here (a file changed during the upload?):"; diff "$tmp/l" "$tmp/r" | head -10
    echo "nothing swapped; run again"; exit 1
  fi
  echo "verified $here: $(wc -l < "$tmp/l" | tr -d ' ') files"
  [ "$here" = public/assets ] && cp "$tmp/l" "$tmp/assets.staged"
done
# Did the game data change? The edge serves stale copies while it revalidates (stale-while-revalidate), so after a
# game-data change every edge could hand out one last old file (e.g. a world.json naming removed files): retire the
# edge cache generation below. A deploy with identical game data keeps the warm edge cache.
ssh "$HOST" "cd ~/$APP/public/assets && find . -type f ! -name .DS_Store -print0 | xargs -0 md5 -r | sort -k2" > "$tmp/assets.live"
assets_changed=0; cmp -s "$tmp/assets.staged" "$tmp/assets.live" || assets_changed=1
echo "game data: $([ $assets_changed = 1 ] && echo "changed ($(diff "$tmp/assets.staged" "$tmp/assets.live" | grep -c '^<') new/changed files)" || echo unchanged)"

# Pages already open keep their build: its hashed bundles, workers and core.wasm stay servable after the swap (an open
# tab lazily loads a worker or chunk minutes later; without the old file it fell back to the main thread and a 5 s
# course load took 30 s). Carry the live build's hashed files into the new one (no-clobber, mtimes kept) and prune
# carried files older than 3 days that the new build doesn't have.
ssh "$HOST" "set -e; cd ~/$APP; ls dist-online.next/assets > dist-online.next/.fresh
  [ -d dist-online/assets ] && cp -pn dist-online/assets/* dist-online.next/assets/ 2>/dev/null || true
  find dist-online.next/assets -type f -mtime +3 | while read f; do grep -qxF \"\$(basename \"\$f\")\" dist-online.next/.fresh || rm -f \"\$f\"; done
  rm -f dist-online.next/.fresh; echo \"kept bundles: \$(ls dist-online.next/assets | wc -l | tr -d ' ') files\""
ssh "$HOST" "set -e; cd ~/$APP; rm -rf dist-online.prev server.prev public/assets.prev
  mv public/assets public/assets.prev && mv public/assets.next public/assets
  mv dist-online dist-online.prev && mv dist-online.next dist-online
  mv server server.prev && mv server.next server
  $restart; grep -o 'main-[A-Za-z0-9_-]*\.js' dist-online/index.html
  /opt/homebrew/bin/node server/precompress.mjs public precompressed | tail -1"
if [ $assets_changed = 1 ] && [ -z "${SSX_NO_EDGE:-}" ]; then
  gen=$(sed -n 's/^const CACHE_GEN = \([0-9][0-9]*\);.*/\1/p' "$ROOT/deploy/edge-worker.js")
  sed -i '' "s/^const CACHE_GEN = $gen;/const CACHE_GEN = $((gen + 1));/" "$ROOT/deploy/edge-worker.js"
  (cd "$ROOT/deploy" && npx --yes wrangler@4 deploy) | grep -E 'Uploaded|Deployed|Current Version|rror' || { echo "edge worker deploy FAILED: CACHE_GEN is $((gen + 1)) locally; run: cd deploy && npx wrangler@4 deploy"; exit 1; }
  echo "edge cache generation $gen -> $((gen + 1))"
fi
echo "deployed; previous version kept as *.prev (deploy/deploy-staged.sh --rollback)"
