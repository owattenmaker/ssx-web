# Workers: stale-safe start, build handshake, fallbacks (2026-09-26)

Every worker of the browser game starts through one helper, `web/worker-guard.js`. It makes sure a page only talks to a
worker from its own build. When a worker cannot be used, the same job runs on the main thread: the player sees no
break, and diagnostics say what happened.

## What can go stale

| Case | What used to happen | Now |
| --- | --- | --- |
| A tab open (or restored from the back/forward cache) across a deploy creates a worker lazily. Its hashed file was deleted by the deploy (`dist-online` is replaced), so the request gets a 404 (or an HTML fallback). | peak-world / fe-preview had no `onerror`: Peak 1 stayed on its loading screen, the Select Character preview never came. Terrain refinement switched itself off. | The start fails, the worker is fetched once more with a cache-busting query, then the job runs on the main thread (`worker-start-failed`, `worker-fallback`). |
| A cache (browser, proxy, CDN) returns another build's script for a hashed URL. | Nothing noticed it. | The build handshake mismatches: `worker-stale`, one refetch with `?v=`, then the main thread. |
| Dev server: a page loaded before a worker's code changed (agents edit files all the time) gets the new worker code. | Protocol drift between old page code and new worker code. | `WORKER_BUILDS` (below) is a hash of each worker's own module graph. The worker reports the new hash, and the page falls back to its own, consistent copy of the handler. |
| A worker crashes (`error`, `messageerror`) or hangs mid-job. | Requests waited forever (or failed silently). | The worker is terminated and restarted once, with the requests in flight replayed; after that the main thread takes over. |
| A back/forward-cache restore returns with a dead worker. | Hung requests. | `pageshow` (persisted) pings each worker; one that does not answer within 5 s is replaced. |
| A service worker on the origin serves old files. | (Never registered by the game.) | Any registration is unregistered, its Cache Storage cleared, and a page it controlled reloads once. |
| A tab left open for days never picks up the new deploy. | It kept running the old build. | `/build.json` is checked. A stale page reloads into the new build the next time it sits on the title screen. |

## Pieces

- **`web/build-id.js`:**
  - `BUILD_ID` is filled by the Vite plugin `web/vite-build-id.js`. A build gets one id per `vite build` (`b<UTC stamp>-<6 hex>`), shared by the page, every worker bundle and `dist/build.json`. The dev server uses `dev-<start>`; node gets `unbuilt`.
  - `WORKER_BUILDS` is dev-server only: per worker, a hash of its module graph (the entry and its local imports), recomputed after any file change (the plugin invalidates the module on every watcher event and serves `/build.json`).
  - The plugin is in both `plugins` and `worker.plugins` of `web/vite.config.js`: Vite builds worker bundles with `worker.plugins` only.
- **`web/worker-guard.js` (page side):**
  - `workerUrl((Worker) => new Worker(new URL('./x-worker.js', import.meta.url), {type: 'module'}))` returns the URL Vite built (content-hashed, same build) without starting a worker. Vite rewrites that literal pattern; the parameter shadows the global. In node it returns the file URL.
  - `createGuardedWorker({name, url, local, raw?, ...})` starts the worker and waits for its hello:
    - the right build: requests or messages are flushed to it;
    - another build: terminate, one refetch with `?v=<BUILD_ID>.<time>`, then the main thread;
    - a start failure (error, `messageerror`, no hello in 15 s; the timer waits while the page is hidden): the same path.
  - Request mode, `guard.request(data, transfer) -> Promise`:
    - the main-thread handler gets a `structuredClone` of the input, as a worker would (`cloneInput: false` for handlers that never change it: the audio decoder);
    - the requests in flight are replayed on a restarted worker or on the main thread, except those whose buffers were transferred to the dead worker (they reject, and callers fall back per request).
  - Raw mode (`{raw: true}`) returns a Worker-like object (`postMessage`, `onmessage`, `onerror`, `terminate`) for workers with their own protocol. Messages queued before the handshake go to the main-thread handler if the worker never starts. A raw worker that dies after its start lost its state, so the caller's `onerror` decides.
  - Watchdog: while a worker has work (or always, in raw mode), it is pinged every 5 s. No answer and no other message for `stallTimeoutMs` (30 s; peak-world 60 s) means it is treated as hung. The watchdog pauses while the page is hidden.
  - `?workers=0` runs every worker's handler on the main thread. `ssxWorkers()` lists the live guards (state, attempt, restarts, build).
