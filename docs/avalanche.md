# Avalanches and rock slides (recorded tumbler playback), 2026-09-28

Status (2026-09-28, core50): **core playback ported, bit-exact** (`engine/avalanche.hpp`, `web/avalanche_gameplay.inc`). The draw and the rumble loop are the rendering-sweep agent's (pv `avalanche`). Not ported yet: the trails' emitter points, the moving pieces' collision, and the save / restore. See "Port" below.

Retail SSX 3 does **not** simulate an avalanche. It plays back motion recorded into the location's SSB kind-22 record.
The debug recorder is `0x2D96E0` -> `0x2D9660` -> `0x2D66A0` (5.4 KB of particle physics, bounce, patch colour), then save
`0x2D83B8` / load `0x2D87D0`. It runs only with the tweak `gp+2372` set, which is 0 in retail, so **`0x2D66A0` is not needed**.

## Where

Each avalanche is started by a contact trigger (slot 2 of a volume -> builtin 94 id) or a LiveComp timer (slot 5).

| location | avalanche ids | trigger | pieces (group instances) | record |
|---|---|---|---|---|
| ABC1 Happiness | 10 .. 14 (5 avalanches, 46 groups) | ava2Trig_1000/1001 (12), ava2Timer_1000 (13), snowfield_1000 timer (10) | ava1boulderA/B, ava1bitA/B, ava2powder, ava2mini, ava2shrub, ava2Tumbler ... | 88 KB |
| DBC2 Ruthless | 71 | rockslideTrig_2000 | rock slide (8 groups) | 24 KB |
| DRA4 | 20, 21 | crumbleLip_1000 / _2000 | 14 groups | 13 KB |
| ERA5 Gravitude | 28 | ava1Trig_1000 | ava1node_1000..1007 (emitter carriers) | 10 KB |
| ESS3 Kick Doubt | 58 | ava2Trig_1000 | 7 groups | 9 KB |
| EBA3 Much 2 Much | 45 | rockslideTrig_1000 | rockslide_1000..1004 (type 2, stay down), rockslide_2005..2007 (dust trails) | 20 KB |
| EBC3 The Throne | 71 | ava1Trig_1000 | 5 groups | 6 KB |

PS2 evidence (memory of the kept states, active slot table `0x538938`):
- ERA5 `peak3/gravitude-full`: avalanche 28 at tick 2019 (t 0.19) and 2418 (t 3.78, the nodes fading: scale 0.469, alpha 0.22,
  sample 113 of 120).
