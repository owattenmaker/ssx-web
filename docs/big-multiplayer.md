# Big online sessions: time attack (design, phase 1)

Status (2026-10-13, big-multiplayer agent): design reviewed; **building TA-1..TA-6**, each behind a default-off switch.

**The user's decisions (2026-10-13, on section 9):**
- Time limit: **6:00 by default**, set by whoever creates the session; a run in progress at the deadline may finish.
- Rotation: **every event with a board** (records.mjs eventTable: 20 events): the five races and the three Rival Time runs ranked by
  time (lowest wins), the slope style, big air, super pipe and three Rival Points events ranked by score (highest wins). Freestyle
  events keep their own in-event time limits. Podium locators (CUTSCENES/locators.json `locators['0'].podium_steps`) exist on 14
  courses: ABA1 ARA1 ASS1 BHP1 BRA2 CBA2 CHP2 CRA3 DRA4 DSS2 EBA3 EHP3 ERA5 ESS3; the backcountry courses ABC1, DBC2, EBC3 (6 events)
  have none and show the standings screen instead.
- Attributes: **maxed for everyone**: the 7 raw attribute bytes at 55 (level 11, buy-attribs.js LEVEL_MAX); the server refuses
  any other bytes in a run's replay.
- Session cap: **50** to start.
- The vote (3 candidates + same course again) and translucent ghosts as proposed.
Sections below that say otherwise (Q1's 10:00, the five-race rotation, stock attributes) are superseded by this list.

**The user's rules for the podium and the vote (2026-10-13, TA-5; they replace section 2.4's vote):**
- **Podium:** the CTM podium cutscene (web/cutscenes.js, WS12 0x27AC60) with the window's top three verified riders in their own
  characters, each called out on screen with its place (the race HUD's own number and suffix, race-place-hud.js: gold 1ST, blue
  2ND / 3RD) and its player name in the front-end font, above its step. The podium scripts stand binding 1 / 2 / 3 on anchors
  20 / 21 / 22 (checked in every win_ps_* / placeshow_* script's actor tracks), so the winner's label is over anchor 20. Outfits:
  the front end's preview models exist for the default outfits and this page's own worn outfit only, so another player's outfit
  shows its character's default one.
- **Courses without podium steps** (the backcountry ABC1, DBC2, EBC3: 6 of the 20 events): the PS2's Single Event results panel
  (43final_standings) with the gold / silver / bronze hexes on the top three. The PS2 never shows a podium there, and its results
  with medals are the closest presentation it has.
- **Vote:** the last window's results stay on screen: the same results panel (the top six, their names and times or scores, the
  medals on the top three). Below it, three thumbnail boxes: **two events and Random**. A box shows the event's Select Event map
  art: its peak map (MAPGFX_map_peak<A|B|C>) with only its route, red, cropped around the route. There is no course picture on the
  disc (99QPEvent is not played), so the map is the game's own event art. Random shows the whole mountain with the loading
  screen's orange '?' (GL_1-1). Each box has its name and its live vote count. Left / Right pick, Select votes.
- **Random** draws uniformly from the rotation, excluding the two events shown (the current event can come up). The two events are
  drawn from the rotation except the event just ridden and the two before it (while the rotation has enough others).
- **Ties and no votes:** drawn at random among the tied options (no votes: among all three).
- **Names:** a session shows each player's Player Name (Options > Player Name), sent with ta-create / ta-join, not the rider's
  name. Today's 6-player lobbies ([multiplayer.md](multiplayer.md)) stay as they are; both new modes go behind default-off
switches (section 8).

Goal: online sessions of 20-100 players in a new **time attack** mode (Trackmania style) next to today's lobbies: the server loads
a course, everyone rides it for a time window, restarting as often as they like; each player's best complete run counts; then a
podium, a server-wide vote for the next course, and the next window. Other players are ghosts: drawn, never collided with.
(Scope, 2026-10-13: the user dropped the mass-race mode; only time attack is designed here.)

The mode does not exist on the PS2 (its online mode, flag 0x534B30, is a 6-rider race), so most of this is a port extension. What
stays PS2-exact: each player's own simulation (the solo run in time attack is a Single Event run), the screens, fonts and the
podium cutscene. Every port-only rule below is marked **(port-only)**.

## 1. Measurements (this Mac, Apple M5 Pro, node 26, the live core)

Scripts: the session scratchpad `measure-packets.mjs` (a 3600-tick scripted run per rider through web/net/test-core.mjs,
every 20 Hz packet encoded by web/net/rider-packet.js) and `relay-load.mjs` (bots against a local mp-server on 127.0.0.1:18787).

### 1.1 Today's state packet (Snow Jam, 1200 packets per rider)

| Part | Zoe (27 bones) | Psymon (26 bones) |
|---|---|---|
| header | 56 B | 56 B |
| world pose (f32 position + quaternion per bone) | 756 B | 728 B |
| pair view (16 body spheres + tail) | 388 B | 388 B |
| lighting (irradiance bank, rim, bounds, point) | 200 B | 200 B |
| FX records (3 passes, delta-coded) | 495 B | 493 B |
| **total, mean / max** | **1895 / 1975 B** | **1865 / 1934 B** |
| deflate per packet | 1466 B (-23 %) | 1434 B |
| deflate with context takeover (permessage-deflate style) | 1246 B (-34 %) | 1231 B |

At 20 Hz that is 37-38 KB/s per sender, as multiplayer.md says. Compression alone does not change the picture (a third at best).

### 1.2 Today's relay (local mp-server, real packets, 20 Hz, 15 s each)

