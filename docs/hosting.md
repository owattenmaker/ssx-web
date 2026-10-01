# Hosting

The game and the multiplayer server run as one Node process (`web/server/mp-server.mjs`) on a Mac at home (the host:
`SSX_HOST` in the git-ignored `deploy/.env.local`, see "Deploy configuration"), reached from the internet only through a
Cloudflare Tunnel.

```
browser ──https/wss──> Cloudflare (the site)          ──tunnel (outbound from the Mac)──> cloudflared
                                                                                              │
                                                              127.0.0.1:8787  mp-server.mjs ◄─┘
                                                              (static game + /mp WebSocket + password gate)
```


**Public since 2026-10-01.** The password gate is off: the host plist has no MP_GATE_PASSWORD_FILE (backup ~/ssx-host/state/server.plist.bak-20261001-gate) and the worker has no GATE_SECRET, so the edge checks no session. The origin sends `x-robots-tag: noindex, nofollow` and serves a disallow-all /robots.txt. To gate it again: restore MP_GATE_PASSWORD_FILE in the plist and reload the agent, then `cd deploy && npx wrangler@4 secret put GATE_SECRET --name ssx-edge` with ~/ssx-host/state/gate-secret.

## Security model

- **No inbound ports.** `cloudflared` only dials out to Cloudflare; the Node server listens on `127.0.0.1` only, so
  nothing new is reachable from the LAN or the tailnet.
- **Password gate** (`web/server/gate.mjs`): every request (page, assets, `/mp/status`, the `/mp` WebSocket upgrade)
  needs a signed session cookie. Without it a page request gets the login form, anything else 401. Login is rate
  limited per client address (CF-Connecting-IP behind the tunnel, `MP_TRUST_PROXY=1`), compares in constant time,
  and sets an `HttpOnly; Secure; SameSite=Lax` cookie (30 days, HMAC-SHA256 with a secret generated on first start).
  Redirects after login go only to same-site paths, so an invite link keeps its lobby query.
- **Login page** (`gate.mjs page()`, 2026-09-25): styled like an SSX 3 front-end screen (sky-blue gradient, orange
  swoosh, drifting snowflakes, slanted white "SSX 3" with an orange 3, dashed header rule, right-aligned "Password" menu
  row, pulsing "Press START" button, PS2 ✕ "Enter" legend). It is shown before login, so it uses no game data at all
  (no /assets, no atlases/logos/fonts, nothing inlined from the disc): one `<style>` (~7 KB page), inline SVG shapes and
  system fonts. Every gate HTML response sends `Content-Security-Policy: default-src 'none'; style-src 'sha256-<hash of
  the style>'; form-action 'self'; base-uri 'none'; frame-ancestors 'none'` (the hash is computed at module load, so it
  follows CSS edits). Real `<label>`, `<main>`, `<h1>`, `role="alert"` error line, 16px+ input (no iOS zoom), 44px+
  targets, safe-area padding, reduced-motion stops the snow and pulse. Form fields, messages and redirects unchanged.
- **Static hardening** (`serveStatic`): no dotfiles, no `..`/backslash segments, symlinks resolved and kept inside
  the roots, `nosniff`, `SAMEORIGIN` framing, `same-origin` referrer.
- **Server sandbox:** the Node process runs under `sandbox-exec` (`deploy/ssx-server.sb`), verified on the host: no
  outbound network except loopback (LAN / tailnet / internet connects fail with EPERM), no reads under `$HOME` outside
  `~/ssx-host` (`~/.ssh`, `~/Documents` fail), writes only in `~/ssx-host/state` and `logs`, no spawning programs.
  macOS does not enforce local-address filters on bind/inbound in SBPL, so the listen address is pinned by the launch
  arguments and `MP_REQUIRE_LOOPBACK=1` (the server exits unless `--host` is loopback); port 8787 is closed from the tailnet.
- **Least credential on the host:** the account-wide `~/.cloudflared/cert.pem` from `cloudflared tunnel login` was
  deleted after the tunnel and DNS route were created; only the tunnel's own credentials file (mode 600) remains.
  Changing tunnels/DNS later needs `cloudflared tunnel login` again (browser approval).
- **WebSocket origin:** `MP_ORIGINS=https://<the site>` (in the local server plist).
- Password (`~/ssx-host/etc`) and cookie secret (`~/ssx-host/state`) are mode 600 on the host, never in the repo.

## Downloads, compression and caching