- EBA3 `peak3/much-2-much-full`: 45 active from tick 820 (t 2.52) to 2819 (t 20.5). The type-2 rocks keep the slot alive.
- ABC1 `setpieces-abc1/full`: none of the 33 states (the capture's line never touches a trigger).
- In the swept frames the pieces were off screen: the effect shows only where the rider sets it off.

## Data

### Builtin 93 `0x3059A0`: define groups

- Defaults block `0x4FBD90` (50 words), expected types `0x4466D8`.
- Keys:
  - 0 = avalanche id;
  - for k = 0..7: 1+k group instance (resource, -1 none); 9+k type (int); 17+k duration (s, default 10); 25+k fade-in (s, 0);
    33+k fade-out (s, 3); 41+k speed factor (1).
- Each valid k calls `0x2D92D0(id, instance, type, dur, fin, fout, speed)`.

**Avalanche record** (0x210 bytes, `0x317D70` tag `0x487568`). The list head is `gp+2496`, newest first.
- +0 id; +4 group list; +8 next.
- u16 +12 event count; u16 +14 sound count.
- +16 events {f32 t, instance*} x n12.
- +272 sounds {u16 tick, u16 tumbler index} x n14.

**Group record** (0x120 bytes, tag `0x487578`). The list is linked at +244, newest first.
- +0..+0xD7 the builtin-96 emitter parameters.
- +224 start position (vec4, w = 1); +240 u16 type: key 2 -> 0, 5 -> 2 (persistent), else 1; +242 has emitter; +243 owns +252.
- +248 instance; +252 sample pointer.
- +256 dur x 30, +260 fin x 30, +264 fout x 30 (EE mul, round to zero); +268 speed.
- +272 the instance's largest bounds extent x 0.2 (only the recorder reads it); +276 tumbler.
- Global table `0x537C50[gp+2512++]` = group. The instance's u16 +146 |= (index << 8) | 2. `gp+2508` = 0 (re-parse).

### Builtin 96 `0x305F40`: emitter parameters

- It is the builtin-16 particle block (0xE0 bytes, defaults `0x4FBE58`, types `0x4467A0`): NumParticles 1, Duration -1, Damp 1,
  Size 4, colours 1, TextureId 16, NumFlipTextures 1, FlipTextureRate 20, Instance (key 54) -1.
- `0x2D9538(instance, block)` copies 0xD8 bytes into the group +0 and sets +242 = 1 (not for type 2).
- `engine/set_piece_particles.hpp originalParticleDefaults(16)` / `originalParticleApplyArgument` parse the same block.

### Builtin 95 `0x305D90`: AvaSpline

- Key 0 = instance (-1 current). It must have an entity (Object / LiveComp, vt+0x84 accepts modifiers).
- `0x355A78` -> ctor `0x357798` (vtable `0x48F338`) -> attach `0x3554B0`. Modifier +128 = instance, +64 = matrix (from the instance).
- Update +0x14 = `0x357820` -> `0x2D1CF0` -> `0x2D9C00(instance, matrix)`: the tumbler whose group +248 is this instance gives
  **matrix = tumbler rotation (+112) x scale (+176), translation = tumbler +96**. With no tumbler the matrix is unchanged, so the
  piece stays at its authored place until triggered.

### Builtin 94 `0x305C88`: trigger

Key 0 = id -> `0x2D97A8(id)`, when `gp+2516` is 0 (the record's magic was good).

### Kind 22: the recording

- Resolver case 22 -> `0x2D9FB8`: record pointer `gp+2500`. The magic `0x02BEEF00` must match, else `gp+2516` = 1.
- `0x2DA028` parses it once definitions exist (`gp+2496`) and `gp+2508` is 0, then sets `gp+2508` = 1. Every `0x2D92D0` resets
  `gp+2508`, so the last parse sees every definition.
- The definitions are global stage handlers: ABC1 programs 63 and 87, ERA5 118, EBA3 74.
- Layout from +8, in list order:
  - per avalanche (newest first), per group (newest first): {u16 `0xBEEF`, u16 size = n x 10 + 12, f32[3] start, n = trunc(dur x 30)
    samples of 10 bytes};
  - then u16 n12, u16 n14, n12 x {f32 t, u32 instance resource}, n14 x {u16 tumbler, u16 tick}.
- The parse zeroes +12, so the recorded instance events are **dead** in retail.

**Sample** (10 bytes, read by `0x2D5778`):
- s8[3] position step: the integrated position += 2 x step;
- u8[3] colour: x 1/128 x `gp+2488` (1.0), lerped with the next sample; used for the emitter;
- s8[3] rotation axis x 1/127, normalised (kept as is when its length is 0);
- u8 angle x 0.0082133 rad.
- Orientation: M = M x Rot(axis, angle) per sample (`0x31BE50` then the 4x4 product).

## Core playback (physics agent)

State:
- 16 slots x 28 bytes at `0x538938`: +0 avalanche, +4 tumbler list, +8 tumbler array (built by `0x2D7E60`), +16 event cursor,
  +20 sound cursor, +24 time t.
- A pool of 64 tumblers x 752 bytes at `0x4EE770`:
  - +0 sample index; +16 integrated position; +32 / +112 rotation (identity at start `0x4FF1A0`); +96 position; +176 scale;
    +180 alpha; +192 colour;
  - +208 emitter; +480 ambient colour (`0x2EE7C8` / `0x2EE810` / `0x2EE858`); +720 = 1; +736 group; +740 link; +748.

Update: `0x2D8948`, the PathArrow environment component (vtable `0x488648` +0x14; component 16 of `0x2F0548`). It runs from the
environment update `0x2F0A98` in entity group 3 (`0x354F98(mgr, 3)` at `0x230CE8`). When `gp+2368` (1), parsed (`gp+2508`) and
Update (`gp+2416` = 1), it calls `0x2D7EF8(slot)` for each of the 16 slots.

**Trigger `0x2D97A8(id)`:**
1. Parse if needed. Find the avalanche. If it already has a slot, do nothing. Take the first free slot.
2. Per group: take a free tumbler (+736 = 0). Set +736 = group, group +276 = tumbler, link it, +0 = 0, +16 = group start,
   rotations identity, +96 = instance +64, +192 = (1, 1, 1, 1), `0x2D7C00(tumbler, 0)`.
3. If the group has an emitter: `0x371600(+208)`, `0x370DC8(+208, 2.0, group params)`, +720 = 1, +480 = ambient.
4. If more than 64 tumblers are needed, release all and fail.
5. Then `0x2D7E60`, `0x29DEF0(audio, 1)` (the avalanche loop), and counter `*(*(*(game+0x84)+0xC)+0xA8)+1800` += 1.

**Per tick `0x2D7EF8(slot)`:**
- t += `gp+2420` = **0.009**.
- Each tumbler, `0x2D7CA8(tumbler, t)`:
  - t' = t x speed;
  - `0x2D5778(out, tumbler, t')`: s = trunc(t' x 30); clamp s <= dur - 2 with fraction 1.0. Integrate from +0 up to s:
    position, rotation, and, with +748, an emitter point per sample through `0x3717C0` (dt 1/60). Then out = +16 + 2 x step[s] x
    fraction, plus the lerped colour.
  - While t' x 30 < dur: +96 = position, +112 = rotation, +192 = colour. For emitter groups (not type 2), +508 = +180 and
    `0x3717C0(+208, point, colour, t' < dur, 1/60)`.
  - Otherwise (not type 2): release through `0x2D7DD8` (entity vt+0x08(3): the piece goes, emitter cleared). Type 2 stays at its
    last pose.
- Envelope `0x2D7C00`:
  - s = t' x 30. If s < fin: scale 1, alpha s / fin. Else if s < dur - fout: 1 / 1.
  - Else (not type 2): alpha = (dur - s) / fout, scale = sqrt(alpha) when alpha > 0.
- Sounds: while t x 30 >= sound.tick, `0x29E560(audio, tumbler[index] +96)`.
- The dead events list is never used.
- When no tumbler is alive: `0x2D81B0` (release all, `0x29DEF0(audio, 0)`, counter -= 1).

Also:
- Save / restore: `0x2D9CB0` / `0x2D9D68`, from `0x26D818` / `0x26DBF0`.
- Camera shake on the triggers is builtin 91 (below).

Export for the core (a runtime asset; the core reads it as is, so keep the field set and the list order):
`python3 tools/export_avalanches.py [--location LOC] [--out DIR]`
- Output: `<DIR>/<LOC>/avalanches.json` (event package, served as `/assets/<LOC>/avalanches.json`) and the same file at
  `<DIR>/PEAK<n>/<LOC>/avalanches.json` beside the streamed location's other files (PEAK1/ABC1, PEAK2/DBC2 and DRA4, PEAK3/ERA5,
  ESS3, EBA3, EBC3; the MOUNTAIN world's locations use these PEAK roots). Seven locations: ABC1, DBC2, DRA4, EBA3, EBC3, ERA5, ESS3.
- Schema (every float is its raw f32 bits as an unsigned integer, as the PS2 stores it):
  - `location`, `source`, `record_sha256` / `record_size` (the kind-22 record);
  - `avalanches[]` in list order (the trigger's search order): `id` (builtin 93 key 0 = builtin 94 key 0), `groups[]`,
    `dead_events[]` (`time` bits, `instance`: parsed, never used by retail), `sounds[]` (`tumbler` = index into this avalanche's
    groups, `tick`: `0x29E560` when t x 30 >= tick);
  - `groups[]` in list order (tumbler k = group k, 0x2D97A8): `resource` (the instance, packed id), `name`, `program` (the global
    handler that defined it), `type` (0 key 2, 2 key 5 = persistent, else 1), `duration` / `fade_in` / `fade_out` (x 30, EE
    round-to-zero; group +256 / +260 / +264), `speed` (+268), `extent_cm` (the instance bounds' extents; +272 is 0.2 x the largest,
    recorder only), `start` (3 words, +224), `samples` (hex, 10 bytes each, n = trunc(duration); layout above), `emitter` (null, or
    the 54-word builtin-96 block = group +0..+0xD7), `ava_spline` (a builtin-95 AvaSpline follows this group's tumbler), and
    `record_offset` (bytes into the record, for checks).
- The core playback (physics agent, 2026-09-28) reads this unchanged: bit-exact over 11 PS2 states, 4026 ticks.
- Checked against PS2 memory with `--check-states "EBA3:local/ps2-capture/runs/peak3/much-2-much-full.tick*.p2s" --check-states
  "ERA5:local/ps2-capture/runs/peak3/gravitude-full.tick*.p2s"`:
  - 56 and 232 group records equal to the PS2 (words, record bytes at +252, list order);
  - 49 live tumbler positions, sample index and position bit-exact (EE round-to-zero integration).
- The output goes to the scratchpad; the coordinator copies it into `web/public/assets` (it is disc data).

## Draw (JS, pv `avalanche`, off; web/avalanche-state.js)

**Built (2026-09-28), waiting for the live core's exports to be switched on:**
- **Batches:** `web/prepare.py` puts every `ava_spline` group instance into moving_resources, so each piece has its own batches
  (`moving_resource`; the pieces hidden at the start keep `hidden_resource` too). Re-split packages of the 7 locations: same
  triangles, pixel-identical renders (docs/visual-parity.md 41). The streamed packages get the same split from the CTM agent
  (tools/export_peak_world.py).
- **Matrices:** the core emits the followers through `moving_instances()` (key = resource, the three.js delta while a tumbler
  drives the piece), so `web/moving-instances.js` moves them.
- **Visibility** (`createAvalancheDraw`, from `web/set-pieces-renderer.js`): a piece drawn at the start (countdown audit 'static')
  stays until released; a piece hidden at the start (runtime flag bit 0 clear: ERA5's 8, DRA4's 14, 24 of ABC1's) shows from the
  trigger (its builtin 0 gives it an entity; it is then in `avalanche_pieces()`) until released; released (0x2D7DD8, entity
  vt+0x08(3)) = gone until a new race.
- **Audio:** `web/audio-world.js avalanche()` from `web/game-audio.js` each tick (section "Audio").
- **Snapshot:** `avalancheState(core)` reads `avalanche_pieces()` / `avalanche_sounds()` once per core tick (the pieces export drains
  its released list); `web/test-avalanche-state.mjs` checks the volume formula, the reader and the visibility rule.

The original plan (kept for the trails):

1. **Pieces:** the group instances with a builtin-95 AvaSpline follow their tumbler matrix (rotation x scale, position).
   - The core should export these as moving-instance deltas (as `moving_instances()`), so `web/moving-instances.js` moves the
     authored batches.
   - A released type-1 piece is hidden (entity state 3).
   - Before its trigger a piece draws at its authored place, as now.
2. **Trails:** `0x2D9130` (vt +0x1C, drawn when `gp+2368 && gp+2508`, or the debug `gp+4348`) sets material priority 7 (word2
   `|= 0xE0`: after the fog composite) and texture -1.
   - It then runs `0x2D8EA8(slot)` for each slot: `0x371688(emitter, 7)` for each tumbler with an emitter.
   - This is the colour-emitter draw of the rider snow (renderer +0x2A4 `0x380CE0`), with the per-point terrain colour from the
     samples.
   - Reuse `web/snow-renderer.js` / `web/set-piece-particles.js` with a core export of the emitter rings. Convert the colours with
     `toFrame` (`web/frame-space.js`).
   - The debug lines `0x2D8A00` (`gp+2408` RenderNLines = 0) are not drawn.

## Audio (traced 2026-09-28, JS with the draw: web/audio-world.js)

**Only the loop plays.** The per-tumbler sound events (`sounds[]`: `0x2D7EF8` at `0x2D811C`, and the dead instance events at
`0x2D8098`) call `0x29E560(audio, position)`, which is `jr ra` (an empty stub, the same in PS2 RAM of the ERA5 and ABC1 states). So
retail plays no per-tumbler sound; the `sounds[]` list is silent.

**The rumble loop `0x29DEF0(audio, on)`** (from the trigger `0x2D97A8` at `0x2D99F4` with 1, from `0x2D81B0` at `0x2D8218` with 0):
- A count at audio+0x6040: +1 on / -1 off; the voice starts when it goes 0 -> 1 and stops when it returns to 0 (several avalanches
  at once share one loop).
- Start: the listener = the leading local human rider (`0x285D98(audio, 0x288AE0() ? 0 : -1)`: rider +0x874 && +0x87C, lowest
  +0xEC), its index kept at audio+0x6070; its position = rider +0x6C0 object vt+0x28.
- `0x2DA1C0(out, listener)` over every tumbler of every active slot (+736 != 0): centroid of the tumbler positions (+96),
  average scale (+176), minimum distance to the listener (cm; the `< 225000000` test compares the distance, not its square, so it
  never rejects), count, any-not-persistent flag. Kept at audio+0x6050 (the voice's position pointer).
- Volume `0x29E438`: v = trunc((100 - d_min x 0.01) x 1.27 x scale_avg x 8.466667), clamped to 0..127 (127 up to about 88 m at
  scale 1, 0 at 100 m).
- The voice (`0x2906B8`): bank slot 8 (the location's first kind-20 bank, `<LOC>_slot8.bnk`), sound 2, bus 5, positional at the
  centroid, distance parameters `0x2A9988(100, -1)` (mVanish 100 m), loop flag set, per-voice update `0x29E4A0` (pointer-to-member at
  0x4A36D0): every audio update the centroid, the average scale and the minimum distance to the kept rider are recomputed and the
  volume written to the voice (+0x64). Handle at audio+0x5FF4.
- Stop: `0x2AD5F0(queue, handle, 2.0, 1)` (2 s fade), the update callback removed, audio+0x6030..0x6038 = -1.
- Bank check: sound 2 of every avalanche location's slot-8 bank (ABC1, DBC2, DRA4, EBA3, EBC3, ERA5, ESS3) is the same looped
  MicroTalk patch (22050 Hz, 160813 samples, loop 2144..157477): the rumble.

## Related stage builtins (traced 2026-09-28)

- **91 `0x3050F0`, camera shake.**
  - Keys: 0 (skip if not 0), 1 use instance (1), 2 instance (-1), 3 distance factor (1).
  - For each local rider with a camera (rider+0x87C): `0x15E360(camera, type gp+0xAE8 = 0, 1.0, |rider+0x110 - P| x key3)`.
    Type 0 = period 0.05, random 0.01, amplitude 0.4, radius 20000 cm: full strength within 100 m, zero at 200 m.
  - Used 132 times in slot-5 LiveComp windows: ABC1 22, CRA3 9, DRA4 18, DBC2 31, ERA5 4, ESS3 20, EBA3 1, EBC3 27.
  - The port returns nil. `engine/original_camera.hpp` already has the shake. Also, its `requestShake` sets pending only when
    the amplitude grows; the PS2 sets it on every call that is not suppressed (`0x15E448`), so at amplitude 0 the port skips the
    PS2's 12 visual-RNG start draws.
- **92 `0x305660`, scripted lightning.**
  - For a human contact: `0x390EC8(0)`, gated by `gp+0x151C`. The flash counter runs 14 ticks with the ScreenTint colours;
    thunder follows at once.
  - Used by DRA4's 18 treefalltriggers and EBA3 speakertower_breaktriggera_1000, both behind `if b77(0, 100) < 66`.
  - Core one-liner: `case 92: weather_lightning_strike(0)` for a human. `web/screen-tint.js` already draws it.
- **35 `0x300B20`, rail group off / on.**
  - `0x358B28(id, flag)` on the RailMan `gp+0xF50`: off removes the group's rails from the octree (`0x358780`), on re-inserts them
    (`0x358998`).
  - ABC1 fallingtreea trunk rails (slot 1 off, slot 5 on at the fall, then builtin 48 binds them to the tree). DRA4
    treefallrailg / i (off at slot 1, on inside the 66 % gate).
  - The port keeps all 14 rails live: grindable before the tree falls. Gameplay, not drawn.
- **74 `0x303F80`, tunnel.** Rider+0x3FC = 1 (cleared every tick by `0x120F20`). `0x2ED490` eases the environment selector
  (x 0.9 + 0.1), and the rider lighting bank blends toward the tunnel bank. DBC2 tunnelvolume_1000..1002. Subtle (DPTN1 = DPDK1).
- **101 `0x306438`, hazard sphere.** Its only consumer is an empty stub (`0x11A0C0`). The port's no-op is right.

### Port status of these builtins (2026-09-28, core43)

- **91, ported.** `web/stage_script_gameplay.inc` case 91 queues `request_stage_camera_shake(type 0, 1.0, fade)` for this core's human (`web/core.cpp`), applied in call order after the crash request and before the camera update. Fade is `|rider+0x110 - P| x key3`, where P is the translation of the entity matrix (`stage_entity_matrix`) of the key2 instance, or of ctx+0x290 (30A688 sets it to the slot-5 instance). With no instance, only the contacting rider shakes, at fade 0.
  - **`requestShake` fix** (`engine/original_camera.hpp`): +0x458 pending is set on every call that is not suppressed.
    - PS2 check, new gate `cam-boost-slow`: Snow Jam glide, Tricky poked, braked to 1.7 km/h, then Square. 15E460 requests shakes 1..3 at scale 0 (amplitude 0) every tick; the PS2 records `shakeWasPending` 1 at index 0 from 649, and the shake start draws its 12 visual-RNG words.
    - Camera words exact on 599/599 ticks with core43; the old port fails at 650.
  - **The Throne** (peak3/the-throne-tuck): the PS2's only index-0 run in the comparer window is 4111..4138 (programs 59/60, 0x422A / 0x5E22A at 23..57 m), and the port fires on exactly those ticks.
    - The port also fires 2405..2503 (program 20, LiveComp 0x5942A started by trigger 0x5C22A, program 19). In the PS2 that LiveComp was already done (time 4.4333 = its end, previous / unclamped 1e10 at 2403..2450, savestates `runs/shake/throne.tick*.p2s`).
    - A computer rider hit the trigger first. The solo comparer has no computer riders, so this is a harness limit, not a builtin-91 difference.
- **92, ported.** A strike at distance 0 (`weather_lightning_strike(0)`) when the current player is this core's human, under the same gate as the ScreenTint update (`stageLightningFinishAge < 3`, the port's gp+0x151C).
- **35, ported** (`web/rail_dynamic.inc` `browser_rail_group`): the RailMan's 128 slots.
  - Off: slot the id and take the rail out of the static layers (`static_record_bound`). A slotted id is not removed again.
  - On: re-insert only a slotted id and free its slot.
  - Handlers: DRA4 0x7CE20 (slot 1 off / slot 2 on: 0x4020..0x4220) and 0x81F20 (0x4320..0x4520). ABC1 has eight trunk rails (0x4706..0x4E06: slot 1 off, slot 5 on).
  - PS2 dra4-full: the RailMan is created between 9218 and 9618 and its slots are empty from 9618 (off, then on).
  - Open: the re-insertion goes to the head of the octree lists (328C20); the port keeps the original walk order.
  - Streamed worlds (core47): the static copy is rebuilt after every appended catalog (`browser_static_records_rebuild`), leaving out the grouped-off and bound rails. Before, the copy was dropped at every append, so a grouped-off or RailModifier-bound rail answered statically again as soon as another location's rails came in.
- **74, not ported** (low priority).
  - 2ED490 eases the per-rider selector: `x 0.9`, `+ 0.1` while rider+0x3FC.
  - `environment_bridge.cpp` passes selector 0.
  - Porting it needs the port's tick order between race_end (121818, where builtin 74 would set the flag) and the trail / environment update that reads it.

## Port (2026-09-28, core50)

- **Engine** `engine/avalanche.hpp`, instruction by instruction:
  - the trigger 0x2D97A8 (pool search from the first free tumbler, list head insertion, the 0x2D7E60 array);
  - the per-tick 0x2D7EF8;
  - the tumbler update 0x2D7CA8;
  - the integration 0x2D5778: s8 x 2 steps, per sample an axis-angle matrix from s8 x 1/127 and u8 x 0.0082133 applied as `new[i] = R x old[i]`, then the partial step, the lerped colour and the partial rotation (u8 x 2.0944 x frac x 1/255);
  - the envelope 0x2D7C00, release 0x2D7DD8, release-all 0x2D81B0, and the 0x2D9C00 matrix.
- **Emitter catch-up not ported.** The emitter points 0x2D5778 adds per skipped sample (when +748) are not ported. They only feed the emitter at +208 and never touch the tumbler state.
- **Rotation row 3.** Row 3 of the per-sample matrices is (0, 0, 0, 1). The original takes it from a stack temp that holds 0x4FF130 at that point, and the PS2 states show (0, 0, 0, 1).
- **Oracle** `tests/avalanche_live.cpp` / `tools/test_avalanche_live.py`: the recompiled original (0x2D7EF8, 0x2D97A8, 0x2D9C00; the sound calls return at once) against the engine, built from the savestate's own lists, slot table and pool.
  - States: EBA3 much-2-much-full 420 (trigger 45), 820..2819 (running); ERA5 gravitude-full 1619 / 2818 (trigger 28), 2019 / 2418 (running, through the fade-out and release).
  - Result: 4026 ticks, every tumbler field and every slot bit for bit, 25817 0x2D9C00 matrices, 0 failures.
- **Core** `web/avalanche_gameplay.inc`:
  - `init_avalanches(text, append)` reads `<location root>/avalanches.json`. `web/avalanche-load.js` does the loading from `set-pieces-renderer.js` (event courses), `peak-set-pieces.js` (every location of a streamed world's peak.json) and the comparers (`stage-world-compare.mjs`, `peak-capture.mjs`).
  - Builtin 94 triggers; the new tumblers' +96 is the instance matrix's translation row.
  - The per-tick step runs at the end of `advance_world_entities` (race_begin): group 3 after the entity pass. A slot-2 trigger (race_end of tick N) and a slot-5 one (the entity pass) both take their first step before the rider of N+1.
  - Resets with the stage scripts at a new race; the definitions stay with the world.
- **Page-flow check:** new capture `runs/avalanche/eba3-slots` (the much-2-much-event-tuck script, watching the slot table 0x538938 and pool tumbler 0), compared through the comparer.
  - The trigger is at record 541 on both.
  - Slot t and the sound cursor are equal on all 878 compared ticks.
  - Tumbler 0's 0x2D9C00 matrix and alpha are equal on all 555 ticks up to its release (1096).
  - The ERA5 run (`era5-slots`) cannot be compared solo: a computer rider sets avalanche 28 off at 1998, while the solo port reaches the trigger at 2587.
- **Exports:**
  - `moving_instances()` carries every AvaSpline follower a tumbler drives: key = resource, the draw delta of the 0x2D9C00 matrix against the authored instance matrix.
  - `avalanche_pieces()` gives the 0x2DA1C0 set (active slots, list order): resource, state (1 following, 2 type-2 at rest), follower, emitter, the 0x2D9C00 matrix, alpha, scale. It is followed by the instances released since the last call (entity vt+0x08(3): hidden) and the loop refcount with its changes (0x29DEF0, audio+0x6040). The per-tumbler sound cues are dead (0x29E560 is `jr ra`); only their cursor advances.
  - `avalanche_info()` (QA: definitions, triggers, ticks, slots) and `avalanche_trigger_qa(id)`.
- **Open:**
  - **Trails:** the emitter init 0x371600 / 0x370DC8 draws the presentation stream (0x3177F0), plus the per-sample points and 0x3717C0 / 0x2D8EA8 / 0x371688.
  - **Moving pieces' collision.** The type-3 rocks carry flags 0x40210000 and are on the static route 0x20 before the trigger (PS2 EBA3: 0x40214123), then on the entity route 0x40 with the Object entity after it (0x40214145). The port still leaves these instances unsupported, so they have no collision at all, as before.
  - **Save / restore** 0x2D9CB0 / 0x2D9D68.
  - **Computer-rider cores** load no definitions yet (their contexts), so a computer rider's own trigger only plays through the human core's replay of the contact.
  - **Duplicate ids:** DBC2 and EBC3 both use id 71. In MOUNTAIN the first definition in list order wins, which is the later-loaded location.