- **`web/worker-guard-child.js` (worker side):**
  - `serveWorker(name, handler | factory, {raw})` sends the hello (`build: expectedWorkerBuild(name)`) and answers pings and requests.
  - Errors carry the message: Firefox's `Error.stack` has only the frames, so the peak-world worker used to send Firefox errors without their text.
- **`web/build-check.js`** (imported by worker-guard):
  - Checks `GET /build.json` with `cache: 'no-store'`: on a `pageshow` restore, when the page is shown after 5+ minutes hidden, on `online`, every 30 minutes, 20 s after start, and on a worker mismatch.
  - Another id: `build-stale` (diagnostics) and `<html data-stale-build>`. On the title screen (visible) the page reloads once into the new build (`build-reload`; a sessionStorage mark prevents loops).
  - Only on hosted pages (https) or with `?buildcheck=1`.
  - Service workers are cleaned up (`service-worker-removed`), unless `?sw=keep`.
- **Diagnostics** (web/diagnostics.js, on the hosted site or with `?diag=1`): `worker-stale`, `worker-recovered`, `worker-start-failed`, `worker-failed`, `worker-fallback`, `worker-unavailable`, `build-stale`, `build-reload` and `service-worker-removed`, each with the worker name, the expected and received build, and the action taken.

The workers and their main-thread handlers:

| Worker (name) | Entry | Handler (shared with the main thread) | Callers |
| --- | --- | --- | --- |
| peak-world | peak-world-worker.js | peak-world-prepare.js `preparePeakLocation` | peak-world.js |
| terrain (raw) | terrain-worker.js | terrain-worker-core.js `createTerrainWorkerHandler` | terrain-refinement.js |
| fe-preview | fe-preview-worker.js | fe-preview-prepare.js `prepareFrontEndPreview` | fe-preview.js |
| audio-decode | audio-decode-worker.js | audio-decode-job.js `decodeAudioJob` | sfx.js (banks), pathfinder.js (music bars) |
| texture-decode | texture-decode-worker.js | texture-decode-job.js `decodeTextureJob` | texture-archive.js (Xbox HD rider BC entries: the BC mip chain or the texels, docs/xbox-textures.md section 8) |

`net/mp-game.js` also has a Blob-URL ticker worker for hidden tabs in online races. It is generated from inline code, so it cannot be stale, and it already falls back to timers.

Adding a worker:
1. Put the job in a module that runs anywhere (no DOM, no `self`).
2. Write a two-line entry that calls `serveWorker('<name>', job)`.
3. Add the name to `WORKER_ENTRIES` in web/vite-build-id.js.
4. Start it with `createGuardedWorker({name, url: workerUrl(...), local: () => job})`.
5. Import the job statically: a dynamic `import()` would be a lazy chunk, which an old page cannot load after a deploy either.

## Caching (server, edge)

- `web/server/mp-server.mjs`:
  - `hashed` is now exactly Vite's output: `^/assets/<name>-<8 hash chars>.(js|css|wasm)$`, top level of `/assets` only. It was `{8,}` anywhere under `/assets/`, which also matched names like `terrain-worker-core.js`.
  - Those files are `max-age=31536000, immutable`, pages `no-cache`, and `/build.json` `no-store`.
- `deploy/edge-worker.js`:
  - The browser header is now the origin's decision: `immutable` only when the origin's `cache-control` says so (a 404 never does). It used to run its own looser regex.
  - **The edge worker deploys separately:** `cd deploy && npx wrangler deploy`. Until then the old edge worker keeps its regex, which gives the same answer for every file that exists today.
