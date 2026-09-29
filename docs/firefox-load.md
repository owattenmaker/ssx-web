# Firefox load: pipelines, the load screen and the intro (2026-09-25)

Before these changes, Firefox took 46-84 s from the Snow Jam link to the race on a first visit (33-58 s on a repeat
visit). The load screen froze 17-75 times for more than 250 ms, and for up to 2.8 s at a time. The intro cutscene then
sat on one frozen (often black) frame for 11-23 s.

Now Firefox reaches the race in 16.5-25 s on a first visit and in 15.0-19 s on a repeat visit; Chrome takes 15.0-15.7 s.
Once the warm-up starts, the load screen animates with no frame over 0.3 s. The intro has no frozen frame: the slowest
is the one frame that switches from the load screen, 0.25-0.77 s, and the baseline had that frame too. Pipelines after a
Snow Jam load fell from 264-278 to 62-63. Chrome renders the same pixels and is not slower. Firefox renders the same
pixels except 35 HUD pixels, which differ by 1/255.

## What Firefox does differently

Firefox's WebGPU (wgpu -> naga -> Metal) creates every render pipeline serially in its GPU process. It translates the
WGSL to MSL and Metal compiles that, about 100-150 ms per pipeline when the macOS Metal shader cache has not seen that
MSL before. The cache is keyed by the MSL text, so identical WGSL compiles quickly on the next visit. Three things
turned this into a minute-long load:

1. **Too many pipelines.** three.js r186 builds one shader module per distinct WGSL text and one pipeline per shader
   pair and render state. A Snow Jam load had 264-278 pipelines and 241-253 vertex shader modules, but only 56 distinct
   shaders:
   - An unnamed uniform or storage buffer is named after its node id (`NodeBuffer_<id>`, `WGSLNodeBuilder.getUniformFromNode`).
     So every skinned mesh (`skeleton.boneMatrices`) and every per-material `uniformArray` had its own shader text.
     The ids also change between visits, so the Metal cache never hit. This alone added 185 modules and pipelines.
   - The render pipeline key includes the ids of the geometry's morph attributes (`RenderObject.getGeometryCacheKey`),
     so every morphing FE-preview part (NIS heads, hands, board flex) got its own pipeline even when the shader was the
     same.
   - Some constants were written into the shader text:
     - each rider's skin palette row offset (`worldAt(groupCount)`, rider-skinning.js), used by the computer riders and
       every rider shadow;
     - each snow emitter's instance capacity (`array<mat4x4<f32>, N>`);
     - each FE part's morph texture width, which equals its vertex count.
2. **The cutscene compiled for the wrong target.** The intro prep compiled the actors with `renderer.compileAsync(group)`,
   which targets the canvas. The world pass draws into its own target (rgba16float, depth24plus-stencil8, 4x MSAA), so
   the first intro frame built all the actor pipelines again: 24-57 pipelines at once, which was the frozen frame. The
   sky compile had the same problem and built 2 pipelines nothing used.
3. **Waiting on the GPU process.** Three things waited while the GPU process was compiling:
   - The UI's offscreen sprite canvases (glyph tints, LUI sprite caches, HUD atlases) were accelerated 2D canvases. In
     Firefox, every `drawImage` from one of them into the software UI canvas waits for a readback from the GPU process:
     0.1-0.7 s per load screen frame.
   - The warm-up draws its slices into the WebGPU canvas under the load screen, and a presented WebGPU frame holds the
     compositor until the GPU process gets to it. That stopped `requestAnimationFrame`, and with it the load screen.
   - The warm-up finished as soon as its frames were submitted, so the compiles were still queued when the intro started.

## Changes

