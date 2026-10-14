# Online multiplayer (browser port)

Online races of up to six human racers, each on their own machine, with the original's race world: the same grid
gates, rider-vs-rider contacts, knockdowns and punches, the original place display, board tracks / spray / boost of
every rider, a shared world (pickups, crashbags, sections), and the original Single Event Results with every racer.

Big sessions (20-100 players, a Trackmania-style time attack with ghosts): design in [big-multiplayer.md](big-multiplayer.md), not built.

## Gate-opening freeze hotfix (2026-10-14)

Field reports at 04:53 UTC (ARA1, both clients of one lobby) and 05:05 UTC (ERA5) show repeated WebAssembly
exceptions and later out-of-bounds accesses. Two local Chrome pages reproduced the first exception on gate opening:
`std::runtime_error: Native pair body pose is unavailable`; later attempts throw `Race tick already open`.

M3 configured the offline rider manager while preparing the course. Online installs `rider_host(2)` and its own
`pairHost`, but `rider_manager_native()` checked only whether an offline manager remained configured. Thus the
0x107888 dispatch read dormant computer riders instead of the online human and streamed ghosts. A missing body
pose aborts the tick; repeatedly re-entering that unfinished tick accounts for the subsequent errors.

The original 0x107888 reads the participating actors' posed AA0 volumes in the second motion phase (121750),
with the pair records refreshed by 0x10F560 every sixth tick; the order and collision arithmetic are unchanged.
Network ownership is port-only: flag 8 selects the core manager, flag 2 alone selects the page's network host.
`web/rider_manager.cpp` now requires flag 8 in `rider_manager_native()` for every pair callback. An offline
`ai-racers.js` start/resume always restores its own host flags after the online host has been disposed.

Regression coverage:

- `test-mp-pairs.mjs` now leaves an unused computer-rider manager configured before the online takeover. It fails
  on shipped core11 with the same first exception, and passes on the fix (contacts, punches, reactions, rankings).
- `test-ai-racers.mjs` borrows and disposes the online host before starting its offline race. Without re-arming
  the host, it fails in the JS controller callback; with the fix all five opponents run their normal 1500 ticks.
- `test-mp-page.mjs`: two browser pages connect through a local server, start, stream at least 720 ticks, perform
  pair checks, ride over 10 m beyond the gates, return to the lobby and start again. Default Chrome + Chrome;
  `--webkit` uses Chrome + WebKit. `MP_COURSE=ERA5` checks the second reported course; `MP_SHOTS=directory` saves frames.
- Five targeted ARMSX2 capture regressions pass: event-start, event-race-ai, ai-idle, event-race-ai-pairs,
  bc/bc-race-tuck2. The core-rider-manager differential also passes, including relationships and event restarts.

Chrome + Chrome passes two starts on both ARA1 and ERA5. Chrome + WebKit passes two ARA1 starts, including
contacts/reactions, with screenshots checked. The WebKit driver reports `visibilityState=hidden`, so native rAF
never runs; `MP_WK_TIMER=1 node test-mp-page.mjs --webkit` explicitly pumps the renderer's queued animation
callbacks from Node at real wall time. This verifies WebKit simulation and rendering, not visible-window frame
scheduling. The ordinary `--webkit` check needs an available display and is not in the unattended default suite.

Scratch core: `local/mp-freeze/core`; verified local runtime core SHA-256 begins `1e117b0c`. Deployed on 2026-10-14 after the full PS2 capture gate passed, with unchanged game data. Public bundle
`main-D9E5OnW8.js` serves the verified core; `/mp/status` is healthy. Existing tabs must reload.
Browser and deployment logs and screenshots: `local/mp-freeze/` (private).

## Run it

Development / LAN:

```
cd web && npm run online            # PORT=5174 npm run online for another page port
```

This starts the server (`web/server/mp-server.mjs`, port 8787) and the game's dev server for the whole network
(`vite --host 0.0.0.0`). The server prints the `http://<LAN address>:5173/` link for friends. The page reaches the
server through the dev server's `/mp` WebSocket proxy (`web/vite.config.js`), so one address is all anyone needs.

One process, production build (what a host runs):

```
cd web && npm run online:serve      # = vite build --config server/vite.online.config.js
                                    #   + node server/mp-server.mjs --static dist-online,public
```

`web/dist-online` holds only the code (the build does not copy `web/public`); the server serves it first and the
extracted game data from `web/public` second, plus `/mp` (WebSocket) and `/mp/status` (JSON), on one port. See
**Hosting** below.

## Flow

1. Main menu → **Online** → pick a rider → **Online Lobbies** (`web/mp-ui.js`).
2. **Create Lobby** (current course). **Copy Invite Link** copies `…/?lobby=<ID>`; opening it goes straight into the
   lobby. The lobby list shows every lobby; a racing lobby can be joined and seats you for its next race.
3. In the lobby: Ready, the host's **Course** (everyone's page loads the course in place, 2026-09-25: the socket and the
   seat stay, the URL becomes `?course=<code>&rider=..&online=1&lobby=<ID>`; docs/course-switch.md "Online"; with
   pv `mpCourseSelect` the row opens the Single Event selector instead and nothing loads until the start, see
   **Lobby course** below), **Chat** (optional, rate limited), **Start Race**.