| Bots | Server CPU | Server in | Server out |
|---|---|---|---|
| 6 (one full lobby) | 0.9 % of a core | 213 KB/s | 1067 KB/s |
| 24 (4 lobbies) | 2.0 % | 853 KB/s | 4270 KB/s |
| 96 (16 lobbies) | 6.3 % | 3414 KB/s | 17078 KB/s |

- **The relay's CPU is not the limit; the host's upload is.** One full 6-player lobby already sends **~8.7 Mbit/s** out of the home
  connection (plus cloudflared's TLS / QUIC overhead and CPU, not measured here). Relaying everyone's full packet to everyone grows
  as N²: 20 players would need ~117 Mbit/s, 100 players ~3 Gbit/s. So time attack needs interest management and smaller packets.
- Not measured: the host's real upload speed (open question Q7) and cloudflared's CPU per Mbit/s.

### 1.3 A quantised ghost pose

- **Rigidity:** in the bone hierarchy (rider.json `parent_index`), every bone's offset in its parent's frame stays within 0.04 cm
  over the run, except three: `footleft`, `footright` (up to 1.4 cm / 27 cm) and `board_childg` (the board, up to 81 cm).
- **So a pose is:** the root position (3 x f32), one smallest-three quaternion per bone, and those three bones' positions relative
  to the root (i16 at 1 mm). The receiver rebuilds the others by forward kinematics from the parent offsets (constant per package).
- **Error** (worst bone, per packet, vs the exact world pose):

| Quaternion bits | Mean | Max | Pose bytes (27 bones) |
|---|---|---|---|
| 10 (32-bit word) | 0.11 cm | 0.25 cm | 12 + 108 + 18 = 138 B |
| 9 | 0.15 cm | 0.33 cm | (bit-packed) ~122 B |
| 8 | 0.26 cm | 0.49 cm | (bit-packed) ~106 B |

  The exact palette is not needed for a drawn ghost (today's renderer already blends between packets, "presentation only").

### 1.4 Simulation and verification speed

- Solo human core tick in node: 0.55-0.72 ms with lighting and FX recording on (measure-packets.mjs); 0.12 ms riding alone in the
  TAS node harness (docs/tas.md "The node harness"); WebKit 0.137 ms per human tick (docs/sim-performance.md).
- Today's Chrome verifier (docs/online-records.md): 17-20 s wall per 16,000-tick Snow Jam run **with five computer riders**, ~30-60 s
  page load per batch, ~2.8 GB Chrome tree RSS.
- **Estimate (to measure in TA-2):** a solo time-attack run (no computer riders, ~11,000 ticks for a 3:00 run) on a page already on the
  course: ~2-4 s. In a node harness: ~1.5-8 s of CPU.
- Replay upload size: a 2:22 TAS run is 23 KB (local/tas/page/best-8507/replay.ssxr); online-records.md gives ~1 KB a minute for a
  tuck run and ~13 KB a minute deflated for a stick that moves every tick, plus 4-6 KB of meta. A 3-minute human run is ~20-45 KB.

### 1.5 Course length (PS2 default records, career.json rules.records)

The five races' default top times are 2:57-3:37 (Snow Jam 177-215 s, Metro City 178-210, Ruthless Ridge 179-207, Intimidator 181-217,
Gravitude 180-216); the Rival Time courses are 2:45-4:03. **A 6-minute window gives most players one or two complete runs.**
Trackmania tracks are 30-60 s; SSX races are three to four times longer. This drives open question Q1.

### 1.6 Drawing many riders (code survey; numbers from the docs)

- web/opponent-riders.js draws each rider as its own GPU-skinned set of meshes: one palette texture each, 5-7 materials,
  ~21 draw calls per rider (docs/web-render-performance.md: 106 draws for five), no sharing between riders of the same package
  (every texture is decoded again), no instancing, no LOD, no per-rider opacity.
- 0.18 ms of Chrome render time per rider (Snow Jam race, 5 computer riders); WebKit's 6-rider event frame is already 20 / 36 / 49 ms
  p50 / p95 / p99 with 525 draws (docs/sim-performance.md).
- `buildPalette` (pose-codec.js, the bit-exact software-float palette): 126-177 µs per packet in V8. 99 riders at 20 Hz would be
  ~300 ms of CPU a second.
- Remote effects: one full wasm core per remote racer (web/net/remote-fx.js, INITIAL_MEMORY 128 MB), and a renderer pool of
  exactly 5 (`rendererPool`, count 5). Rider shadows: `SHADOW_MAX = 6`.
- **So today's rider path cannot draw 99 riders.** Ghosts need their own cheap renderer (section 4).

## 2. Time attack

### 2.1 Session flow

Phases of a session room, all driven by the server's clock (every client shows the same times through mp-client's clock offset):

```
LOAD (course) -> WINDOW (time limit) -> OVERTIME (runs in progress finish) -> VERIFY (podium candidates) -> PODIUM -> VOTE -> LOAD ...
```