| file | change |
| --- | --- |
| `web/shader-keys.js` (new; main.js imports it right after three) | Buffer names become their index in the shader being built (`NodeBuffer<k>`), so identical graphs give identical WGSL, and the same WGSL on every visit. The WebGPU pipeline key keeps the morph target count but drops the attribute ids. `?shaderKeys=0` turns both off. |
| `web/rider-skinning.js` | The previous-palette row is a uniform (`previousRows`), not a literal, so the human rider, the computer riders and all rider shadows share their shaders. |
| `web/snow-renderer.js` | All ten emitters size their instance matrix array to the largest capacity. `userData.capacity` keeps the authored bound check. |
| `web/fe-preview.js` | Morphing parts are padded to a multiple of 256 vertices (`padMorphPart`). The added vertices are zero and no index uses them. Every rider's head, hands and board then share a few shaders, which also helps Select Character, where each new rider used to add pipelines. |
| `web/fog-renderer.js` | `compileObject(object)` compiles for the world pass's target and MRT, including hidden sub-parts such as the board and the PDA. `main.js` passes it to the cutscene host as `compile`. The sky gets its own compile only when there is no fog renderer; otherwise the first warm frame builds it. |
| `web/main.js`, `web/ui.js` | After its frames, the warm-up waits for `gpuIdle()` (`queue.onSubmittedWorkDone`, capped at 30 s; mark `warm:gpu`), and so does the intro prep, so the intro never starts behind queued compiles. The intro prep starts when the warm-up's frames are done (`ui.warmFramesDone`), so its downloads and compiles overlap the warm-up's GPU wait. |
| `web/style.css`, `web/loading-screen.js` | The race canvas is `visibility:hidden` while the load screen covers it, so the warm-up frames drawn there never hold the compositor. World mode (the CTM in-world load, `data-load-world="1"`) keeps it visible. |
| `web/sprite-canvas.js` (new) + `ui.js` and 7 HUD/LUI modules | The offscreen sprite canvases use `SPRITE_2D`: software (`willReadFrequently`) in Firefox, the browser default elsewhere. Chrome's accelerated sprites give slightly different edge pixels than software ones (up to 57/255 on the HUD), so Chrome and Safari keep theirs. `?uicanvas=gpu` keeps the default everywhere. |
| `web/diagnostics.js` | New events: `stall` for a frame gap over 1 s while the page is visible, `gpu-stall` when the GPU queue is more than 1 s behind (probed every 2 s), and the `warm:*` marks. A Firefox run with `?diag=1` reported the `gpu-stall` events and marks of a cold load. |

Pipelines after a Snow Jam load (Firefox and Chrome): **264-278 -> 62-63**. Vertex modules: 241-253 -> 35-36 (fragment
modules stay at 31). The intro now builds 1 pipeline instead of 24-57. Snow Jam with `&cutscenes=0&ai=0`: 80 -> 53.
The pipelines that remain are the genuinely different material and state combinations.

## Measurements

Setup: Snow Jam, Zoe, 5 computer riders, `?course=ARA1&rider=zoe&autostart=1`, the dev server, a 1280x720 viewport at
device pixel ratio 1, macOS, Firefox 156.0.1 and Chrome for Testing (WebGPU). Times are from navigation. **Before** is
the same tree with only this work reverted, served on a second port and rebuilt from the live tree before each batch
(other agents were changing the tree meanwhile). **Cold** salts every WGSL identifier (`SALT=1`) so Metal sees new MSL,
as on a first visit; **repeat** is a second visit with Metal's cache warm. There were three batches of 2 runs per cell
(batch 3 includes the texture archive work). Other agents shared the machine: load average 15-27, highest in batch 3.

Firefox:

| | intro starts | race starts | warm-up (incl. GPU wait) | load screen after warm-up start: frames > 250 ms (longest) | longest intro frame |
| --- | --- | --- | --- | --- | --- |
| cold, before | 28.8-54.6 s | 46.4-84.2 s | 15.0-29.4 s | 27-71 (1.4-2.8 s) | 11.0-22.8 s |
| cold, after | 9.6-17.5 s | 16.5-25.3 s | 5.6-11.8 s (3-7 s of it GPU wait) | 0-1 (0.29 s) | 0.27-0.77 s |
| repeat, before | 14.8-30.3 s | 33.0-57.9 s | 3.6-13.7 s | 16-32 (0.8-1.4 s) | 10.7-21.4 s |
| repeat, after | 7.8-12.0 s | 15.0-19.0 s | 2.0-4.3 s | 0-1 (0.29 s) | 0.25-0.64 s |