- **Streaming, not a bulk download:** a first visit transfers about 31 MB before the title screen (Snow Jam world,
  animations, core); riders, other courses and music load when reached. `web/downloads.js` (first import of main.js)
  routes every `/assets/*` GET through one shared download per URL (course files were fetched twice by main.js and
  ai-race.js) and counts the bytes. The progress is shown only through the game's own screens, in their own style:
  the event loading screen's percentage cannot run ahead of the download (web/loading-screen.js), the title card's
  "Loading..." line gets a percentage, and menus show a FEFONT "Loading..." (bottom right, help-text colour) while
  something streams for more than 0.6 s (web/ui.js drawStreamingNote). No extra widget; nothing on cache hits.
  Since 2026-09-26 the first load shows the original title from the first paint with the Select Character meter as its
  load bar (docs/first-load.md).
- **Texture archives (2026-09-25):** world textures come from one shared library `/assets/TEXTURES/world.tex` (4.9 MB,
  all 788 world textures) instead of ~200-350 PNGs per course, and each rider's textures from
  `/assets/WARDROBE/<ID>/textures.tex` (default outfits, ~0.1 MB) plus `gear.tex` (Equip Gear items, 3-11 MB, only
  when an outfit needs them) (docs/asset-formats.md "Texture archives"). They are ordinary game files for
  the server, the edge worker and web/downloads.js: one edge-cache entry each, revalidated with the ETag on every
  visit (one 304 instead of hundreds), counted by the loading screens. The payload is PNG, so precompress.mjs keeps no
  gzip copy (gzip saves 3%). Deploying them is a full asset deploy: the new world.json / wardrobe.json files reference
  the archives, and the per-location PNGs are gone from the export.
- **Gzip copies:** `web/server/precompress.mjs` (run on the host by both deploy scripts) writes
  `~/ssx-host/app/web/precompressed/<path>.gz` for every file it shrinks by 10%+ (1347 MB -> 614 MB; `colors.bin`
  8.1 MB -> 314 KB). The server sends them to gzip clients (`MP_PRECOMPRESSED`), never when older than the file, and
  always adds `X-Decoded-Length` for the progress bar. Cloudflare does not compress `application/octet-stream` itself.
