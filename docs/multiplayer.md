# Online multiplayer (browser port)

Online races of up to six human racers, each on their own machine, with the original's race world: the same grid
gates, rider-vs-rider contacts, knockdowns and punches, the original place display, board tracks / spray / boost of
every rider, a shared world (pickups, crashbags, sections), and the original Single Event Results with every racer.

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
   seat stay, the URL becomes `?course=<code>&rider=..&online=1&lobby=<ID>`; docs/course-switch.md "Online"),
   **Chat** (optional, rate limited), **Start Race**.
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
| `web/mp-ui.js` | Lobby screens and the online Single Event Results. |
| Core exports | `rider_skin_scale`, `race_world_pair_disable`, `race_world_pair_respond`, `shade_external_rider_lighting`, `fx_recording`, `fx_records`, `fx_record_size`, `fx_puppet_reset`, `fx_puppet_step`, `pickup_revoke` (+ `OriginalPairCallbacks::attackHit`, `OriginalRiderPairSystem::respondToAttack`). None changes single-player / AI behaviour (capture gates exact). |
| Tests | `test-mp-grid.mjs` (grid spots per slot, scale and stance), `test-mp.mjs` (server end to end), `test-remote-riders.mjs` (packets, bit-exact palettes, receiver), `test-mp-pairs.mjs` (rider vs rider over a link, attack reactions), `test-mp-fx.mjs` (effects replay), `test-mp-latency.mjs` (contacts at 50/100/200 ms vs zero latency), `test-mp-pickups.mjs` (pickup arbitration), `test-mp-plausibility.mjs` (finish checks: real runs and forgeries). |

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