- `/build.json` goes straight to the origin: it is not under `/assets/`, and Cloudflare does not cache JSON by default. The origin marks it `no-store`.
- index.html is revalidated (`no-cache`); every worker and core file is content-hashed; game data under `/assets/*` is revalidated with its ETag. With a build id in every bundle, the main bundle and workers get new names on every build (they used to keep them when the code was unchanged).
- Service workers: none is registered anywhere in the app (web/, the server, the boot plugin, the GitHub mirror's history). `build-check.js` removes any that a shared dev origin or a leftover put there.

## Verification (2026-09-26)

- **`web/test-worker-guard.mjs`** (npm test), with a scripted Worker:
  - same build;
  - stale once, then fresh after the refetch (nothing reaches the stale worker; the `?v=` URL);
  - stale twice: the main thread, on a copy of the input;
  - 404, no hello, or a constructor that throws; no main-thread handler: rejects and `onError`;
  - error, messageerror or a hung worker at run time: restart with replay, then the main thread; a transferred buffer is not replayed;
  - an idle worker is not pinged;
  - back/forward revalidation, both dead and alive;
  - raw mode: queued init, a copy for the main thread, a crash goes to the caller;
  - terminate: no callbacks afterwards, and the registry forgets the guard;
  - the plugin: graph hashes follow only the worker's own files; the build value is filled in.
- `test-edge-worker.mjs`: look-alike names and 404s are not `immutable`. `test-mp-gate.mjs`: a hashed bundle is `immutable`, a look-alike is not, and `/build.json` is `no-store` and gated.
- **Browsers:**
  - Setup: a QA server (session scratchpad `qa-workers/server.mjs`) served two builds of the same code (A = the page, B = "another deploy") with the host's headers. Per scenario it could serve B's worker for A's URL (without `?v=`, or always), a 404, `index.html` as the worker (an SPA-style fallback), or a script that never finishes loading.
  - The test page: `web/worker-guard-test.html` (built with the QA config) starts the four real workers as the game does and runs one request on each.
  - Results were POSTed back, and the diagnostics events reached `/mp/diag`.
  - **Chrome 149 (headless), Firefox 156 (headless, BiDi), Safari 27 (macOS) and iOS 26.5 Safari (Simulator, iPhone 17 Pro):**
    - normal: all four in workers;
    - stale-once: `worker-stale` x4, refetch, `worker-recovered` x4, all in workers;
    - stale-always: `worker-stale` x8, `worker-fallback` x4, all correct on the main thread;
    - 404 and HTML: `worker-start-failed` x8, `worker-fallback` x4, main thread;
    - never-loading script (Chrome): hello timeout after 15 s, refetch, main thread at 30 s;
    - `?workers=0`: main thread.
  - The browsers cache the immutable worker files as intended. Safari and Chrome served a later run's "stale" URL from their cache: the right content.
- **The game itself** (the built bundle, headless Chrome):
  - Snow Jam with `autostart=1`: terrain, audio-decode and fe-preview in workers, terrain refinement 20 patches, 60 fps.
  - Peak 1 free ride: peak-world and audio-decode in workers.
  - The same with every worker file answering 404, or stale: every job on the main thread, the race runs, Peak 1 loads, refinement is still 20 patches.
- **New deploy while on the title:** `/build.json` switched to build B. `build-stale` then `build-reload`, and index.html loaded twice (one reload, no loop).
- **Service worker:** a registered SW controlling the page, with one Cache Storage cache. After load it was unregistered, the cache cleared, the page reloaded and was no longer controlled (`service-worker-removed` in diagnostics).
- **Dev server:** the page loaded, then `peak-world-prepare.js` was edited. A new peak-world worker reported the new graph hash: `worker-stale` x2, then the main thread. The terrain worker, whose graph is unchanged, still started. After reverting the edit, peak-world matched again.

# Game tick: simulate / present (stage A, 2026-09-26)

`web/game-tick.js` is main.js's former `simTick`. The frame clock, `ssxQA.advance` and the online hidden-tab ticker all
run the game through it.

- `createGameTick(host, {trace})`: `host` has getters and setters over main.js's game state and its collaborators
  (main.js `gameHost`).
  - `simulate(input, ticksLeft)` makes every core call of one tick, in the original order, and returns the tick record.
    It returns null while a streamed world waits for its data (2306B8).
  - `present(rec)` does the page's work with that record: HUD pre-pass, rider frame, skin and lighting uploads, tint,
    sun, glare and thunder, terrain refinement, trick name, career run end, results, rumble, audio and collectibles.
  - `tick(input, ticksLeft)` runs one tick, and `advance(clock, seconds, input)` runs a frame's ticks on the fixed 60 Hz
    clock.
- **QA hooks:**
  - `?simtrace=1` pushes a per-tick FNV into `window.__simTrace.ticks`. It hashes the human's outputs, every rider's
    state, pose and world state, the visual and game RNG words, and the score object.
  - `window.__memoryHash()` gives an FNV of the whole wasm memory.
  - Use them to show that a change to the game loop is bit-identical: take traces before and after, with a fresh
    browser profile per run. The relationship tables live in local and session storage and age at every start, so two
    runs in one profile diverge.
- **Verified identical** before and after the extraction, and again after the simulation worker was removed (below):
  - 3000-tick traces on Snow Jam with 5 computer riders (tuck, and a scripted carve / jump / grab pad), Metro City,
    Ruthless and a Peak 1 free ride;
  - a whole Snow Jam race with a restart, to the results screen: 14,471 ticks, the same result rows.

# Frame pacing after a hitch: the PS2's frame loop (pv stallCap, 2026-09-28)

A drawn frame advances the game as the PS2's frame loop does (`web/ps2-frame-pacing.js` `ps2FrameTime`, used by main.js
`frame` offline). Before, the fixed clock kept the whole debt of a slow frame and replayed it at 12 ticks a frame. A Safari
stall mid-rotation then fast-forwarded the air auto-complete, and the rider seemed to snap from inverted to upright.