| Phase | Length (default) | What happens |
|---|---|---|
| LOAD | until 90 % loaded, at most 45 s | everyone loads the course behind the original loading screen (as today's race start) |
| WINDOW | the time limit (Q1) | attempts; restarts at will; best complete run per player counts |
| OVERTIME | until every attempt that started before the deadline ends, at most 4:30 | no new attempts; runs in progress may finish (port-only) |
| VERIFY | at most 15 s | the verifier finishes the podium candidates (section 2.3) |
| PODIUM | about 20 s (skippable per client) | the CTM podium cutscene with the top three, then the final standings |
| VOTE | 15 s | the next course (section 2.4) |

- **Join mid-session:** at any phase. During WINDOW, the client loads the course and can start attempts until the deadline. During
  OVERTIME / VERIFY / PODIUM it watches the standings and votes; it loads with everyone at LOAD.
- **Leave:** the player's verified best stays in the standings and can still make the podium (port-only; Q4).
- **Reconnect:** today's token resume (30 s). The best times live on the server, so they survive a reload too; only the attempt in
  progress is lost.
- **Spectating:** v1 = the standings screen and the HUD. Following another rider with the game camera needs a camera target that is not
  the local rider's core; not in v1 (open item O2).
- **The window's attempts:** an attempt is a Single Event run of the session's course with no computer riders: the PS2 countdown
  (3-2-1, 180 ticks; none on a backcountry rolling start), the race, the finish. Restart = the Single Event pause's Restart (PS2 menu
  r3-pause) which re-runs startRun and a new countdown; an optional one-button restart (port-only, its own switch) can come later.
- Each attempt starts with `{t:'attempt', n}` (the server notes its start time) and ends with a finish, a Give Up, or a restart.

### 2.2 Simulation model

- **Each client runs a solo deterministic run.** The core is exactly the Single Event one: the rider's own grid seed (slot 0),
  `aiRace.enabled = false` (the lineup `{values: [], enabled: false}`, ai-race.js prepareFixed already maps that to no computer
  rider), no pair world (pairNet is not created), no shared world, no remote FX puppets. Ghosts never enter the core.
- **Each attempt is recorded** as today (main.js startRun -> replay.liveStart), so a finished attempt has a replay file
  (online-replay.js onlineReplayFile, with the empty lineup).
- **Attributes:** the stock Single Event attribute bytes for everyone (as online races since the career reset fix); the server
  refuses a replay whose `meta.rider.attributes` are not the rider's stock ones (Q5).
- **To confirm in TA-2:** that a solo race replay (empty lineup) verifies through today's `?verify` path. prepareFixed's
  `values: [] / enabled: false` branch goes through prepareFreestyle with no opponent; this has only been exercised for freestyle.

### 2.3 Anti-cheat: uploads and the verifier

- **Upload:** after a finished attempt that beats the player's best in this window, the client POSTs the deflated replay file to
  `/mp/session/run` (session id, attempt n). The server runs records.mjs's tier-0 checks (decode, meta, pad stream walk, sizes,
  the course floor from record-floors.json), checks the attempt was announced (its start + the claimed time + the countdown fits the
  upload's arrival, as plausibility.mjs's clock checks), and lists it at once as **unverified**.
- **Liveness (port-only):** the server keeps each attempt's ghost stream at 1 Hz (position, tick). The verifier's re-simulation
  reports the rider's position at those ticks; a replay whose path is more than ~5 m from what was streamed live is pulled. This
  ties a replay to an attempt actually ridden in this session (a replay prepared offline does not match the live stream).
- **Verifier session lane** (web/server/records-verifier.mjs grows a second queue; same token and loopback rules):
  - Today the verifier skips while any online race runs (`busy()`), so it would never run during a session. The session lane runs
    regardless (still niced), the records lane keeps the old rule.
  - Workers: W Chrome pages, each parked on the session's course (no course switch per run). W from `MP_SESSION_VERIFIERS` (Q7).
  - Priority: (1) the runs that would place in the current top 3, (2) the top 10, (3) every other best. A newer best of the same
    player replaces its queued older one.
  - Budget: with W = 2 and ~3 s per run, ~40 runs a minute. 100 players x 2 improvements each = 200 runs per window: all
    verified within the window when it is 10 minutes, the top 10 always.
- **Deadline rule:** the podium shows verified runs only. VERIFY waits at most 15 s for unverified runs that would make the top 3;
  one still unverified then is skipped for the podium (the next verified run moves up), shown as "unverified" in the final
  standings, and verified afterwards for the session history. A pulled run drops out of the standings at once.
