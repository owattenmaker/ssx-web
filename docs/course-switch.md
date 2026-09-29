# Course changes in the page (2026-09-25)

Switching course, event mode or peak never reloads the page. Single Event course picks, the Conquer the Mountain world
(free ride, transport into a race, peak runs), an online lobby's course and Back / Forward all run in the page:
the original load screen shows, the old course is released, the new one is built, and the History API keeps the URL a
reload would use. The quality tier applies at run time as well.

Code: `web/main.js` (the "Course lifecycle and in-app navigation" block: `navigateCourse`, `switchCourse`,
`unloadCourse`, `loadCourse`, `onPopState`, `applyAntialias`), `web/net/mp-game.js` (lobby course), `web/loading-screen.js`
(`cancel`), `web/snow-composite.js` (`pruneEncodedEffects`), `web/fe-preview.js` (`releasePreviewCores`),
`web/lineup.js` (`settleRaced`), `web/rider-shadow.js` (dispose), `web/touch-controls.js` / `web/quality.js` (tier).

## Page vs course

| Page (made once in `init`) | Course (`loadCourse`, released by `unloadCourse`) |
| --- | --- |
| renderer, scene / camera objects, UI (menus, HUD, load screen), audio engine, input (keyboard, pad, touch), cutscene player, online session (socket, lobby seat), render loop, compiled wasm module | wasm core instance, world and sky, set pieces, terrain refinement (worker), rider model + animation core state, computer riders (rider contexts of the core) and their FX / icons / rival beam, streamed Peak 1 world (free-ride, peak-world worker, peak set pieces, Big Challenges), post passes (fog, sun flare, light glow, glare, screen tint, rider shadows), board trail / snow / wake / boost, environment + lighting, audio world (`gameAudio.leaveWorld`) |

Each course gets a **fresh core instance** (`newCore`: `WebAssembly.instantiate` from the module compiled once,
`compileCore`); every `init_*` runs on a pristine instance exactly as on a first load, so the simulation is unchanged.
The old instance is released with the objects holding it: after a GC the `WeakRef`s in `window.__coreRefs` are empty
for every earlier course (checked below). Things that held an old core and were fixed: `FrontEndPreview.core`
(cutscene actors, Select Character) and `CharacterSelect.core` (`releasePreviewCores`), `game-audio` `state.humanCore`
(cleared in `leaveWorld`), `lineup.js` `pendingRace` (the raced event, `settleRaced`), `globalThis.ssxEffects.core`
(QA handle, deleted).