4. Start: every racer loads the event behind the original loading screen (the remote racers' models, their effect
   puppets and lighting included), reports loaded, and the server sends one GO time (server clock). The countdown is
   held until that instant, so all countdowns run together (backcountry events have none: a rolling start at GO). The
   packets' race tick counts from GO either way; the race clock starts 180 ticks later after a countdown, at once on a
   rolling start. Racer k starts on the original gate of countdown slot k
   (`npc-riders.json` seeds, core `human_grid_seed`). No computer riders in online races.
5. The race: each client simulates only its own rider (bit-exact core, unchanged) and streams it; the others are
   drawn from their streams, collide with it through the original pair system, and rank with it.
6. A racer who finishes sees the original results 408 ticks after the finish (288 after a TIME'S UP), with the
   finished racers' times and 0x122D78 estimates for racers still on course, updating as they finish. The server's
   final order replaces the estimates once every racer finished / is DNF, or 90 s after the first finisher
   (then the rest are DNF), or 15 min after GO. **Back to Lobby** returns to the lobby for the next race.

With pv `mpPreRace` the start runs a Single Event's pre-race (the intro with every racer, the card, Continue) before the GO;
see **Pre-race** below.

## Lobby course (pv mpCourseSelect, 2026-10-14)

The field report: "clicking Course loads everything and puts you back in the lobby; you can't select a different course."
Cause (by design since 2026-09-25, not a state bug): the row called `cycleCourse()`, which stepped to the next entry of
`ui.courses`, and every page's `client.on('lobby')` then loaded that course in place (`switchCourse`, load screen, back to
`mp-lobby`). One click was one full load on every page. Also, the backcountry's two entries (Happiness, Happiness Jam: one code)
kept the cycle on Happiness for good, so the courses after it were unreachable. `cycleCourse` now steps once per course code
(this fix is live without the switch).

With the switch on, the host's Course row opens the original Single Event selector (`web/fe-event-select.js` `openLobby`: Select
Peak, Select Mode, Select Event; docs/characters.md "Single Event track selector"). The cursor starts on the lobby's course. Cross
on an event names the lobby's course (`{t:'course'}`, the others see it in the lobby), and Back on Select Peak returns to the lobby.
Rows the online race cannot hold are listed but not playable: the peak runs, as in Single Event, and a backcountry's Jam row (its
Rival Time row picks the same course). The player's own remembered Single Event choice is left alone. Nothing loads in the lobby.
The race's course (`start.race.course`) loads at the start, under the load screen the event load then takes over (`mp-game.js`
start, `session.starting`). Mixed pages work both ways: a page without the switch still loads the lobby's course when the lobby
changes.

The online lobby itself has no PS2 counterpart (the disc's online play is two-player challenges from EA's lobby screens, see
below), so only the course list is the game's.

## Pre-race (pv mpPreRace, 2026-10-14)

Field report: "no pre-flight screen once the course loads, it just drops you into the start". Before this, `cb.introPrepare` /
`cb.intro` skipped the intro in online races, and the start ran `startRun` right after the load; `holding()` held the countdown
until the GO.

**PS2 rules (recompiled code / ssx3-decomp asm; [N] = from notes only):**

| Step | Address | Rule |
|---|---|---|
| load | 0x22F544 (cGame_load) | pushes WS10, ticks the world states 0x230F40 under the load screen until WS10 is ready; load screen enter 0x245418: 99QPEvent, or 102MPMatch when the game type 0x535C11 == 2 |
| intro lists | WS10 pre-enter 0x234F40 -> 0x27AAF8(nis, 0) | table *(0x445488 + 4 x game type): CTM 0x481E48 [2,0][3,3][5,0]; Single Event and Multi Play 0x481E68 [4 start hut, flags 3 (skippable)][5 gate idle]; online flag 0x534B30 set: 0x481E80 [4,3] only (no idle) |
| intro wait | WS10 update 0x2357F8, WS1 update 0x2343B0 phase 3 | the list must reach its kind-5 (idle) step or stop; grid placement 0x1289F0; then WS2 |
| script choice | ScriptChoice 0x27B0C0 | masks, least played (nis+0x530), BXrand 0x3177F0 % n on the presentation stream 0x4FF018 |
| card | WS2 enter 0x236BB0 (context 1), 0x20A8F8(2): race -> overlay 8 '40race_pre', Multi Play freestyle -> 0x15 '50multifs_pre' | Cross: command 1 -> 2 -> WS2 +8 = 1 [N for the chain]; exit 0x236C88 -> 0x236CD8 stops list 0, race camera, race audio 0x2871B0 |
| online card | overlay 8 create 0x1FBBD8: comm+0x64 = 1200; comm tick 0x257750; card update 0x1FBCE0 | the card closes itself after 1200 ticks (20 s); a received message restarts the count (0x25801C) |
| waiting | 0x258CB0 | formats kT_OVRCMNWaitPlayer "Waiting for other player..." (0x0BA409E2) and sends it to the 'hud' screen as command 14 (0x1EC2A0: text at +0xA48, flag 0x40000; command 15 clears); cOVStateHUD1P_onRender2D 0x1EC810 draws it with 0x1F2DB0 in FEFONT (+0x428) at HUD descriptor 63: centred on (320, 270), scale 0.7, yellow, wrapped at 360. When the comm code calls 0x258CB0 is not traced |
| countdown | WS3 enter 0x234AD0, update 0x234C68; 0x113D38 C+0x1C = 180 | 180 ticks, 0x29C420 each 60, then StartgateOpen, GO, WS4 |