- **Later (TA-7, optional): a node verifier.** tools/tas/race.mjs already reproduces page runs exactly in node (docs/tas.md) at
  0.12 ms a tick solo, without Chrome or WebGPU. It imports web/*.js, which the host does not have (only dist-online and
  web/server), so it would ship as a bundled file in dist-online. Gate: its verdicts equal the Chrome verifier's on every test run.

### 2.4 Rotation, vote, podium, standings

- **Rotation (default, Q2):** the five races (ARA1, BRA2, CRA3, DRA4, ERA5); the three Rival Time courses (ABC1, DBC2, EBC3) as an
  option. All 17 courses are ready (courses.json); the freestyle events are score events and need a score-attack variant (later).
- **Vote (default, Q3):** three candidates drawn from the rotation (not the current course, not the previous two) plus "same course
  again"; 15 s; one vote per player, changeable until the end; most votes wins, a tie is drawn at random, no votes = a random
  candidate. The result is shown for 2 s, then LOAD.
- **Podium:** the CTM podium cutscene (web/cutscenes.js `playCutscene({kind: 'podium', place: 0, roles})`, PS2 WS12
  0x236F08 / 0x236F40 / 0x236F90, steps 0x27AC60 / table 0x481E90) with `roles` = the top three's riders (their characters,
  outfits through `entry.fe_root`). Podium locators exist for 14 courses, including all five races; the backcountry courses have
  none, so they show the final standings only. Each client can skip it (Cross) as in CTM; the vote starts on the server clock
  either way. Gating the call: today `career-ui.js finishCutscenes` only plays it in career mode with the human in the top three;
  the session needs its own trigger (the human can be anywhere in the standings) (port-only).
- **Final standings:** `43final_standings` (results-lui.js `results()`: 6 rows) with paging, the page in the help line the way the
  online records board (`ctm-board`, 61toptimes, `PAGE = 5`) shows "Page n of m" (paging is port-only; the PS2 screen has 6 rows).

### 2.5 Ghosts (display only)

- Every client streams a **ghost packet** (section 3.1, 152 B) at 20 Hz during its attempts; the server forwards each recipient a
  selection (section 3.2).
- Ghosts of other players are on other attempts, so "where they are now" has no meaning for gameplay: the client can draw them with a
  large interpolation delay (200-300 ms) without harm. A ghost's attempt restart is a teleport (serial change): never interpolated.
- Ghosts have no collision, no pickups, no shared world, no effects, no shadows, no rider lighting (section 4).


## 3. Network

### 3.1 Packets (binary, little endian, new kinds; today's 2 / 3 / 4 unchanged)

| Kind | Name | Contents | Size |
|---|---|---|---|
| 5 | ghost pose (client -> server) | u8 kind, u8 flags, u16 serial, u32 attempt tick, f32 x3 root, i16 x3 velocity, 27 x u32 quaternions, 3 x i16 x3 free bones | 152 B |
| 6 | ghost lite (server-made) | u16 player, u16 serial, f32 x3 position, u16 yaw + i8 pitch + i8 roll, i16 x3 velocity, u8 pose hint (riding / air / crash) | 26 B |
| 7 | ghost dot (server-made) | u16 player, f32 x3 position | 14 B |
| 11 | bundle (server -> client) | u8 kind, u8 count, u32 server tick, then count x (u16 player, u16 length, payload) | 8 B + 4 B per entry |

Kind 5 in detail: flags = finished, gave up, new attempt; serial = attempt + placements + rescues (a change is a teleport); velocity
in cm/s / 4; quaternions smallest three at 10 bits; free bones (both feet, the board) at 1 mm from the root (section 1.3).

- The body scale, rider, package and outfit travel once in the session roster, not in every packet.
- The server derives kinds 6 and 7 from each sender's newest kind 5 (it parses packet headers already, plausibility.mjs readHeader).
  A client only ever sends kind 5.
- The protocol number stays 2 while these are additive: the server accepts kind 5 only from a session member, and an old client
  never joins a session.
- The server sanity-checks the stream it relays (plausibility.mjs's motion bounds), so a forged ghost stream cannot teleport around
  other players' screens; it decides nothing about the standings (the verified replay does).

### 3.2 Update rates and interest management

The server picks, per recipient, which senders it forwards at which rate, and sends everything due in one bundle per 100 ms
(fewer WebSocket frames through the tunnel). Distance is 3D, from the recipient's own rider (between attempts: its last position).
The sets update at 2 Hz with hysteresis.

| Tier | Who | Rate | Packet |
|---|---|---|---|
| near | the 6 nearest | 10 Hz (20 Hz when the budget allows) | 5 |
| mid | the next 18 | 4 Hz | 6 |
| far | everyone else | 1 Hz | 7 |

Ghost latency does not matter (section 2.5), so 10 Hz with a 200-300 ms interpolation delay is enough for the near tier. A budget cap
(`MP_SESSION_OUT_KBPS`) makes the server shrink the near count and rates first. Back-pressure keeps today's rule: ghost frames drop,
control messages never.

### 3.3 Server outbound bandwidth (computed from the measured sizes; per recipient and in total)

Assumptions: the sizes above, 52 B of WebSocket + tunnel framing per bundle, ~0.2 KB/s of control and standings. Not yet
load-tested (TA-6).

| Players | Today's relay (everyone's full packet, for comparison) | Time attack, near at 10 Hz | Time attack, near at 20 Hz |
|---|---|---|---|
| 6 | 188 KB/s each, 9.2 Mbit/s | 8 KB/s each, 0.4 Mbit/s | 16 KB/s each, 0.8 Mbit/s |
| 20 | 714 KB/s, 117 Mbit/s | 11 KB/s, **1.9 Mbit/s** | 21 KB/s, 3.4 Mbit/s |
| 50 | 1.8 MB/s, 754 Mbit/s | 12 KB/s, **5.1 Mbit/s** | 22 KB/s, 8.8 Mbit/s |
| 100 | 3.7 MB/s, 3 Gbit/s | 13 KB/s, **10.9 Mbit/s** | 22 KB/s, 18.4 Mbit/s |

- Server inbound (the host's download): 3 KB/s per player of ghost packets, plus the replay uploads (~20-45 KB per personal best;
  ~200 uploads per 100-player window is ~6 MB).
- Client downstream is the per-recipient column (8-22 KB/s); client upstream is 3 KB/s.
- **Time attack at 100 fits a typical home upload (~11 Mbit/s at the 10 Hz default).** The per-recipient rate grows only with the
  mid and far tiers (26 / 14 B), so the total is close to linear in N.
- Today's own 6-player lobby (9.2 Mbit/s) is not changed by this work.

### 3.4 Control traffic

Today `broadcastLobby` sends the whole lobby (every member, ~250-450 B of JSON each) to every member on every change, and the lobby
list to every client outside a lobby: O(N²) per change, ~2.5 MB per change at 100 members. Sessions send deltas instead:
`{t:'roster', add | drop | update}` for one member, standings deltas at most 2 Hz (the top 10 and the recipient's own row), and the
full state only on join / resume.

## 4. Client performance (Safari first)

- **A ghost renderer** (new, web/ghost-riders.js; port-only look, since the PS2 has no ghosts):
  - geometry and textures shared per package (one upload for every ghost riding Zoe); ghosts beyond the near tier draw their rider's
    default outfit, so a crowd does not download 3-11 MB of gear textures per outfit;
  - instanced: one draw per package per material (the pose of every instance in one RGBA32F texture, pose to matrix and bind in the
    vertex shader), so 99 ghosts of 10 packages are ~60 draws instead of ~2000;
  - plain float arithmetic: no bit-exact palette (pose-codec.js's 126-177 µs per packet is for exact remote riders), no rider
    lighting call, no shadow, no FX puppet, no board / hand morphs;
  - the look: translucent with the rider's own textures. Screen-door (dithered) transparency keeps depth writes and needs no sorting,
    which suits WebKit; real alpha blending is the alternative (Q6);
  - LOD: the near tier skinned at its 10 Hz pose, interpolated; the mid tier skinned at its 4 Hz pose (the lite packet's orientation
    and pose hint on a stock riding / air / crash pose of the package); the far tier a small marker. A visible cap (default 32
    skinned ghosts) and frustum culling by the 5 m sphere (main.js cullOffscreenRiders' rule).
- **Budget to hold** (measured in TA-4 with 99 bot ghosts, WebKit through webkit-driver.mjs and Chrome through headless-chrome.mjs):
  - ghosts add at most 2 ms of frame CPU (decode + interpolation + uploads) and 2 ms of GPU at 99 ghosts;
  - the event frame's p95 moves by at most 2 ms (WebKit's solo event frame is the baseline, no computer riders in time attack);
  - WebContent memory +50 MB at most (shared packages: ~0.2 MB of buffers and ~0.55 MB of textures per distinct package);
  - no input latency: ghosts are drawn after the local tick, and nothing waits for them.
- The local solo run is cheaper than a Single Event race (no computer riders: one rider's tick instead of six), which leaves the
  frame room the ghosts use.

## 5. UI (the game's own screens, fonts and style)

| Screen | Built from | Notes |
|---|---|---|
| mode select | mp-ui.js `mp-lobbies` (PDA frame, FEFONT rows) | a "Time Attack" row above the lobbies, with the course, players and time left; the PS2's own online menus first (below) |
| session HUD | race clock `raceClock` 0x1F16C0 (attempt); freestyle clock 0x1EC3F8 (time left, red under 10 s); 3-row standings 0x17 (top 3, you) | port-only arrangement |
| end of window | `trickHud.finishBanner(list, true)` TIME'S UP (0x21F660) | |
| vote | the Single Event map (fe-event-select.js `fe-event`, route drawn red), votes on the INFO panel; or the PDA list | no course thumbnails exist |
| podium | the CTM podium cutscene (section 2.4) | backcountry courses: none |
| standings | `43final_standings` with paging | |

HUD sources: trick-hud.js `raceClock` (0x1F16C0, HUDFONT), career-ui.js `hud()` (the freestyle clock 0x1EC3F8, the red blink
0x4C8688), trickHud `standings` / `standingsRows` (descriptor 0x17 at (20, 20)).

**Decomp before the UI is built (TA-3):** the PS2 has an online mode: the flag 0x534B30 (WS3 enter 0x234B14 calls 2300F0 only
online), the load card 102MPMatch when 0x535C11 is 2 (docs/loading-screen.md), and the EA Talk row. Its lobby / match screens in
FE.LUI / GL.LUI, if any, come first, before anything is made from other screens.

## 6. Server (web/server only: imports from web/server and node:)

- **web/server/sessions.mjs** (new): rooms `{id, course, phase, phaseEndsAt, rotation, recent, players: Map(id -> {name, rider, pkg,
  base, outfit, scale, best, pending, attempts, lastPacket, tierSets}), votes}`, the phase machine (section 2.1), the vote, the
  ranking (the verified best, then the earlier submission, as records.mjs `better`), the standings deltas, the session history (the
  last sessions' standings in `<MP_RECORDS_DIR>/sessions/`).
- **web/server/interest.mjs** (new): the tier sets, the derived lite / dot entries, the bundles, the budget cap.
- **Rooms:** one public time-attack room; when it reaches the cap, a second opens (players pick on the mode select).
- **mp-server.mjs:** mounts both only with `MP_SESSIONS=1` (off by default: today's server unchanged), routes kind 5 and the session
  messages, `/mp/status` gains the sessions. Settings: `MP_TA_LIMIT_MS` (Q1), `MP_TA_ROTATION`, `MP_SESSION_MAX` (Q8),
  `MP_SESSION_OUT_KBPS`, `MP_SESSION_VERIFIERS`.
- **Verifier queue:** section 2.3. The server keeps per-session priority queues; records-verifier.mjs asks
  `/mp/session/verifier/queue` before the records queue, and reports to `/mp/session/verifier/result` (loopback + token, as today).
- **Limits:** per-connection token buckets as today (kind 5 at 20 Hz is far under the 90 frames/s limit); uploads 6 a minute per
  player; the room cap; MAX_CLIENTS (300) stays above the caps.

## 7. Build plan

Every step: decomp where a PS2 rule exists, implement, check in Chrome and WebKit; behind its switch until verified. No core physics
changes. game-tick.js and ai-racers.js are not touched; if TA-3 needs a hook there, it is agreed with the core-migration agent first.

| Step | What | Check |
|---|---|---|
| TA-1 | sessions.mjs phases, rotation, vote, attempts, the upload endpoint with tier-0, provisional standings (MP_SESSIONS) | test-mp-session.mjs with node bots |
| TA-2 | the verifier session lane, the solo-run verify (empty lineup), liveness, the deadline rule; measure the per-run cost | real solo runs verified; a forged and an offline replay pulled |
| TA-3 | decomp of the PS2 online menus; client: pv `onlineTimeAttack`: join, load, attempts, restart, upload, HUD, end of window | Chrome + WebKit, local server |
| TA-4 | pv `ghostRiders`: kind 5, the server tiers and bundles, the instanced ghost renderer, LOD | 99 bot ghosts: Safari / Chrome frame budget |
| TA-5 | podium, final standings with paging, vote screen | Chrome + WebKit |
| TA-6 | load tests at 20 / 50 / 100 (section 7.1); then the coordinator flips the switches with the user's OK | |
| TA-7 | optional: the node verifier | verdicts equal the Chrome verifier's |

### 7.1 Load test

- **Stream bots** (one node process, hundreds of bots): replay recorded ghost streams (kind 5 from recorded runs; every bot its own
  run and offset), restart at random, upload recorded replay files at their finish ticks, vote at random. For the relay, the
  bundles, the tiers and the control traffic.
- **Sim bots** (a few, node, the core): ride recorded pads (local/tas/*.tas, scripted pads) and stream exactly as the page does, so
  their uploads are real runs for the verifier lane (and a few forged ones it must pull).
- **One or two real clients** (Chrome via headless-chrome.mjs `startBrowser()`, WebKit via webkit-driver.mjs, `?mute=1`) in the bot
  session for the frame budget and memory with 19 / 49 / 99 ghosts.
- **Uplink:** a throttling proxy on 127.0.0.1 between the bots and the server at 5 / 10 / 20 Mbit/s, to see the budget cap and the
  back-pressure work; no system network settings changed.
- **Measured at 20, 50, 100:** server CPU and event-loop delay (perf_hooks monitorEventLoopDelay), bytes out per recipient and in
  total, dropped frames, upload-to-verdict time per priority, deadline-to-podium time, LOAD time to quorum, server memory, the real
  clients' frame p50 / p95 / p99 and WebContent memory.
- Only against a local mp-server on 127.0.0.1; never the public server.

## 8. Switches

Client (web/pv-flags.js, added to PV_DEFAULTS before any code reads them, all off): `onlineTimeAttack`, `ghostRiders`.
Server: `MP_SESSIONS=1` (unset: no sessions). With all of them off, the page and the server behave as today.

## 9. Open questions for the user (each with a recommended default)

| # | Question | Recommended default |
|---|---|---|
| Q1 | Time limit. At 6:00 most players get one or two complete runs (races take 3-4 min, section 1.5). | **10:00**, configurable; a run in progress may finish (at most 4:30 more) |
| Q2 | Courses in the rotation: races only, or every timed event? | the **five races**; the three Rival Time courses as an option; freestyle later as a score attack |
| Q3 | Vote rules | 3 random candidates (not the last 2 courses) + "same course again", 15 s, plurality, random tie-break, no votes = random |
| Q4 | Does a player who left still count? | yes: a verified best stays in the standings and can make the podium |
| Q5 | Career-upgraded attributes allowed? | no: stock attributes for everyone (as today's online races) |
| Q6 | Ghost look | translucent (dithered) with the rider's own textures; no name tags (the original has none) |
| Q7 | The host's upload speed and CPU cores (sets the caps and the verifier workers) | measure it; until then capped at 50 players, 2 verifier workers |
| Q8 | Max session size | 50 at first, 100 after the load test |
| Q9 | Team / private sessions | not in v1; private sessions later through today's invite codes |
| Q10 | Restart | the PS2 pause menu's Restart; a one-button quick restart as a later, separate switch |
| Q11 | Your own best as a ghost (Trackmania shows it) | later: it needs a second solo simulation of your best replay (0.12-0.7 ms a tick) |

## 10. Not confirmed / open items

- O1: a solo race replay through the page's `?verify` (section 2.2).
- O2: a spectator camera on a remote rider (the camera is the local core's).
- O3: the PS2 online menus (section 5).
- O4: cloudflared's CPU and any Cloudflare limits for ~10-20 Mbit/s of WebSocket traffic.

## 11. Built

### TA-1: server sessions (2026-10-13), behind MP_SESSIONS=1

- **web/server/sessions.mjs** (new): rooms, the phase machine (load quorum 90 % / 45 s, window, overtime with the runs in
  progress, verify up to 15 s, podium 20 s, vote 15 s + 2 s result), the uploads (`POST /mp/session/run`) with records.mjs's
  tier-0 checks plus the session rules (the window's event, attributes 55 x 7, no computer riders outside the backcountry rival
  events, an announced attempt of this window started before its deadline, the run's length within the wall time since the attempt
  started, 6 uploads a minute), time and score ranking (records.mjs `better`), the podium from verified runs only (stale and
  unverifiable runs never count), standings deltas at most 2 Hz (top 10 + your row), the verifier queue / result endpoints
  (loopback + the records verifier's token) with priorities (could make the podium, top 10, the rest; then the best rank), players
  who leave keep their runs for the window and get them back on rejoin, token resume. The protocol is in the module header.
- **records.mjs:** the tier-0 checks moved into `checkRunFile` (same rules; submit calls it).
- **mp-server.mjs:** mounts sessions only with `MP_SESSIONS=1` (never fatal: no career.json or a failure leaves them off); 'ta-*'
  messages, `/mp/session/*` (503 when off), `/mp/status` `sessions`, a session member is not sent the lobby list, joining a lobby
  leaves the session and the other way round. Settings: `MP_TA_LIMIT_MS`, `MP_SESSION_MAX` (default 50), and `MP_SESSION_OPTIONS`
  (JSON phase timings, for tests).
- **web/test-mp-session.mjs** (new, in test:all): the phases on a fake clock, every upload check, time and score ranking, the queue
  order, verified / pulled results, the podium wait and its timeout, leave / rejoin / resume, the vote and the next window, and the
  mp-server mount end to end (off and on). test-records-server, test-mp, test-mp-gate, test-mp-plausibility and the other mp tests
  pass; a copy of web/server alone starts with MP_SESSIONS=1 (sessions off without the disc tables).

### TA-2: the verifier's session lane (2026-10-13)

- **web/server/records-verifier.mjs:** each cycle first runs at most `--session-batch` (4) runs from `/mp/session/verifier/queue`
  (best podium candidates first), under its own load bound `--session-max-load` (0.9) and `--session-timeout` (180 s), and while
  any online race or session is on. Then the records lane runs as before (its own busy rule, unchanged), so sessions never starve it.
  A server without sessions answers 404 / 503 and nothing changes. While a session is active, Chrome stays open on the page and the
  queue is checked every `--session-interval` seconds (3) instead of 60. startChrome retries once after 2 s (a start right after
  the last Chrome was killed on the same profile once never opened its DevTools port).
- **Page:** online-records.js `sessionReplay(id)`; online-replay.js `ui.cb.verifySessionRun` / `window.ssxVerifySession` (the same
  re-simulation as the records' ?verify), `ui.cb.soloLineupNext()` (the next event load prepares no computer riders: aiRace.fixedNext
  = {values: [], enabled: false}), `ui.cb.attributeOverride(bytes | null)` (a time-attack attempt's 55 x 7 bytes replace the
  profile's in career-ui play / startRun; the replay meta records what was applied).
- **O1 settled:** a solo race replay (empty lineup) verifies through the page's ?verify path: the lineup goes through
  prepareFixed -> prepareFreestyle without an opponent on record and on replay alike.
- **Measured** (this Mac, headless Chrome for Testing, Snow Jam, ~14,400 recorded ticks): a solo session run verifies in **3.3-8.5 s
  wall, 2.7-4.6 s of simulation** (0.19-0.32 ms a tick), 4-16 s of Chrome CPU, ~3.4 GB peak Chrome tree RSS; the records board's
  six-rider runs in the same suite took 18-26 s each. So one Chrome page verifies ~10-15 solo runs a minute.
- **Liveness** (the replay's path against the live ghost stream, section 2.3) waits for TA-4's stream.
- **web/test-session-verifier.mjs** (new, in test:all): a genuine solo run ridden in the page (no computer riders, 55 x 7), uploaded
  to a session with a faster claim on the same replay and a forged empty-pad replay beside it, and the same run on the records board:
  one `--once` pass verifies the genuine session run (standings: verified), pulls both forgeries, and verifies the records run.
  test-records-verifier passes unchanged.

### TA-3: the client (2026-10-13), pv onlineTimeAttack (off)

- **web/net/ta-game.js** (new): the page's controller, created by mp-game.js on the lobby connection and installed as
  `ui.timeAttack`. It follows the server's phases and drives the page's own Single Event flow. At load it starts the event with
  `ui.startSingleEvent` and arms no computer riders (`ui.cb.soloLineupNext`; the backcountry rival events keep their rival:
  prepareFixed would otherwise take the rival branch as Rival Time) and the maxed bytes (`ui.cb.attributeOverride`). It sends
  'ta-loaded' when the objectives card is up. `attemptStart()` lets an attempt start only inside the window and sends
  'ta-attempt'. `finish()` uploads a new best as its replay file (POST /mp/session/run, the reconnect token as credential) and
  sends 'ta-end' (restart / gave up / finish, uploading). The vote and the next window's event load are handled too. The screens'
  state is in `state` (session, roster, standings, you, results, votes, upload).
- **web/career-ui.js:** `play()` asks `ui.timeAttack.attemptStart()` in a session (a refused Continue keeps the objectives card);
  `finish()` hands a session attempt to `ui.timeAttack.finish()` and skips the records boards (time-attack runs, with maxed
  attributes, never go to the online or local top 5). Without `ui.timeAttack` both are unchanged.
- **web/net/mp-client.js:** every 'ta-*' message is its own event; `session(m)` sends one; `token`.
- **web/online-replay.js:** `soloLineupNext(false)` disarms the solo lineup (leaving a session); the results' Restart re-prepares
  the lineup (main.js heatLineup), so the controller re-arms it at every attempt start and finish.
- **No main.js / game-tick.js edits.**
- **web/test-ta-page.mjs** (new, in test:all; Chrome): the page creates a Snow Jam session and a node player joins. The page loads
  the event and reports it loaded. An attempt before the window is refused. A solo run is uploaded (kept) with the solo lineup and
  55 x 7, and the verifier verifies it. A given-up attempt does not upload. After the deadline Continue is refused. The podium shows
  the page's player. The vote's event (Super Pipe The Junction in the run) loads on the page for window 2. Leaving disarms.
  test-online-records, test-ctm-left, the career tests, test-fe-screens, test-mp, test-mp-session and test-mp-gate pass.
- **Not done yet:** the screens (TA-5, from the PS2's online screens); the session name shown is the rider's name (mp-game's
  hello), to become the Player Name with the screens; the in-race time left / standings HUD (with the screens).

### The PS2's online screens (decomp for TA-3 / TA-5)

FE.LUI and OV.LUI hold the original online UI (listed with tools/export_online_screens.py's header, which exports them). Their
set-up code is found from the screen name strings (SLUS_207.72): 230online_main_menu 0x1BFBB8, 231lobby_menu 0x1BCB58,
232CreateRoomPopup 0x1BE7D8, 235chat_room 0x1B1F88, 249Stats 0x1C2D18, 109MultiplayerStandings 0x24AE90 (next to
43final_standings 0x249C68). Plan: the session list from 231lobby_menu, create from 232CreateRoomPopup with the time limit as a
240challenge_popup list-box row (port-only), the session's players from 235chat_room, the standings from 249Stats (ten rows and the
slider) and 109MultiplayerStandings (the circuit standings: "Current standings", "Winner:", "Congratulations %s! You have won the
circuit."), the match card 102MPMatch (GL.LUI). Their behaviour (menus, focus, input) is to be traced from those functions before
each screen is built.

### TA-4: ghosts (2026-10-13), pv ghostRiders (off; needs onlineTimeAttack)

- **The pose packet as built** (web/server/ghost-packet.mjs, shared by the page and the server): 34 + 7 F + 4 B bytes. Every rigid
  bone is placed by forward kinematics from its parent's rotation and the rig's own `source_translation_cm` x the body scale: measured
  within 0.022 cm on Zoe, Moby and Snowballs, so the receiver needs nothing but its own copy of rider.json. The free bones, sent as
  positions from the root, are the rig's other roots (`board_rootg`, no parent), the board (`board_childg`) and any bone the sender's
  FK misses by more than 0.5 cm (the feet stretch up to 11 cm in some crashes). Over 800 packets of a scripted run (tricks,
  crashes): **156 B mean for Zoe (max 170), 160 for Moby, 155 for Snowballs; worst bone 0.19 cm, rotation 0.27 degrees**. The lite
  (25 B) and dot (12 B) entries and the bundle are as section 3.1.
- **Server** (sessions.mjs, mp-server.mjs): a session member's binary frame is a pose (size-checked, under the connection's rate
  limits) kept as its newest. Every 100 ms each recipient gets one bundle of what is due: the 6 nearest players' poses (10 Hz), the
  next 18's lite entries (4 Hz) and their pose every 2 s, everyone else's dot (1 Hz). Only news is sent: a pose supersedes that
  moment's lite and dot, and a ghost silent for 3 s is not sent. `/mp/status` `ghosts` counts the bytes in and out.
- **Page** (web/net/ta-ghosts.js, owned by ta-game.js): during an attempt the local rider's world pose goes out at 20 Hz. Bundles
  come in. Each frame every ghost is drawn 300 ms behind on the server clock (low 32 bits, wrap-safe), interpolated between its
  poses (never across a serial change), or its last pose moved to its newest lite / dot position; nearest first, at most 32.
- **Renderer** (web/ghost-riders.js, web/ghost-package.js): one geometry and texture set per package; one InstancedMesh per package
  texture; each instance's bone skin matrices (pose x scale x the rig's bind, plain floats: within 0.06 cm of the core's own palette)
  in one RGBA32F row; GPU skinning from rider.json source_skin (up to 4 bones). A 4 x 4 Bayer screen-door at 50 % opacity, the
  riders' unlit texels (a ps2-domain texel x 2) tinted pale blue, in the frame's colour space (frame-space.js toFrame); no lighting,
  shadow, effects or morphs. `gameplayOnly`, so the pause and menus hide them. The scene origin is read every frame (it moves with the
  course: main.js loadCourse). No main.js changes.
- **Budget, measured** (web/.ghost-budget.mjs, scratch: a solo Snow Jam Single Event, the real frame loop for 8 s per step, synthetic
  ghosts copying the rider's pose around it; this Mac, 1280 x 720):

| Browser | Ghosts asked / drawn | Frame interval p50 / p95 / p99 | Ghost CPU per frame |
|---|---|---|---|
| WebKit (webkit-driver) | 0 / 0 | 17 / 20 / 24 ms | 0.02 ms |
| WebKit | 20 / 20 | 17 / 19 / 27 ms | 0.12 ms |
| WebKit | 50 / 32 (cap) | 17 / 20 / 26 ms | 0.20 ms |
| Chrome (headless) | 0 / 0 | 16.7 / 16.8 / 16.8 ms | 0.01 ms |
| Chrome | 20 / 20 | 16.7 / 16.8 / 16.8 ms | 0.08 ms |
| Chrome | 50 / 32 (cap) | 16.7 / 16.8 / 16.8 ms | 0.18 ms |

  The frame stays on vsync in both: the ghosts' GPU time is not visible in the frame interval at this size, and their CPU is under
  0.2 ms a frame. Not measured yet: a phone-class device, and the frame with 100 players' bundles arriving (TA-6).
- **Tests:** test-mp-session (the tiers per recipient, superseding, staleness, a malformed pose); test-ta-page now runs with
  ghostRiders: the node player's ghost poses reach the page through the bundles and are drawn, and the page's own 27-bone poses
  reach the node player.
- **Not done:** the liveness check (section 2.3) needs the attempt tick in the pose packet (to compare with the replay's
  re-simulated path); kept for after TA-6.