The rider model is rebuilt like a first load (RIDER_SAM on the course's `initial.json`, then the chosen rider in the
event load's `ensureRider`), so every course starts from the same state as a page opened on it.

## Switch sequence

1. A callback (`ui.cb.course` / `freeRide` / `peakRun`, the lobby's course, popstate) calls
   `navigateCourse(url, {history, after, screen})`. The callbacks keep their contract: they return `false` ("the page is
   changing course"); `ui.courseReady` is a Promise that resolves when the switch is done. Requests are queued and
   coalesced: a newer one replaces a queued one; a switch in progress finishes, then the latest runs.
2. History: `push` records the screen the player leaves from in the current entry (`history.state.screen`) and pushes
   `{ssx3:1, online}` with the same query as the old reload (`course`, `rider`, `base`, `autostart=1`, `peakCourse` /
   `peakMode`; online: `course`, `rider`, `base`, `online=1`, `lobby`). Online lobby changes after the first one
   replace the entry (the lobby, not the player, moved).
3. `switchCourse`: the run stops (`stopRun`: audio `leaveWorld`, cutscene stop, rumble, input), the frame loop goes
   idle (`live=false`: `idleFrame` draws the UI and follows the screen audio, nothing touches the world), the load
   screen opens (`ui.loading.cancel(); ui.loading.open()`: its own animation frames; a CTM ride-in's world mode carries
   over), `unloadCourse`, `loadCourse`, then the boot rider state.
4. After: `autostart` = as a page loaded with `?autostart=1` (`careerUI.resume()` for a pending career / world round,
   else `ui.loadEvent` + `startRun`; the load screen then runs the warm-up, pipelines, audio prep and intro as on a
   first load); `lobby` = back to the lobby screen (a host start that arrived during the load keeps the load screen
   for its own event load: `mpGame.session.starting`); `menu` = Back / Forward.

`unloadCourse` releases, in order: Big Challenges, the streamed world (`freeRide.stop`: peak worker, locations, set
pieces, core streaming), the raced event's lineup draws, the computer riders' renderer, rival beam, terrain refinement
worker + overlays, the post passes (`dispose`), the extra area skies, then every scene object the course added
(`courseRoots`, recorded at the end of `loadCourse`), the rider, hidden world meshes and the sky scene through
`disposeRoots`: each object's `dispose` event (three frees its render objects: bindings, uniform buffers), then
materials, geometries, skeletons and textures (map slots, TSL texture nodes, `userData.warmTextures`, the world's
texture set / atlas; render-target textures stay with their owners). The disposal yields every ~8 ms so the load
screen keeps animating. Encoded-pass effects of the old scene are pruned (`pruneEncodedEffects`).

## Back / Forward

`onPopState` leaves what runs (online race: DNF + leave; online lobby when the entry is not an online one; a career
session: pending round dropped, career saved, `careerMode` off; the run), then loads the entry's course if it differs
and lands on a menu, never mid-event: the screen recorded when the player left that entry (title, main, character,
setup, details; any Single Event selector screen -> Select Peak via `ui.set('event')`), else Select Peak for an event
entry (`?course=<event>&autostart=1`), else the main menu. An online entry (`&online=1` / `&lobby=`) re-enters Online.
The rider in the entry's URL is selected again.

## Online

`net/mp-game.js`: a lobby update with another course calls `switchCourse(url)` (main.js `navigateCourse(url,
{after:'lobby'})`) after `leaveRace()`; the WebSocket and the lobby seat stay (no reconnect / resume). `pendingCourse()`
is the course being loaded, so repeated lobby updates during the load do not queue it again; a `start` waits for
`courseReady()`. Checked with two Chrome clients on 5174/8787: host cycles ARA1 -> BRA2 -> BHP1, both clients switch in
~2.3 s, same client ids and slots, one `/mp` socket each for the whole session, then a race on BHP1 (both racing,
packets flowing).

## Quality tier

`setQuality({tier})` (touch settings panel) applies render scale, upscale, fps and frame-time cap at once (as before),
and `applyAntialias` sets `renderer._samples` (4 / 0; 0 in WebGPU compatibility mode): the canvas and the fog-renderer
pass targets follow `renderer.samples` on the next frame. If the course was already warmed, the warm-up runs again at
once so the pipelines for the new sample count are built then, not on later race frames (it draws the scene slices
behind the pause / settings panel for ~1.5 s). No reload, no renderer re-create.

## Measurements

Chrome for Testing (WebGPU, 900x660, dev server, other agents' tests running), Single Event from the menus, Metro-City
and Snow Jam alternating, 6 switches (`scratchpad/nav/flow5.mjs`, memory after `HeapProfiler.collectGarbage`, 4 s into
each race):

| | per switch |
| --- | --- |
| unload | 9-51 ms (BRA2: 4,238 objects, 4,151 geometries, 371 materials, 381 textures released) |
| course load | 1.9-2.6 s (one 3.4 s) |
| warm-up (under the load screen) | 1.3-1.6 s |
| click -> objectives card | 14.7-14.9 s (7 s load-screen minimum + intro cutscene; 7.7-7.8 s with `?cutscenes=0`) |
| long tasks during a switch | 5-6, longest 170-274 ms (a first load of the same event: 7, longest 177-182 ms) |

GPU objects at the same point of each race (`renderer.info.memory`, `?cutscenes=0`): Metro-City geometries
2745 / 2746 / 2747, textures 490 / 492 / 494, uniform buffers 5430 / 5434 / 5437, render objects 4129 / 4131 / 4133;
Snow Jam 3484 / 3485 / 3485, 497 / 499 / 499, 8457 / 8460 / 8460, 6549 / 6551 / 6551: flat. With cutscenes on, the
cutscene actor pool (up to 8 idle FE preview models kept for the next event, `cutscenes.js trimPool`) adds its models
until it is full. Old cores: all released after GC. JS heap after GC at the same point: Metro-City 174 / 189 / 186 MB,
Snow Jam 201 / 207 / 200 MB (flat). Before the last two fixes it grew ~25 MB per event: `game-audio.js` kept every song
file it had played in its byte cache (a page reload per course used to drop it; now `leaveWorld` evicts `b:music/`).

Peak 1 (free ride <-> Peak 1 Race, 3 cycles, `scratchpad/nav/peakloop.mjs`, `?cutscenes=0`): Peak 1 Race geometries
3711 / 3711 / 3711, textures 282 / 284 / 286; free ride 1581 / 1582, 359 / 362. Found on the way: `free-ride.js stop()`
disposed the resident locations with `disposeGroup` only (their node-material textures and light atlas stayed, ~330
textures per world); it now uses `releaseLocation` like the streaming release, and `unloadCourse` disposes every object
the course had before the modules detach parts of it (the peak set pieces' particle / halo meshes and textures).

Safari 26 (safaridriver, WebGPU, trusted pointer clicks so the pushed entries carry user activation; WebKit skips
entries pushed without it on Back), 5 switches (`scratchpad/nav/safari-nav.mjs`): unload 16-54 ms, load 1.86-1.99 s,
warm-up 4.3-5.7 s, click -> objectives 16.1-17.3 s, load-screen frames p50 17 ms, p95 43-55 ms (longest 338-578 ms);
GPU objects as in Chrome (Metro-City geometries 2733 / 2862 / 2884 with the cutscene pool filling); WebContent
`phys_footprint` at the menu after each event 2504 / 2525 / 2061 / 2238 / 2014 MB (1214 MB at boot); the old cores'
WeakRefs clear as JSC collects. Free ride PEAK1: load 2.9 s; Peak 1 Race from it: 2.8 s to the objectives card (the
race start after Continue checked in Chrome; Safari runs need an unlocked screen: a hidden page gets no animation frames
and the load waits). Online with a Chrome host and a
Safari guest: ARA1 -> BRA2 2.6 s, -> BHP1 0.6 s, same client id, no new WebSocket, then a race on BHP1.

Audio failures are contained (`main.js audioSafe`): a song whose stream uses codec 4 (microtalk) made `pathfinder.js`
throw on every tick, which stopped the frame (the race froze on the give-up / results path). The game now goes on and
logs it once; the decoder gap itself is the audio module's.

## Gaps

- Save import (Options > Save/Load, `web/fe-saveload.js`) still reloads the page: a dozen modules read their part of
  the save only at start-up (career, wardrobe, options, selection, audio, touch, quality, ...); an in-page re-read would
  need each of them to reload its state.
- Back / Forward always land on a menu (never resume an event mid-way), and leave a career session to the main menu.
- `peakRun` / `freeRide` URLs: `selectFreeRide` now drops a stale `peakMode` (a peak-run URL followed by free ride kept
  it before, so the free ride would have loaded the peak run's start).