**The PS2** (recompiled `local/output`):
- `main` 0x31AF80 runs the app framework 0x316D60 on the app object `*(gp+0x2A74)` = 0x4C9428 (+0x10 = 60 Hz,
  +0x14 = 1/60, **+0x20 = 12**).
- One game update runs per pad sample. The pad writer 0x326B88 stores one sample per vblank in a 30-slot ring
  (`*(gp-0x850)`, read +0x2EE0, write +0x2EE4 = (write + 1) % 30, overwriting).
- The frame loop 0x316F00 runs the game update (game vtable +0x34), then the app's +0x34 (0x227E98). When a sample is
  pending (0x326B48), that feeds it to the pad history 0x321298 and the loop runs the next update at once, without
  drawing.
- 0x317188: at most app+0x20 = 12 updates per drawn frame. Past that, +0x2C (0x227E68 -> 0x326C60) resyncs the ring
  (read = write - 1) and the backlog is dropped.

**Measured** with stall savestates. `tools/ps2_stall_state.py` adds a stub to the capture's provider hook that spins K extra vblanks at Crow's Nest
tick 700. The record watches hold the vblank counter 0x50AAA8 (VBLANK_S handler 0x3C1980) and the ring indices.

| K extra vblanks | Catch-up updates |
| --- | --- |
| 2, 4, 11 | K |
| 12..29, 45..59 | 11 |
| 31, 35, 40 (ring lap) | 1, 5, 10 |
| 60, 90, 120 | 0 |

- K = 30 ran 11: the write index had lapped onto the read index (a sub-frame race; the lap model says 0).
- The sim state equals the unstalled run in every case.
- A 50 % EE clock (`PS2_CAPTURE_EECYCLE=-3`, tools/ps2_capture.py) shows the same pattern: 2-vblank frames, each followed
  by a 0-vblank catch-up update.

**The port.** Offline, `Math.min(ps2FrameTime(dt), quality.maxFrameDt)`: min(F, 12) ticks for a frame of F ticks, F
lapped by the 30-slot ring, and no debt kept. The low tier's 4/60 still applies on top.
- Online races keep `mpGame.pace` (the full catch-up): the race is server-timed.
- The replay clock is its own.
- Race results are tick-counted. A BHP1 event to the finish with six 900 ms stalls matched the unstalled run on all
  4439 ticks (finish at 4403, result 4222 ticks).
- `test-frame-clock.mjs` gates the 17 PS2 data points.

## Keyboard input during a hitch (pv stallKeys, 2026-09-28)

On the PS2 each catch-up update reads the pad sample of its own vblank (the 30-slot ring). The page used to give every
catch-up tick the frame's one sample, so a key released during a stall acted from the first catch-up tick.
- With `stallKeys` (main.js `stallKeyInput`, game-tick.js `advance(clock, seconds, input(k))`), a frame that runs k > 1
  ticks gives its tick k the keyboard as it was at previous frame + (k + 1)/60 s. The state is rebuilt from the
  keydown/keyup event timestamps logged since the previous frame.
- Ticks at or past the frame time read the current keys. A frame of one tick is unchanged.
- Simple-mode roles are rebuilt in time order from the frame's start.
- Gamepads and touch cannot be read during a stall, so they keep the frame's sample.
- Offline only.
- Verified: in the Crow's Nest release run, a 250 ms stall 4 ticks before the D-pad release, with the keyup inside the
  stall, stays exact against the PS2 in physics, air state and the board bone on every control-5 tick (Chrome, WebKit).
  Without the switch it leaves at 700: the release acted 4 ticks early.