- **Edge cache and the gate:** a shared cache keys on the URL only, so it must never hold a gated file unless something
  checks the session first. The origin marks gated responses `private` + `CDN-Cache-Control: no-store` (Cloudflare
  bypasses them). `deploy/edge-worker.js` (Cloudflare Worker `ssx-edge` on the site's `/*` route, the local `deploy/wrangler.toml`)
  runs before Cloudflare's cache: it verifies the gate cookie (same HMAC, `GATE_SECRET` = the host's
  `state/gate-secret`), answers 401 otherwise, and fetches `/assets/*` through the cache with `X-SSX-Edge` =
  `EDGE_SECRET` (`etc/edge-secret`), for which the origin returns shareable headers (`CDN-Cache-Control: max-age=30d`).
  Deploy: `cd deploy && npx wrangler login` (browser approval, keychain) then
  `ssh host cat ~/ssx-host/state/gate-secret | npx wrangler secret put GATE_SECRET`, the same for `EDGE_SECRET`, and
  `npx wrangler deploy`. **Deployed 2026-09-24** (wrangler OAuth with only account/user/zone read + workers/scripts/routes write, in the prep Mac's keychain). Verified: every file cached before the gate fix answers 401 without a session; logged-in requests MISS once, then HIT (gzip kept, `private` to the browser); game load and a two-player online race through the worker.
- **Stale edge copies answer at once (2026-09-26, docs/first-load.md):** the origin marks edge copies of game files
  `CDN-Cache-Control: max-age=300, stale-while-revalidate=604800` (was `max-age=300`). A repeat visit revalidates every
  game file with the browser's ETag; when the edge copy was older than 5 minutes, each of those waited on a round trip
  through the tunnel to this host, and the loaders ask for ~46 of them one after another before the title (measured:
  +100 ms per trip = +4.6 s to "Press START", 5.1 s -> 9.7 s). Now the edge answers from its copy and refreshes it in the
  background. **After an asset deploy, bump `CACHE_GEN` in `deploy/edge-worker.js` and `npx wrangler deploy`**, so no
  PoP hands out one last stale copy of a changed file (a stale world.json could point at files the deploy removed).
  Code deploys need nothing (hashed bundles; index.html always goes to the origin).
- **Hardening found in hosting:** zero-byte files (Sam's `fe/morphs.bin`) crashed the server (read stream end -1);
  fixed, and a failing static request now answers 500 instead of taking the process (and its races) down.

## Layout on the host

```
~/ssx-host/
  app/web/{server,dist-online,public/assets}   # code (from the repo build) + game data (rsync from the prep machine)
  etc/{gate-password,ssx-server.sb}   state/gate-secret
  cloudflared/config.yml                        # tunnel -> http://127.0.0.1:8787
  logs/
~/Library/LaunchAgents/<SSX_LABEL>.{server,tunnel}.plist   # deploy/<SSX_LABEL>.*.plist with __HOME__ filled in
```

- Tunnel: a named Cloudflare Tunnel with a CNAME for the site; config in `~/ssx-host/cloudflared/config.yml` (that
  hostname -> `http://127.0.0.1:8787`, everything else 404). Other tunnels on the account are untouched.
- Services are per-user LaunchAgents (`launchctl print gui/$(id -u)/<SSX_LABEL>.server`, `...tunnel`); they start
  when the host user is logged in (the console session). Restart: `launchctl kickstart -k gui/$(id -u)/<label>`.
- Logs: `~/ssx-host/logs/{server,tunnel}.log`. Password: `~/ssx-host/etc/gate-password` (edit, then restart the server;
  existing sessions stay valid until the cookie secret `~/ssx-host/state/gate-secret` is deleted).

## Deploy configuration

Nothing that names the host is in the repo. `deploy/env.sh` (sourced by both deploy scripts) reads the git-ignored
`deploy/.env.local`; start from `deploy/.env.example`:

- `SSX_HOST` (ssh target `user@host`), `SSX_HOST_ROOT` (the host folder under that user's home, `ssx-host` above),
  `SSX_LABEL` (the launchd label prefix: `<SSX_LABEL>.server` / `.tunnel`). A variable set in the environment wins
  over the file (`SSX_HOST=me@other deploy/deploy-staged.sh`); `sh deploy/env.sh` prints the resolved values.
- Also local-only (`deploy/.gitignore`), each made from a committed template: `deploy/wrangler.toml` (the site's route
  and zone; `wrangler.toml.example`), `deploy/<SSX_LABEL>.server.plist` and `.tunnel.plist` (label, host folder, the
  site's origin for `MP_ORIGINS`; `ssx.server.plist.example`, `ssx.tunnel.plist.example`), and wrangler's
  `deploy/.wrangler/` cache (the Cloudflare account).
- The public code repo is synced from this tree with these excludes (local-only config, game data, generated and
  lifted code, agent scratch); its root files (README.md, LICENSE, CONTRIBUTING.md, .gitignore) are kept in the repo:

  ```sh
  rsync -a --exclude=node_modules --exclude='dist*' --exclude='runtime*' --exclude=generated \
    --exclude=public/assets --exclude=public/runtime --exclude=public/test-data --exclude=bin --exclude=obj \
    --exclude='*.pdb' --exclude='*.dll' --exclude='*.pkl' --exclude='*.p2s' --exclude=.DS_Store --exclude=__pycache__ \
    --exclude=.env.local --exclude=.env --exclude=.wrangler --exclude='deploy/wrangler.toml' --exclude='deploy/*.plist' \
    --exclude='web/.*.mjs' \
    web engine tools docs licenses deploy tests <public repo checkout>/
  ```

## Deploy / update

On the prep machine (it holds the extracted assets, which never go to GitHub):

```sh
deploy/deploy-staged.sh       # every deploy: stage code + assets beside the live copy, md5-verify, swap together (*.prev kept)
deploy/deploy-staged.sh --rollback  # swap the previous version back in
deploy/deploy.sh              # first install, or a change to deploy/ssx-server.sb or the server plist: build, sync code + assets,
                              # install the sandbox profile and the LaunchAgent plist, restart the server agent (not staged)
deploy/deploy.sh --no-assets  # the same without the game data
```

Checks: `web/test-mp-gate.mjs` (in `npm test`); end to end over the public URL (login, Snow Jam race with computer
riders, two-player online race through the tunnel: packets both ways, 13 ms RTT) on 2026-09-24.

## Online records (docs/online-records.md)

The boards and their replays (web/server/records.mjs) are off until the server has `MP_RECORDS_DIR`. Host change (the coordinator
applies it; not done by the feature's deploy):

- `~/Library/LaunchAgents/<SSX_LABEL>.server.plist`, EnvironmentVariables: `<key>MP_RECORDS_DIR</key><string>__HOME__/ssx-host/state/records</string>`
  (`__HOME__` as in the other keys), then `launchctl bootout` / `bootstrap` the agent (or `kickstart -k` after a `launchctl` reload of
  the plist). The directory is created on start.
- Sandbox (`deploy/ssx-server.sb`): no change. Writes are allowed under `~/ssx-host/state` (board.json, replays/), reads under
  `~/ssx-host`; the disc tables come from the static root (`public/assets/CAREER/career.json`, `<code>/npc-riders.json`).
  Checked locally under the profile with sandbox-exec (a copy of web/server/ alone in a host-like tree): it starts and creates
  `state/records/replays`.
- `state/` survives deploys (deploy-staged swaps `app/` only), so the boards and replays persist; back up `state/records` with the
  rest of `state` if anything is backed up.
- Size: at most 100 runs per board, 20 boards, each replay ~2-60 KB deflated: under ~150 MB worst case.
- Privacy: nothing about a client is stored (no address, user agent, cookie); the rate limits key on the address in memory only. The
  boards hold names players typed, rider, time / score, the UTC day, core / build ids. diag.log is unrelated and never served.
- Endpoints (behind the password gate like everything else): `GET /mp/records`, `GET /mp/records/board?event=`, `GET
  /mp/records/replay?id=`, `POST /mp/records/submit`. With the directory unset or unusable they answer 503 and nothing else changes.

## The records verifier (docs/online-records.md "The verifier")

`web/server/records-verifier.mjs` re-simulates submitted runs in headless Chrome with the game's own page and reports to mp-server on
loopback. It is its own LaunchAgent, **not** under the server sandbox (it starts Chrome); it runs niced (`nice -n 19` for Chrome,
launchd `Nice` 10, `ProcessType Background`, `LowPriorityIO`), one run at a time, skips while the 1-minute load per core is above
0.6 or an online race is running (`/mp/status`), and closes Chrome whenever its queue is empty.

Install on the host (once):

1. Chrome: Google Chrome in /Applications (or set `--chrome` in the plist to another Chrome / Chromium; WebGPU must work headless).
2. The shared secret: `openssl rand -hex 32 > ~/ssx-host/state/verifier-token && chmod 600 ~/ssx-host/state/verifier-token`.
3. The server reads it: `MP_RECORDS_VERIFIER_TOKEN_FILE` = `__HOME__/ssx-host/state/verifier-token` in the server plist's
   EnvironmentVariables (deploy/ssx.server.plist.example and the local `<SSX_LABEL>.server.plist` have it), then reload the server agent.
   Without the file the verifier endpoints answer 404. They also answer 404 to anything not on loopback or carrying CF-Connecting-IP
   (a request through the tunnel), so the endpoints are never reachable from the internet.
4. The agent: `deploy/<SSX_LABEL>.verifier.plist` from `deploy/ssx.verifier.plist.example` (label, `__HOME__`), copied to
   `~/Library/LaunchAgents/` with `__HOME__` filled in, `mkdir -p ~/ssx-host/state/verifier`, then
   `launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/<SSX_LABEL>.verifier.plist`. Log: `~/ssx-host/logs/verifier.log` (one line
   per run: the verdict, wall time, the page's simulation time, Chrome's CPU seconds and the process tree's peak memory).
5. After a code deploy nothing is needed: the verifier loads the page fresh for each batch; a new core makes it re-verify the listed
   runs (D7 (3)). To stop it: `launchctl bootout gui/$(id -u)/<SSX_LABEL>.verifier`.

Admin by hand: `cd ~/ssx-host/app/web && node server/records-admin.mjs --dir ~/ssx-host/state/records flagged | list <event> |
show <id> | approve <id> | delete <id> | requeue <id>` (the running server reads the changed board on its next request).

The log (`~/ssx-host/logs/verifier.log`):
- **at start:** `start: page ..., chrome ...`, then `chrome <version>; WebGPU available` (or a `!!! WEBGPU UNAVAILABLE ...` line, repeated every
  cycle: nothing can be verified), then `core <id> (build <id>)`;
- **every cycle (60 s):** `cycle N: K queued (core ...)`, or `cycle N: skipped (load | online race)`;
- **per run:** `run <id> <event> claim ...: reproduced / NOT reproduced (reason) -> verified | pulled | stale | listed ...` with its timings.
Chrome starts only when there is work: the core id is read once per build (`/build.json` changes at a deploy).

A live check: `node server/records-admin.mjs --dir ~/ssx-host/state/records list 0:ARA1` for an id, then `requeue <id>`; within a minute the
log shows `cycle N: 1 queued` and the run's verdict (a requeued run that was pulled stays hidden until it verifies).


## Game data

`web/public/assets/` (about 2.5 GB) is extracted from your own discs by `npm run setup`/`tools/`; it is git-ignored
and is copied to the host directly. It is never committed or published.