The disc's online mode (game type 2 + 0x534B30, EA dirtysock, FE screens 230online_main_menu, 231lobby_menu, 233QuickPlayPopup,
240challenge_popup...) is two-player head to head from challenges. No all-players-ready handshake before the countdown was
found (the comm packet handler 0x257E98 is not traced), so the shared ready is port-only.

**Port sequence** (`web/net/mp-game.js`, `web/mp-ui.js` `mp-card`, `web/server/mp-server.mjs` `maybePreRace`):

1. The host's page sends `{t:'start', preRace:true}`; the race carries `preRace: true`.
2. Each page loads under the load screen. Once its own load has settled it reports `loaded`, and the load screen holds
   (`ui.loadEvent` `hold`) until the server's intro instant. The server sends `{t:'intro', at}` (1.5 s ahead) once every racer
   has loaded, or after 45 s; the late racers are DNF 'late', as before.
3. The intro: the Single Event / Multi Play lists (0x481E68): the start hut (skippable with Cross), then the gate idle. The cast is
   the same on every page: race rider k = the racer of slot k, humans = slots 0 and 1, no computer riders. The script and its
   track alternatives are drawn from the race seed (`seededRandom`, no play counts), so every page plays the same script. This is
   port-only: the PS2's stream is per machine. The online table 0x481E80 (no idle) is not used, because what the PS2 shows under
   its card then is not traced. A course without intro lists (backcountry) has no intro, and its card lies over the ready state
   (`readyView`, as the Single Event rival card; it uses the racer's own online grid slot).
4. The card at the idle: overlay 8 '40race_pre' (`results-lui.js raceCard`), with the event, the Single Event round's objective,
   the racers in slot order (this page's in the human colour) and Continue. It holds the idle with context 1 and takes input from
   phase 5, as `ctm-objectives` does. The subtitle 'Online' (0x0012F325) is the port's choice. Continue, or the PS2 online card's
   own auto-continue after 20 s, sends `{t:'ready'}`. The card then closes, and "Waiting for other player..." stays over the idle
   as the PS2's HUD line (descriptor 63, `trick-hud.js text`).
5. The server sends `{t:'go', at}` (2.5 s ahead) once every racer is ready. A page 150 ms before the GO starts its run
   (`startRun`: the NIS stops, the race camera, as WS2's exit). `holding()` holds the countdown until the instant, so the 3-2-1
   runs together everywhere. A GO that arrives under the load screen starts the run when it closes.

**Rules for pages from before the switch:** a racer whose `hello` did not announce `preRace: true` counts as ready once it has
loaded (it rides the old start: gate, held countdown). A racer not ready 60 s after the intro instant (`MP_READY_TIMEOUT_MS`: a
hidden tab, a stuck page) no longer holds the GO back; its page starts from its card on the GO. Racers already DNF (left, late)
hold nothing back. A host without the switch never sends `preRace`, so the server keeps the old start (loaded -> GO); the
server half is inert until a host turns it on. A reconnect inside the pre-race gets `introAt` / `goAt` in its `welcome`.

**Checked:**
- `test-mp-prerace.mjs` (server, node clients): the intro waits for every load, no GO while a card is open, an old page is ready on
  its load, the ready timeout, a racer leaving on its card, a host without the switch (no intro).
- `PAGE_PV=mpPreRace,mpCourseSelect MP_PRERACE=1 node test-mp-page.mjs` (two pages, cutscenes on, two races each):
  - Chrome + Chrome on ARA1: the same script #89 `ra_sgb_var1` on both pages, intro start spread 7-11 ms, the game start within 2 ms.
    Player 1's card auto-continued after 22 s; both countdowns ran from the same GO.
  - Both pages bind the same racers in the start hut: actor 16 = Zoe (slot 0), 17 = Mac (slot 1), with models; 18..21 empty (no
    racer in slots 2..5). The gate idle #73 `ra_sgb_static` has no actors in its data (a camera and cuts only), so the hut under the
    card is empty, as in a Single Event.
  - Chrome + WebKit (`MP_WK_TIMER=1 ... --webkit`, `MP_PICK=BRA2`): spread 11-14 ms.
  - ABC1 (no intro: the card over the ready state, rolling start) in Chrome + Chrome.
- Screenshots: local/mp-prerace/shots-pre*/.

## Riders: any character, skin and outfit

- **Profile.** Each racer's lobby entry carries its character (`rider`), package, a cheat skin's base rider (`base`)
  and its worn outfit (`outfit`: web/wardrobe.js `outfitKey`, the committed item ids `w1:...`, `sam:<package>` or
  null for the default outfit), besides the pair inputs. A lobby course change keeps `&base=` in the URL.
- **Remote riders.** Every other client resolves the entry with `remoteOutfitRider(entry, key)`: a skin composes over
  its base rider (`character-roster.js composeCheat`), an outfit builds the same race package locally
  (`/assets/WARDROBE/<ID>/o<hash>/`, served by `wardrobeFile`). The drawn model, its skin palette layout, the effect
  puppet's settings and rig all use that package; the body scale comes with every packet. Two browsers: Zoe in an
  Equip Gear outfit and Brodi on Psymon, each drawn by the other in its own model.
- **Grid spot (web/net/grid-seed.js).** A rider's countdown state follows its slot, its body scale and its base
  rider's stance (docs/characters.md). An online racer in slot k starts from its own full slot-0 state for its real
  body scale (lineups.json `human_grid`, `human_template`, `human_base`: lineup.js `humanGridState`) with slot k's
  parts for that scale laid over it (grid `[k][scale]`: position and ground frame; `slot[k]`, `moment[k]`, `state[k]`),
  the same parts `assembleLineup` builds the computer riders from; `test-mp-grid.mjs` rebuilds the five anchor
  computer riders of Snow Jam and BRA2 this way bit for bit.
- **Scales no computer rider has** (Canhuck 0.7 -- a different float from Griff's --, Stretch 1.2, Bunny San /
  Churchill 1.3, North West Legend 1.5, Far East Myth 2.0) come from the PS2 too (`tools/export_grid_scales.py` ->
  `<course>/grid-scales.json`): derived countdowns from `characters/zoe/select.p2s` with one instruction of the race
  copy 0x23A668 changed in the state's RAM (`lw v1,0(a2)` at 0x23A700 -> `addiu v1,zero,<cheat>`), so every computer
  slot holds that skin (on the human's base, as a cheat computer rider always rides); the original menu path and
  load then run unmodified in ARMSX2. A control state (Hiro, 0.75) reproduces lineups.json's five spots exactly on
  both courses. States: `local/reference/pcsx2/characters/grid-scales[-BRA2]/`. The earlier line estimate was off by
  up to 2.1 cm (Snow Jam) / 7.5 cm (Metro-City) and 0.05 in the ground normal; only recorded spots are used now.
- `test-mp-grid.mjs`: every selectable rider and cheat skin on every base rider, in every slot, on both courses
  (2532 combinations) lands on a recorded PS2 spot with its base rider's stance, and 372 core starts (every rider
  with its own settings, six slots, two courses) sit exactly on their spot. Courses that get lineup data later
  (the other Peak 1 courses) need `export_grid_scales.py states/export --course <code>` (the script's Select Event
  step per course) before online races there use exact spots; until then they fall back to the anchor record.
- **Off-line again.** The online seed is set only in online races and removed when the race is left;
  `human_grid_seed('')` now restores the rider's own seed (the settings seed `init_animation` installed) instead of
  dropping to Zoe's compiled spot, and off-line runs never touch the seed (ai-race.js keeps setting the human's spot
  on other courses; the computer-rider lineup picker web/lineup.js is not used online).

## What travels (web/net/rider-packet.js)

State packet, 20 Hz, ~1.9 KB (was ~10 KB), little endian, the server prefixes the sender's slot:

| Part | Size | Used for |
|---|---|---|
| header: tick since GO, remaining distance (+0x4D0), finish ticks, body scale (`rider_skin_scale`), placements/rescues, physical position + velocity | 56 B | ranking, dead reckoning, teleports |
| world pose: per bone position + quaternion (`world_pose_bones`) | 26-29 × 28 B | the skin palette, **rebuilt bit for bit** |
| pair view (`pair_view`: AA0 body spheres, velocity, normals, motion/control, attack window, frames) | ~480 B | rider-vs-rider |
| lighting: environment irradiance bank, rim scalar, query bounds, rank point | 200 B | the rider's own original lighting |
| FX records: the rider-effect inputs of each of the 3 ticks (1 full + 2 delta-coded) | ~400 B | board track, spray, wake, boost, sparks |

Other packets: **attack** (kind 3, 24 B: victim, attacker, direction, impulse) and **world** (kind 4: shared world
entity changes and course triggers of the sender's core, `web/shared_world.inc`).

### Skin palette, bit for bit (web/net/pose-codec.js)

The core's `rider_skin_palette` is a pure function of the cached world pose, the authored body scale and the
package's bind matrices / integer-percent weights (310120 pose matrix, 3106CC × bind, 386BD0 weighted sum), in the
original toward-zero float arithmetic. `pose-codec.js` ports `engine/software_float.hpp` (binary32 toward zero from
binary64 residuals) and those three routines, so the receiver rebuilds the sender's palette exactly from the 7 floats
per bone. `test-remote-riders.mjs` replays a recorded run (tricks, crashes; Zoe and Psymon) and compares every word of
every tick's palette with the sender core's: all equal.

## Remote riders on screen (web/net/remote-riders.js)

- Every racer counts ticks from the shared GO instant; a packet's tick stamps the state after that tick.
- Pose: drawn 6 ticks (100 ms) behind, blended between the two packets around that tick (each packet's palette is
  exact; the blend is presentation only), never across a teleport (placement/rescue counters).
- Position: the drawn body is moved to where the rider is *now*: the newest packet's physical position advanced by its
  velocity to this client's tick (at most 20 ticks), the correction eased over 120 ms. So a remote rider is drawn
  where this client's contacts find it.
- Drawn by `web/opponent-riders.js` (the opponent path: source palette vertex node + original rider material) through a
  stand-in core.
- Lighting: `shade_external_rider_lighting` (web/rider_lighting_bridge.cpp) shades the remote rider in this client's
  light world with its own streamed irradiance bank, rim scalar, query bounds and rank point, and this client's
  camera, without touching the local rider's lighting state (the light query and shade keep no history).
- Effects (web/net/remote-fx.js, web/fx_puppet.inc): the original updates every rider's RFX container (track, snow
  spray / wake / breath / kicker / body snow, boost, sparks, fist sparkle). The sender's core records, once per FX
  pass, exactly the rider fields those updates read plus its particle and visual random words; a puppet core per
  remote racer on the receiver writes them back and runs the same `update_board_sparks / update_trail /
  update_snow / update_fist_sparkle / update_boost_fx`, and a set of the human's effect renderers draws it, moved by
  the same correction as the body. With the sender's pose the puppet's track, wake, boost, sparks and particles
  equal the sender's bit for bit (`test-mp-fx.mjs`); in a race the pose between packets is interpolated.
- No name tags: the original has none in races.

## Rider vs rider (web/net/pair-net.js, web/race_world.cpp)

In the original one world holds every rider: 0x10F560 ranks and refreshes the pair proximity records every sixth
tick, and each rider's 121750 runs the rider pairs 0x107888 against every other rider. Online, each client runs that
world in its own core for its own rider:

- Remote racers are kinematic **ghosts**: their streamed pair view, positions dead-reckoned to this tick.
- The local rider's 121750 dispatches 0x107888 for its own slot (`riderHost.pairs`, `rider_host(2)`). A contact with a
  ghost applies only the local rider's half (its separation share by weight, its impulse, its soft/crash reaction);
  the ghost's own client sees the mirror contact against our ghost and applies the other half. Together that is the
  original response.
- An **attack** hit on a ghost (0x107888 attack branch; `attackHit` callback) is sent to the victim; its client applies
  the same 107E70 response to its own rider at its next pair point (`race_world_pair_respond` → `respondToAttack`),
  so the victim's knockdown is decided with its own state.
- A ghost without live state (not received for 45 ticks, disconnected, DNF) leaves the pair world
  (`race_world_pair_disable`).
- Reactions draw from a **contact stream** (see below), not from the machine's own game RNG.
- **Lag compensation**: when this client resolves a contact against a ghost it also computes the ghost's half (its
  separation share and impulse, which the ghost's own client applies) and adds it to the ghost until that client's
  packets carry it (`remote-riders.js correct()`). Without it, a client keeps colliding with the stale, not yet
  pushed ghost for a whole latency and pushes its own rider again and again (see the measurements).
- `test-mp-pairs.mjs`: two clients over a simulated 100 ms link start overlapping, both resolve the contact from
  their side; one punches (R1/L1) the other, every attack reaches the victim and is applied; both rank the leader 1st.

### Contact randomness (web/net/pair-net.js contactStream)

The PS2 has one world RNG (0x317810 on six words at 0x4FF030): every rider's controller, reaction and effect draws
come from it in the order the frame runs them, so a reaction's random words depend on everything drawn before it that
frame, on every rider. Online there is no single frame: each machine runs only its rider, at its own moment. Sharing
the one sequence would need lockstep (every client waiting for every other's inputs each tick). Instead each contact
reaction draws from its own stream: the same generator seeded from (race seed from the server, contact tick, the two
slots, the reacting rider) through a murmur finaliser chain. So

- a reaction's draws are the same wherever it is computed: the victim's real response to an attack and the
  attacker's prediction of it draw identical words and choose the same reaction (`test-mp-pairs.mjs`: every
  predicted attack reaction equals the applied one);
- the same race seed and inputs give the same run (`test-mp-latency.mjs`);
- a rider's own game RNG is untouched by contacts (the core's RNG words are swapped for the stream around
  `pair_react` and restored), where in the original a contact shifts every later draw of that frame.

The words themselves therefore differ from what the PS2 would have drawn at that point of its frame; the
distribution (the same generator, the same request code 107E70 / 108388) is the original's.

### Latency (test-mp-latency.mjs)

Two riders racing side by side, three forced side contacts; a zero-latency reference runs both riders in one pair
world (the original 0x107888 with both halves) with the same contact streams. Distances are the riders' separation
5/15/30 ticks after each contact (reference 125/163/165, 297/425/389, 559/822/711 cm); ghost error = how far each
client's ghost of the other is from its true position; path divergence = mean distance of the riders' paths from the
reference paths.

| One-way latency | Mode | Distance after 15 ticks (cm) | Ghost error mean / max (cm) | Path divergence (cm) |
|---|---|---|---|---|
| 50 ms | plain prediction | 267 / 440 / 392 | 3.1 / 255 | 24.9 |
| 50 ms | + response prediction | 284 / 448 / 399 | 2.8 / 151 | 28.7 |
| 100 ms | plain prediction | 258 / 434 / 392 | 9.3 / 391 | 51.9 |
| 100 ms | + response prediction | 282 / 443 / 396 | 8.2 / 330 | 22.3 |
| 200 ms | plain prediction | 228 / 230 / 497 | 24.4 / 330 | 209.2 |
| 200 ms | + response prediction | 264 / 407 / 392 | 26.5 / 594 | 31.9 |

The response prediction keeps contacts close to the zero-latency result up to 200 ms (the default); second-order
(acceleration) dead reckoning was measured too and left off: it lowers the ghost error (24 → 20 cm at 200 ms) but
adds spurious contacts (path divergence 74 cm).

A full "rewind to the contact tick" was considered: resolving a contact at a past tick from both riders' exact
recorded states would make both clients see the same contact, but its response (a push and an impulse) would land a
whole latency late on a present that has moved on, and each tick of overlap until then would push again; it would
need a rollback of the riders, which this design (each client owns its rider) avoids.

### Boost pickups (web/net/pickup-arbiter.js)

In the original the first rider to touch a pickup takes it (within a tick the lower slot, the rider passes run in
slot order) and it leaves play for 60 ticks. Online the take reaches the others one latency later, so two racers can
both take it. Every client applies the same rule to the same (tick, slot) pairs: the later take gives it back
(`pickup_revoke` takes the award's counter increment back, clamped at zero), so exactly one racer keeps it
(`test-mp-pickups.mjs`: two identical runs reach the same pickup on the same tick; slot 0 keeps it, slot 1's counter
drops by the award). The loser had the boost counter for one latency.

## Ranking, HUD, results

- **Ranking 0x10F998** (key −(remaining + place × 20), shell sort, every sixth tick) runs in each client's race
  world as a pipeline every client runs identically: pass P ranks every racer's state after packet tick
  at(P) = the newest packet tick (packets carry every third tick) at least 6 ticks before P, taking this rider's own
  recorded state and the others' exact packets of that tick; a pass waits until those packets are in (a racer that
  stopped streaming -- finished, gone, stale -- keeps its last state). So every client computes every pass from the
  same numbers, including the +20 cm place hysteresis, and all place displays agree (`test-mp-pairs.mjs`: 147 of 147
  passes identical, also side by side). The pair proximity records are refreshed on the present state (ghosts
  dead-reckoned). The place drives the original place display 0x21E1B0 (0x1EA930 change animation, 1st-place glow)
  exactly as with computer riders.
- **Results**: the original Single Event Results screen (ui.js layout, `mp-ui.js drawResults`) with every racer:
  recorded finish times from the server, 0x122D78 estimates for racers still on course, **DNF** for racers who quit,
  disconnected, gave up (TIME'S UP) or ran out of time. Podium screens exist only in Conquer the Mountain's final
  round in the original, so a single online race has none.

## Robustness

| Case | Behaviour |
|---|---|
| Late joiner | Joining a racing lobby seats you for the next race ("Next race" in the lobby); no rider packets meanwhile. |
| Slow loader | Not loaded 45 s after the start: dropped from that race (DNF 'late'), waits in the lobby. |
| Host leaves (also mid-race) | The next online member becomes host; the race goes on (the server, not the host, runs it). |
| Racer leaves / quits mid-race | Pause > Quit (online) = DNF, back to the lobby; others see a DNF row and the ghost leaves the pair world. |
| Network drop | The client reconnects by itself (0.5..8 s back-off) with its token; the server keeps the seat 30 s, tells the others (`presence`), and the race continues where it is. |
| Page reload mid-race | The seat is resumed, the lost run is reported DNF ('reload'), the page waits in the lobby. |
| Rejoin after results | Everyone is back in the lobby; the next Start seats the late joiners too. |
| Version mismatch | `hello.version` ≠ server protocol: a clear "reload the page" message, no half-working session. |
| Rate limits | Per connection token buckets: control 20/s, binary 90/s and 120 KB/s, chat 1/s, lobby creation 1 per 2 s; frames over 4 KB (text) / 16 KB (binary) are refused; unmasked or >1 MB frames close the socket. |
| Clock drift | Clock offset = the lowest-RTT ping sample of the last minute (pings every 2 s); each frame's sim time is nudged so the race tick follows the server clock (`mp-game.js pace` → `web/net/race-pace.js`: beyond 2 ticks of slack, up to 12 ticks a frame of catch-up, 1 of slowdown). The pace counts the frame clock's debt (`web/fixed-step-clock.js` runs at most 12 ticks a frame and keeps the rest); before 2026-09-28 it did not, so a catch-up after a hitch asked for the same backlog every frame and the surplus ran the race tick 50-600 ticks ahead of the server clock for seconds (a 2 s hitch: ~100). The offline stallCap / stallKeys switches do not touch the online path (full catch-up, the pad as is). |
| Hidden / throttled tab | Online races never pause (the pause menu opens over a running race with a neutral pad); when animation frames stop for 250 ms a worker timer keeps the simulation and the stream going. |
| Back-pressure | A congested socket drops state frames (the next one supersedes them), never attacks / world events / control. |

## Finish plausibility (web/server/plausibility.mjs)

The server does not simulate riders, but it relays every state packet, so it checks a claimed finish against the
racer's streamed run (default `MP_PLAUSIBILITY=reject`: an implausible finish becomes DNF 'invalid' with its
findings; `flag` keeps the time with `verified: false`; `off`). The verdict waits for the packet that shows the finish
(or 2 s). Checks and bounds (measured on real full runs -- tuck, boost, tricks with crashes, from two gates, Snow Jam
and BRA2 -- and real browser races, with about 2x margins):

| Check | Rule | Legit runs |
|---|---|---|
| clock ahead | race tick ≤ server ticks since GO + 45; a lead above that must come back within 30 s (1800 ticks) and stay ≤ 1200 (in-order packets only: a replayed old packet does not end a lead) | pace keeps it within ~2 (≤ 0 on arrival); older clients after a hitch 100-600 for 1-10 s |
| clock behind | claimed + countdown ≥ tick of the finish packet's arrival − 300 | latency + catching up |
| finish early | tick of the finish packet ≥ claimed + countdown − 3 | +0..+2 |
| countdown | 180 ticks (3-2-1) on races and freestyle events; 0 on a rolling start: backcountry (`ABC1`, `DBC2`, `EBC3`, Rival Time and Rival Points) and the peak streams (`countdownTicks`) | the core's race clock reads 1 on tick 181 / tick 0 |
| trace | a streamed packet shows the finish with the claimed time; ≥ 50 % of 20 Hz packets | every run |
| motion | ≤ 9000 cm/s between packets of one placement | max 4600 |
| resets | counters +1/+2, jump ≤ 200 m, ≥ 60 ticks apart | jumps ≤ 69 m |
| stage teleports | a longer jump (counter +1 or +0) only from a Metro-City beam's trigger to one of its destinations, within 9000 cm/s × the interval + 3 m (`web/server/teleport-beams.mjs`; BRA2, PEAK1, MOUNTAIN*) | booth 0004 → 0005 / 0006: 148 / 70 m, 0007 → 0008: 438 m, tower → 0002 / 0003: 210 / 387 m |
| distance | path ≥ 0.8 × route, mean speed ≤ 3500 cm/s, time ≥ route / 4000 cm/s (Snow Jam: 88 s) | path 1.18-1.35 × route, ~1900 cm/s, 3:40+ |

`test-mp-plausibility.mjs` replays five real runs (Snow Jam ×3, Metro-City, the 4-minute Happiness backcountry run)
with latency, jitter, a 3 s stall and 10 % loss (no finding) and finds the forgeries: an earlier claim, an earlier
forged stream, a fast race clock, a 40 s course skip, the skip disguised as a reset, no finish in the stream, a
straight line to the finish, half the course, a race clock held 2 s ahead (also with stale packets replayed in
between), a lead beyond the burst cap. It checks the countdown of every courses.json course against its event and the
core's race clock on one course of each event, accepts the live server's false rejections of 2026-09-28 (backcountry
finish packets 1-2 ticks after the claim; a 46..92+ tick lead), and paces 2-6 s hitches through `race-pace.js` on the
real frame clock: the older pacing's overshoot (100-580 ticks) is accepted, the current pacing stays at or behind the
server clock. It also makes real
Metro-City teleports with the core and the stage world, one for each of the five beam outcomes. Each is accepted both with
the placement counter bumped (the core bumps it by one) and without it. The same jump with the approach or the landing
moved 60 m is found, and so is the same stream on another course. The two-client
browser race finishes `verified: true` for both. What it cannot catch: a fabricated stream that stays inside every
bound (a simulated run); only a server-side simulation of each rider would.

## Hosting

- One Node process: `node web/server/mp-server.mjs --port 8080 --host 0.0.0.0 --static web/dist-online,web/public`
  (env `MP_PORT`, `MP_HOST`, `MP_STATIC`, `MP_ORIGINS` = allowed page origins for the WebSocket, `MP_MAX_CLIENTS`,
  `MP_FINISH_GRACE_MS`, `MP_RACE_LIMIT_MS`, `MP_RESUME_MS`). Static files get ETag / 304, byte ranges, immutable
  caching for hashed bundles, `no-cache` for HTML, no COOP/COEP (not needed: no SharedArrayBuffer).
- HTTPS/WSS: put a TLS reverse proxy in front (Caddy `reverse_proxy 127.0.0.1:8080`, or nginx with
  `proxy_http_version 1.1; proxy_set_header Upgrade $http_upgrade; proxy_set_header Connection "upgrade";` on `/mp`).
  The page picks `wss://` itself on an https origin (`mp-client.js defaultServerUrl`). WebGPU needs a secure context
  anyway, so a public host must be HTTPS.
- Container: `web/server/Dockerfile` (build context `web/`) builds the code and runs the server; the extracted game
  data is mounted at `/app/public` (it comes from your own disc and is not put in the image):
  `docker build -f server/Dockerfile -t ssx3-online . && docker run -p 8080:8080 -v "$PWD/public:/app/public:ro" ssx3-online`.
- Bandwidth per racer: ~38 KB/s up, ~38 KB/s × (racers − 1) down; the server relays, it does not simulate.
- Nothing is deployed anywhere by this repository.

## Pieces

| File | Role |
|---|---|
| `web/server/mp-server.mjs` | Lobbies, race control (start/loaded/GO/finish/results, DNF, time-up), relay, resume, presence, rate limits, static files. `/mp/status` shows the live state. |
| `web/server/online.mjs` | `npm run online`: server + dev server for the network. |
| `web/server/vite.online.config.js`, `web/server/Dockerfile` | Hosting build and container. |
| `web/net/mp-client.js` | Connection, reconnect, clock sync, lobby state, invite links, binary frames. |
| `web/net/rider-packet.js` | State / attack / world packets, FX record coding, sender capture. |
| `web/net/pose-codec.js` | The core's skin palette in JS, bit for bit (toward-zero float arithmetic). |
| `web/net/remote-riders.js` | Remote racers: snapshots, palettes, interpolation, dead reckoning, ghosts, lighting inputs. |
| `web/net/pair-net.js` | The online race world: ghosts in 0x107888, attacks, contact streams, response prediction, ranking, place display timers. |
| `web/net/pickup-arbiter.js` | Boost pickup arbitration (first take keeps it). |
| `web/net/grid-seed.js`, `tools/export_grid_scales.py` | An online racer's countdown state for its slot, body scale and stance (recorded PS2 spots). |
| `web/server/plausibility.mjs` | Server checks of claimed finish times against the streamed run. |
| `web/server/records.mjs`, `web/server/replay-file.mjs` | Online course records and their replays (pv onlineRecords, docs/online-records.md); off without `MP_RECORDS_DIR`. |
| `web/net/remote-fx.js`, `web/fx_puppet.inc` | Remote riders' effects: FX records, puppet cores, effect renderers. |
| `web/net/mp-game.js` | Game glue (main.js hooks), results rows, hidden-tab ticker, race clock pace. |
| `web/mp-ui.js` | Lobby screens, the pre-race card (`mp-card`, pv mpPreRace) and the online Single Event Results. |
| Core exports | `rider_skin_scale`, `race_world_pair_disable`, `race_world_pair_respond`, `shade_external_rider_lighting`, `fx_recording`, `fx_records`, `fx_record_size`, `fx_puppet_reset`, `fx_puppet_step`, `pickup_revoke` (+ `OriginalPairCallbacks::attackHit`, `OriginalRiderPairSystem::respondToAttack`). None changes single-player / AI behaviour (capture gates exact). |
| Tests | `test-mp-grid.mjs` (grid spots per slot, scale and stance), `test-mp.mjs` (server end to end), `test-mp-prerace.mjs` (the pre-race protocol), `test-mp-page.mjs` (two pages; `MP_PICK`, `MP_PRERACE`), `test-remote-riders.mjs` (packets, bit-exact palettes, receiver), `test-mp-pairs.mjs` (rider vs rider over a link, attack reactions), `test-mp-fx.mjs` (effects replay), `test-mp-latency.mjs` (contacts at 50/100/200 ms vs zero latency), `test-mp-pickups.mjs` (pickup arbitration), `test-mp-plausibility.mjs` (finish checks: real runs and forgeries). |

## Rider speech (pv mpRiderSpeech, pv mpSpeechPlace, 2026-10-14)

The PS2's two-player mode (0x535C11 == 2) plays the other human's rider lines: 2A0560 (pass) / 2A0A30 (hit) have no game-type
test, and their local-rider gate (vtable+0x48 = 0x140BC8, rider+0x874 ^ 1, with +0x87C) passes when one rider of the pair is a
local human; both riders' lines post and 2A1820 is 2 (foe) for two humans. Online, `web/net/mp-game.js speechRiders()` gives
`web/game-audio.js gameTick({online})` the local racer's slot and every live remote racer (character, a cheat skin as its base
rider; the ghost pair view's motion [96], position [99..101], velocity [87..89] at the tick). `web/sfx-game.js` keeps them as
riders without a core: the local core's overtake (race_world 299E28 events) and pair events then reach the pass / hit speech,
the bump / attack sounds and the target's grunt. A pass between two remote racers says nothing. The PA lines take the online
rank (pairNet.hud().place, 10F998's +0xEC) as 2A34D0 / 2A1E68 do in multiplayer. Their board / crash loops are not played.
Details and the ARMSX2 evidence: [audio-logic.md](audio-logic.md) 4.5 "Who speaks".

## Known limits

- Contacts use dead-reckoned ghosts with response prediction: at 200 ms a contact stays within ~30 cm of the
  zero-latency result on average, not exactly it (each side resolves the contact it sees).
- Crashbags / teeters / triggers are replayed with the sender's latency; boost pickups are arbitrated (above).
- Contact reactions draw from keyed streams, not the one PS2 world sequence (above).
- Finish plausibility bounds a claim; it cannot detect a forged run that is physically plausible.

### Stage teleports (2026-09-27)

The Metro-City phone booths and water tower (stage builtin 34, [stage-teleport.md](stage-teleport.md)) move the rider
70-438 m. Three of the five outcomes are longer than a reset may jump (200 m), so an honest finish through them was rejected.
- **Server:** `createRunCheck({ course })` (mp-server.mjs passes the lobby's course) accepts such a jump when
  `teleportJump` explains it. The previous packet must be within reach of a beam's trigger box, and the new one within
  reach of one of that beam's destinations. A destination is any roster-slot offset P, anywhere from 70 m below P.z to 2 m
  above (the 11D660 probe). The total of both distances must stay within 9000 cm/s × the packet interval + 3 m. The global
  limits are unchanged, and a matched jump is neither path nor a reset.
- **Table:** `TELEPORT_BEAMS` holds the trigger boxes (world_collision.json) and the destination matrices
  (stage-world.json teleports). The test rebuilds the table from the stage data and compares.
- **Client:** a teleport places the rider through 11D660, which bumps `_reset_info()[2]`. So the packet's `placements`
  changes, remote riders do not interpolate or dead-reckon across it (serial = placements + rescues), and the FX records
  carry the reset (the puppet's trails restart). One small difference remains: without deferred FX (online, no computer
  riders) the rider's FX pass runs before 121818, so the puppet restarts its trail one record before the local one does.