- Simple mode gives the same run with and without the stall.

## One tick per drawn frame at 60 Hz (pv tickLock, off; 2026-10-04, lag agent)

**Input-to-display path, as the page runs it:**
- frame(ms) runs from the animation frame. It reads the pad and keys first (inputs()), then runs the game ticks, then draws, in the same
  callback. The page does not wait on anything in the frame, and it queues no frames of its own. The browser's compositor adds its
  usual vsync.
- A slow frame runs at most 12 ticks and drops the backlog (stallCap). This is the PS2 frame loop 0x316F00 / 0x317188, above.
- The page draws between the last two ticks at alpha = the clock's leftover x 60 (main.js renderAlpha, for the rider and the camera).
  So the view shows a state 0..1 tick older than the newest tick.
  - The leftover's phase is set by when the run started. Replaying the clock on recorded rAF times gives a mean of 5-10 ms of extra
    display lag, and up to 16.7 ms.
  - When the phase sits near a tick boundary, rAF jitter alternates 0- and 2-tick frames.
- The PS2 runs one update per vblank pad sample and draws that update. It does not interpolate.
- Not confirmed from the code: how many vblanks the PS2's draw (game vt+0x3C after the updates) takes to reach the screen, that is, its
  GS kick and display-buffer swap.

**pv tickLock (web/tick-lock.js):**
- Offline, a frame of 0.75..1.25 ticks runs exactly one tick and draws it (alpha 1).
- The real-time debt is kept. Past 4 ticks of debt, one frame runs 2 ticks (or 0). A 59.94 Hz display gets one extra tick about every
  70 s.
- Any other frame (120 / 144 Hz, a missed vsync, a stall) takes the normal pacing.
- Online races (mpGame.pace) and the replay clock are untouched.
- Checks:
  - web/test-tick-lock.mjs: 60 Hz, 59.94 / 60.06 Hz drift, unlocked frames.
  - Chrome, Snow Jam Single Event with the neutral pad: the ?simtrace hashes are identical on and off for all 1406 ticks.
- WebKit not checked.
- Expected gain: the interpolation's 0..16.7 ms (mean ~8 ms), and no 0/2-tick judder at 60 Hz.

# Simulation worker: scrapped (2026-09-26)

Pieces 2 and 3 moved the race's simulation into a module worker. A run started on the page as usual. The core's memory
and the race's JS state were then handed to the worker, which recorded every core read the page makes and sent it back
each frame, pipelined one frame behind. It was exact: traces identical to the main thread in Chrome, Firefox and iOS
WebKit, including a worker killed mid-race and replayed. It was never on by default (`?simworker=1`, off on every
quality tier).

**Measured** (Snow Jam, 5 computer riders; the Mac was under load):

| | Main thread | Worker |
| --- | --- | --- |
| Desktop Chrome / Firefox / WebKit | 60 fps | 60 fps; 1-3 ms less main-thread work per frame |
| Chrome phone model (390x844 DPR 3, 4x throttle, worker slowed 4x) | 18.1-20.3 fps, p95 67-75 ms | 25.2-26.2 fps (**1.37x**), p95 42-50 ms |
| same at 2x | 26-27 fps | 30 fps (the low tier's 30 fps gate): 1.12x |
| Renderer process memory | 620 MB | 787-792 MB (**+167 MB**: the worker's copy of the core) |
| Input latency | | **+1 frame** (17 ms at 60 fps, 33 ms at 30 fps) |
| Run start | | hand-off 194 ms at 4x, of which the page was blocked 57 ms |

**Why it was removed.** The user decided: "game already runs well enough on a phone. we can't increase memory and input
delay for marginal gains". A 1.37x gain on a throttled model did not justify 167 MB more memory, a frame of input delay
and about 1,450 dense lines (110 KB) of hand-off, record and replay code. The code is in the GitHub mirror's history
(owattenmaker/ssx-web a1ea0f5) if the question ever comes back, for example with a SharedArrayBuffer core that needs no
second copy.

**What is left of it.** Only the pieces the rest of the game uses:
- the stale-safe workers and the build handshake (above), without the core-file check that only the simulation worker
  used;
- the game tick (above);
- `web/ai-race.js` and `web/ai-racers.js` are back to their code before the worker: the race state is inline again, and
  there is no attach mode and no state export.