Per batch, race start (before -> after): cold 60.4/64.0 -> 20.6/21.8 s, 46.4/47.8 -> 16.5/17.8 s, 69.9/84.2 -> 25.3/24.0 s;
repeat 53.5/48.3 -> 18.9/16.8 s, 40.5/33.0 -> 15.0/15.2 s, 56.4/57.9 -> 19.0/17.8 s.

The frames over 250 ms that remain on the load screen come at 1.4-5.5 s, during page setup, before the warm-up starts;
the baseline has them too. The longest intro frame is the one that switches from the load screen to the cutscene; the
baseline has it as well, at 0.4-0.5 s, before its 11-23 s freeze. On a cold Firefox load the GPU queue still runs 3-7 s
behind during the warm-up (the `gpu-stall` diagnostics event). Nothing shows it: the canvas is hidden, the load screen
keeps animating and waits for `gpuIdle`.

Chrome:

| | intro starts | race starts | warm-up (start -> end) | longest load screen frame | pipelines |
| --- | --- | --- | --- | --- | --- |
| before | 8.1-8.7 s | 15.0-15.6 s | 1.6-2.1 s | 257-408 ms | 264-270 |
| after | 8.1-8.5 s | 15.0-15.7 s | 1.45-2.5 s (GPU wait 0.02-0.03 s) | 259-490 ms | 62-63 |

`?course=CRA3&autostart=1&cutscenes=0&ai=0` (6 runs before, 5 after): the warm-up took 1.07-1.65 s before and
1.08-1.56 s after, plus one run of each at 2.5 s under load. Both builds draw 105 frames during the warm-up. Chrome's
times are set by the 7 s load screen minimum and the intro, which is why they do not change. The Claude app's embedded
browser pane reported an 85 s warm-up at about 0.7 s per frame on this URL. That pane was hidden, which throttles
`requestAnimationFrame`, and about 105-120 throttled frames account for the 85 s. The GPU work is the same.

**Select Character in Firefox** (`?qa=1`, open the screen, cycle 4 riders; `ffmenu2.mjs`): before, 30 frames of
325-433 ms in 18 s, as the roster prefetch built each rider's pipelines (85 pipelines). After, 0-4 frames over 200 ms
(the longest 438 ms) and 9-10 pipelines. No errors either way.

**WebGL2 in Firefox** (`?backend=webgl`): the race at 26.4 / 29.3 s, with a 5.9 / 8.5 s frozen frame at the intro start
(about 250 GL programs; `shader-keys.js` patches only the WGSL builder). WebGPU stays Firefox's backend.

