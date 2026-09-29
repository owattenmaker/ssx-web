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
- **Gzip copies:** `web/server/precompress.mjs` (run by `deploy/deploy.sh` on the host) writes
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
deploy/deploy.sh              # build, sync code + assets + sandbox profile, reinstall and restart the server agent
deploy/deploy.sh --no-assets  # code only
deploy/deploy-staged.sh       # preferred: stage code + assets beside the live copy, md5-verify, swap together (*.prev kept)
deploy/deploy-staged.sh --rollback  # swap the previous version back in
```

Checks: `web/test-mp-gate.mjs` (in `npm test`); end to end over the public URL (login, Snow Jam race with computer
riders, two-player online race through the tunnel: packets both ways, 13 ms RTT) on 2026-09-24.

## Game data

`web/public/assets/` (about 2.5 GB) is extracted from your own discs by `npm run setup`/`tools/`; it is git-ignored
and is copied to the host directly. It is never committed or published.