**Safari:** not measured. With Safari's "Allow Remote Automation" on, safaridriver still could not create a session
(6 tries from 15:40 to 16:30, each ending after 30 s with "The session timed out while connecting to a Safari
instance"; Safari had been restarted at 15:38). Safari may be waiting on a prompt, or it may need `safaridriver --enable`
run once. None of the changes are
specific to Firefox except `sprite-canvas.js`: fewer pipelines, the hidden canvas and the idle wait apply to Safari too.
Safari's earlier warm-up of 4.3-5.7 s with 338-578 ms frames (course-switch.md) should get shorter; this still needs a
measurement.

## Rendering is unchanged

Deterministic A/B frames, baseline server against the changed one (`scratchpad/ffresume/ab.mjs` for Chrome,
`ffab.mjs` for Firefox). Each run uses the fixed lineup `&lineupSeed=0xB57109A9` and a seeded `Math.random`.
- **Intro:** `window.__cutscenes.freeze()` / `seek(150)`, UI hidden.
- **Race:** `ssxQA.start(); ssxQA.advance(420)`, with the HUD hidden, then shown.
- **UI text:** `ui.text()` drawn into an offscreen canvas.

Each build was run twice; runs of the same build match to the pixel.

| | intro (start hut, actors) | race (riders, shadows, snow, fog, glare) | HUD | UI text |
| --- | --- | --- | --- | --- |
| Chrome | 0 px differ (both start-hut variants, 96 and 89) | 0 px | 0 px | 0 px |
| Firefox | 0 px | 0 px | 35 px at 1/255 (place display) | 0 px |

Side-by-side images (baseline, after, amplified difference) are in `local/browser-validation/firefox-load/`.

Two things are drawn from `Math.random`: which start-hut variant plays (`chooseScript`) and the flag cloth phases. Any
change in how many objects three.js creates before those draws shifts the seeded sequence, so unpinned runs can show a
different variant or banner pose. That is a different random draw, not a rendering change, and in real play these
differ every session anyway. The A/B pins the variant with `__cutscenes.qaScripts = {4: 96}`. `&flags=0` gives equal
frames too.

The load screen differs in one place. Its UI canvas leaves its leftmost pixel column transparent (1 of 640 canvas
pixels, 2 screen pixels at 1280 wide), and the warm-up's world frames used to show through there. With the race canvas
hidden, that column now shows the load screen's black background.

`npm test`: every command passes except `test-slopestyle-bigair`, a Quick Play roster assertion that also fails on the
baseline. The GPU pages (fog, snow composite / flipbook / colour, rider skin / position / normal / lighting / material,
irradiance, glare) pass in Chrome.

## Other work in the tree

- **Texture archives** (`web/texture-archive.js`): a package's textures are decoded when the package loads, before the
  warm-up, with their final format, sampler and colour space, so the warm-up builds the pipelines the race uses. A
  texture decoded later, or swapped for one with a different format, filter, wrap or colour space, changes the material
  cache key and builds a new pipeline on the frame that draws it.
- **Two vite servers:** a second vite on a copy of `web/` with `node_modules` symlinked shares `node_modules/.vite` with
  the dev server unless it sets its own `cacheDir`. The baseline server here does
  (`scratchpad/ffresume/mkbase.sh`).

## Tools (this session's scratchpad, `ffresume/`, not in the repo)

- `mkbase.sh`: builds the baseline, which is the live `web/` with this work reverted (`patches/*.patch` + `revert.py`).
  It is served on :5193 with its own vite cache and no file watching.
- `fftl.mjs`: Firefox over WebDriver BiDi. Records the screen timeline, the `warm:*` marks, frame gaps per screen, the GPU
  queue latency and the pipeline and module counts. `SALT=1` gives a cold Metal cache, `PORT=` picks the server and
  `FFPORT=` the Firefox instance. `ffmatrix.sh` runs before/after, cold/repeat.
- `chrome-tl.mjs` / `chrome-tlq.mjs` (`QS=` query): Chrome timeline and frame statistics per screen.
- `ab.mjs` / `ffab.mjs` + `diff.py`: the deterministic A/B frames (`PIN='{"scripts":{"4":96},"alts":{}}'` pins the
  intro).
- Earlier tools from the first pass (`count.mjs`, `ffprof.mjs`, `ana.js`, `fam.js`) list every pipeline created,
  with its screen, object, material and WGSL, and time every WebGPU/2D call on the main thread.
- To drive Firefox, start an instance with its own profile:
  `open -na /Applications/Firefox.app --args -profile <dir> --remote-debugging-port 9336 -new-instance about:blank`.
  Put `remote.active-protocols` = 1 in `user.js`, and `layout.css.devPixelsPerPx` = "1.0" for a 1x canvas, then
  connect puppeteer-core over `webDriverBiDi`. Firefox allows one BiDi session at a time: a script that dies without
  disconnecting leaves it taken until that Firefox restarts.

## Open

- About 60 pipelines remain, one per genuinely different material or state: terrain blend variants, and double-sided
  transparent materials drawn as a back pass and a front pass. A cold Firefox load still spends 3-7 s compiling them
  behind the load screen.
- The frame that switches to the `cutscene` screen before the script is active draws the (hidden) scene straight to the
  canvas (`main.js frame`, pre-existing). This builds the output-transform pipeline then, and that frame is the
  0.25-0.77 s one. Building that pipeline during the warm-up would shorten it.
- The WebGL fallback (`GLSLNodeBuilder`) still names buffers by node id.
- Safari timings (see above).
