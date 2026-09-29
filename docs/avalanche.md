# Avalanches and rock slides (recorded tumbler playback), 2026-09-28

Status (2026-09-28, core59): **core playback, the moving pieces' collision and the trails emitters ported, bit-exact** (`engine/avalanche.hpp`, `web/avalanche_gameplay.inc`). The draw and the rumble loop are the rendering-sweep agent's (pv `avalanche`). Not ported yet: the save / restore's replay hookup (engine port only). See "Port" and "Collision" below.

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

Update: `0x2D8948`, the PathArrow environment component (vtable `0x488648` +0x14; component 16 of `0x2F0548`), run from the
environment update `0x2F0A98`. (Earlier notes put it in entity group 3. The capture records say otherwise: at the provider exit of
a tick the slot table already holds that tick's step while the AvaSpline +0x40, updated in entity group 1, holds the previous
one, so the step runs after group 1 and before the riders. `web/avalanche_gameplay.inc` runs it there, in race_begin.) When `gp+2368` (1), parsed (`gp+2508`) and
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

**Built and checked against the PS2 (2026-09-28, core50):**
- **The draw rule, from the code:** the trigger program's builtin 0 (2FC0D0 -> 356DB0) gives each piece its Object entity and sets
  instance flags |= 4 (the entity draw 0x356298 tests instance+8 & 4) when its key 2 != 0 or the piece was drawn statically
  ((flags & 3) == 3). So static pieces stay drawn while they tumble; hidden pieces are drawn only with key 2 (`ENTITY_DRAWN`:
  ABC1's 16 ava2mini rocks, DRA4's 14 crumbleLip pieces; `tools/export_avalanches.py --entity-drawn`); ABC1's 8 ava2powder and
  ERA5's 8 ava1node pieces carry the emitters and are never drawn (PS2 gravitude-full 2019 / 2418: flags 0x42 / 0x342 with an
  entity, bit 2 clear; after the release 0x302 / 0x102, no entity).
- **Poses (EBA3 much-2-much-full 820 / 1219 / 1620 / 2019 / 2419 / 2819, Chrome and WebKit):** the page stepped to the PS2 slot's
  own t (2.52 .. 20.5102, equal to 4 digits), each of the 5 rockslide pieces' drawn delta against native(0x2D9C00 matrix from the
  PS2 tumbler +112 x +176, +96) x native(instance +0x10)^-1: rotation identical, translation within 0.1 mm; all 5 drawn where the
  PS2 has flags & 4 and an entity. (The instance matrix has unit rows: 37E238 applies the uniform scale on both paths.)
- **Tumble and release (QA trigger, Chrome and WebKit):** DRA4 20 (7 hidden key-2 pieces shown and moved, gone at the release,
  none with the switch off), ABC1 14 (16 rocks shown, moved, gone at 1038 ticks), ABC1 10 (8 static boulders moved, gone), ABC1 12
  (the powder nodes never drawn). EBA3's pieces are type 2: at rest at their last pose, the slot and the loop live on (PS2 2819:
  refcount 1).
- **Rumble:** the voice `avalanche:8/2` starts with the trigger and stops at the release (Chrome; the WebKit harness cannot unlock
  audio, same logic). Against PS2 RAM (audio +0x6040 refcount, +0x6050 centroid, +0x6060 scale, +0x6064 min distance): EBA3 820
  volume 127 (73.5 m) and 2819 volume 0 (302.8 m), the same centroid at 2819; ERA5 2019 / 2418 volume 0.
- **ABC1 2000 / 7200 frames:** no avalanche is playing there (PS2 kept states: no slot); the static pieces draw the same with the
  switch on and off.
- **After the release (2026-09-28, the physics agent's question):** the release (2D7DD8 -> vt+0x08(3) -> 34FBF0) destroys the
  entity and restores `flags = (flags & 0xFFFF0300) | (flags >> 16) | 2`, i.e. the authored low bits, with 0x100 still set.
  - The static collectors `0x22A5A0` / `0x229FC8` test only `(flags & 3) == 3`, the location (+0x7D, table +36 == 6) and chunk
    (+0x7E, +1008 == 3) residency and the VU0 frustum on +0x50; `0x22C078` draws what they list (the visibility context on the
    stack, sp+320 of 0x22BBF8: +8348 count, +8352 list) through renderer +0x300 (37E238) at the instance's own matrix. 0x100 is only
    the entity draw's list (1032C0 / 0x356298), not a test here.
  - PS2 RAM: EBA3's 5 type-2 rocks sit at exactly 0x40214123 (0x100 set, no entity) at much-2-much-full 420, and they are in that
    frame's static list (context 0x1FF7A00, 69 entries: 6..9 and 68); the 0x102 dust-trail groups are not. DRA4 20 after its
    release (dra4-full 2818 on): 0x40004302, bit 0 clear, not listed. The instance +0x10 matrix and +0x50 sphere are never written
    by the AvaSpline (EBA3 820 .. 2819 equal to the countdown).
  - So a piece drawn at the start comes back **at its authored place** after the release: ABC1 10 / 11 / 13 (22 pieces), DBC2 71's 5
    type-1 pieces, ESS3 58's 7 (they shrink to scale sqrt(alpha) -> 0 over the fade-out, then pop back up the slope). A piece hidden
    at the start (authored bit 0 clear: ABC1 12 / 14, DRA4, ERA5) stays hidden. Type-2 pieces (DBC2 3, EBA3 5, EBC3 5) are never
    released in a race.
  - Page: `createAvalancheDraw` keeps the static pieces drawn after the release; `moving_instances()` drops their delta, so
    `web/moving-instances.js` puts them back at the authored matrix. QA trigger ABC1 10 (Chrome, WebKit): 22 moving batches shown
    before, during and after the release, none moved after it.
  - Not seen in a PS2 frame yet: no kept capture releases a static piece in view (no ABC1 / DBC2 / ESS3 run touches its trigger).
- **core56 poses (Chrome):** the draw now follows the AvaSpline +0x40 matrix (the Object's draw matrix, one record behind the
  tumbler). EBA3 much-2-much-full 820 .. 2819: each rock's drawn delta against native(PS2 AvaSpline +0x40 via entity +0x1C) x
  native(instance +0x10)^-1: rotation within 1e-4, translation within 0.9 mm at all six states (while tumbling, 820 .. 1620, the
  tumbler's own 2D9C00 matrix is up to 0.34 away in rotation: the lag). WebKit and the ABC1 release re-check are pending: on
  core56 the ABC1 QA flow finds no definitions at the start (`avalanche_info` [0, 0, 0, 0]; core50 had 5).

**Implementation:**
- **Batches:** `web/prepare.py` puts every `ava_spline` group instance into moving_resources, so each piece has its own batches
  (`moving_resource`; the pieces hidden at the start keep `hidden_resource` too). Re-split packages of the 7 locations: same
  triangles, pixel-identical renders (docs/visual-parity.md 41). The streamed packages get the same split from the CTM agent
  (tools/export_peak_world.py).
- **Matrices:** the core emits the followers through `moving_instances()` (key = resource, the three.js delta while a tumbler
  drives the piece), so `web/moving-instances.js` moves them.
- **Visibility** (`createAvalancheDraw`, from `web/set-pieces-renderer.js`): a piece drawn at the start (countdown audit 'static')
  is always drawn (tumbling, then statically at its authored place after the release); a piece hidden at the start (runtime flag
  bit 0 clear: ERA5's 8, DRA4's 14, 24 of ABC1's) shows from the trigger (its builtin 0 gives it an entity; it is then in
  `avalanche_pieces()`) until released (0x2D7DD8, entity vt+0x08(3)), then stays hidden until a new race.
- **Audio:** `web/audio-world.js avalanche()` from `web/game-audio.js` each tick (section "Audio").
- **Snapshot:** `avalancheState(core)` reads `avalanche_pieces()` / `avalanche_sounds()` once per core tick (the pieces export drains
  its released list); `web/test-avalanche-state.mjs` checks the volume formula, the reader and the visibility rule.

The original plan (kept for the trails):

1. **Pieces:** the group instances with a builtin-95 AvaSpline follow their tumbler matrix (rotation x scale, position).
   - The core should export these as moving-instance deltas (as `moving_instances()`), so `web/moving-instances.js` moves the
     authored batches.
   - A released type-1 piece loses its entity; it is drawn statically again only when its authored flags have bit 0 (see "After
     the release").
   - Before its trigger a piece draws at its authored place, as now.
2. **Trails** (done: "Trails" below, pv `avalancheTrails`): `0x2D9130` (vt +0x1C, drawn when `gp+2368 && gp+2508`, or the debug `gp+4348`) sets material priority 7 (word2
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
- **74, ported** (core60).
  - 0x303F80: the current player's rider (ctx+0) +0x3FC = 1. The rider manager clears it at 120F20 (the start of the rider pass). After the stage triggers (121818), 1218D0 -> 2ED490 eases the rider's environment block selector (0x4FA370 + i x 0xF0, +0x24): `x gp-0x3974 (0.9)`, then `+ gp-0x3970 (0.1)` while +0x3FC is set. Above 0.1 the rider irradiance takes the alternate bank.
  - Port:
    - `stageRiderTunnel` (web/stage_world.inc), set by case 74 for this core's own rider and cleared at the 120F20 point;
    - `browser_environment_selector_step` (web/environment_bridge.cpp), run in race_end right after the stage triggers;
    - `originalEnvironmentIrradiance` now takes the selector (it was 0).
  - The irradiance is computed in the FX pass. With deferred FX (races with computer riders) that pass runs after race_end, so the bank switch follows the PS2. In a solo run the FX pass runs before race_end and the switch comes one tick late. The selector itself is exact in both.
  - PS2: none of the Ruthless runs we had enters a tunnel volume (the tuck line passes about 1 m above tunnelvolume_1000). The same tuck script held to 11000 ticks (`local/ps2-capture/scripts/tunnel-dbc2-long.json`; capture `runs/tunnel/dbc2-tunnel-ai`, --ai-state, watching the human's block 0x4FA370) enters one at 9830. The selector reads 0.1, 0.19, 0.271 ... and decays to 0 by 10730, where the EE flushes the denormal.
  - Check: selector bit-exact on all 11000 records (899 nonzero); human, Nate and the RNG exact to the end. Gate `tunnel/dbc2-tunnel-ai`; `web/test-tunnel-lighting.mjs` (hook `web/environment-selector-hook.mjs`).

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
  - `moving_instances()` carries every AvaSpline follower a tumbler drives: key = resource, the draw delta of the AvaSpline matrix +0x40 (the Object's draw matrix: 0x2D9C00 of that tick's group 1, see "Collision") against the authored instance matrix.
  - `avalanche_pieces()` gives the 0x2DA1C0 set (active slots, list order): resource, state (1 following, 2 type-2 at rest), follower, emitter, the 0x2D9C00 matrix, alpha, scale. It is followed by the instances released since the last call (entity vt+0x08(3): hidden) and the loop refcount with its changes (0x29DEF0, audio+0x6040). The per-tumbler sound cues are dead (0x29E560 is `jr ra`); only their cursor advances.
  - Since core56 a piece is a follower only once builtin 95 attached its AvaSpline, which 0x305D90 does only on an instance with an entity (+0xC, vt+0x84); no piece has one at the countdown, so it takes the trigger program's builtin 0. ABC1 `ava1bitB_1003` (621062) gets builtin 95 from the snowfield timer (program 64) but never a builtin 0: it stays at its authored place, not a follower. Likewise the released list holds only pieces whose entity the release destroyed (2D7DD8 calls vt+0x08(3) only with an entity).
  - `avalanche_info()` (QA: definitions, triggers, ticks, slots) and `avalanche_trigger_qa(id)`.
- **Open:**
  - **Trails:** done (core59), see "Trails" below.
  - **Moving pieces' collision:** done (core56), see "Collision" below.
  - **Save / restore** 0x2D9CB0 / 0x2D9D68: ported in the engine (not wired), see "Replay snapshot" below.
  - **Computer-rider cores:** done (core57; core58 keys the shared copy by a generation, not the location name). A context that never loaded definitions of its own takes the latest ones a core loaded (`avalanche_sync`: before a trigger, an Object, a new race); a core that loaded its own keeps them, so its stage programs (its own contacts and the replayed ones, web/shared_world.inc event 6) play the avalanche and its rocks collide for the computer riders too. Check: parity-ai/era5, where a computer rider sets avalanche 28 off at 1998, now plays it in all six cores alike (1 trigger, 445 ticks each; `web/test-avalanche-collision.mjs`). All 26 gates on avalanche locations are unchanged: no computer rider in them meets a collidable piece.
  - **Duplicate ids:** DBC2 and EBC3 both use id 71. In MOUNTAIN the first definition in list order wins, which is the later-loaded location.

## Collision (2026-09-28, core56)

Every collidable avalanche piece (72 over the seven locations: the type-3 rocks, boulders and minis; ERA5's and DRA4's pieces are collision type 0) has an event seed: the countdown gives it the static route 0x20 at its authored place (EBA3 0x40214123, the others 0x40214023, ABC1 avalanche 14's minis 0x40204022). None is "unsupported": before this port the rocks kept that static collision for the whole race, never threw, and after the trigger answered where they started instead of where they tumble.

**The trigger program** (EBA3 program 75: builtin 2 on the trigger, 30, 44, 3, **94**, 30, then **0** and **95** for each rock):
- Builtin 0 (0x2FC0D0 -> 0x356DB0, key 0 the piece, keys 1 / 2 = 0): an Object entity (vtable 0x490E80) in entity group 1. Flags: 34FB00 -> 2D1BF0 -> 1032C0 `|= 0x100`; 356DB0 (key 2 = 0) `(flags & 3) == 3 -> flags & ~2 | 4`; key 1 `& ~0x40 | 0x20`.
- Builtin 95 (0x305D90, key 0 int, -1 = ctx+0x290; types gp+0xCB0): with an entity whose vt+0x84 accepts modifiers (Object 0x360DD0 = 1) -> 0x355A78 (skipped when gp+0x9D4, the record's bad-magic flag, is set) -> ctor 0x357750, 0x90 bytes, vtable 0x48F338:
  - +0x40..+0x7F = the instance matrix (instance +0x10), +0x80 = the instance, +0x30 = 0;
  - attach 0x3554B0 -> 0x356780 -> 0x350570: bounds +0x10 / +0x20 = the instance box +0x60 / +0x6C (w 1), radius +0x30 = the largest distance from the instance translation (+0x40) to the 8 box corners (VU dot, sqrt.s); flags `& ~0x20 | 0x40`.
  - EBA3 0x40214123 -> 0x40214125 -> **0x40214145**, the PS2 value (the others end the same way).
- **The entity route** of the Object with this primary modifier:

| Original | Target | Result |
| --- | --- | --- |
| 334458 bounds (vt+0x164 / +0x16C 3569D0 / 356A00 -> 352B88) | modifier vt+0x64 0x361AD8 | +0x10 / +0x20 |
| 334888 override (vt+0x134 356A28) | vt+0xA4 0x360BC8 | 0: the ordinary node path |
| 334888 root (vt+0xCC 356128) | vt+0x9C 0x360BC0 = 0, so vt+0x94 0x361B10 | +0x40 replaces instance+0x10 as the hierarchy root |
| 1057B8 rigid predicate (vt+0x74 355420) | vt+0x44 0x360B60 | 1: the rigid response |
| 104E70 selected callback (vt+0x154 356AE0 -> 353098) | vt+0xB4 0x360BD8 | nothing (no surface velocity) |
| 121818 entity contact (vt+0x144 355770) | vt+0x54 0x360B70 / gate vt+0x4C 0x360B68 | nothing / 1 (the rocks have no slot-2 program) |

**Every tick**, entity group 1 (before the riders): 356198 -> 3556F8 -> the AvaSpline update 0x357820 -> 0x2D1CF0 -> 0x2D9C00 (+0x40 = the tumbler's matrix; unchanged without a tumbler), then 355028(group 1) -> 360DA0 -> 3568B0: bounds = the +0x40 translation -/+ (r, r, r, 0) (VU vsub / vadd, chop) and 3291E0 moves the instance to its new octree cell.
- **Order against the tumbler step, from the records:** at a record (the provider exit) the AvaSpline +0x40 holds 2D9C00 of the tumblers of the *previous* record (eba3-rock-hit watches 0x5B4700 against the slot table 0x538938). So the update runs before that tick's tumbler step, and the port runs it first in `browser_avalanche_tick` (race_begin), then the slots. The kept savestates show the same thing (their +0x40 is one tumbler step behind their slot table).
- **Release** (2D7DD8 -> entity vt+0x08(3) = 361038 -> 3553C0: 3567E0 detach, 352AE8 modifiers, 34FBF0): `flags = (flags & 0xFFFF0300) | (flags >> 16) | 2`, 0x40214145 -> 0x40214123: the static route at the authored place again. Type-2 pieces (all of EBA3's rocks) are never released. **The draw:** the low bits come back as the authored 3 (static draw), with 0x100 still set, and the static draw shows the piece at its authored place again (section Draw, "After the release").

**Port** (`web/avalanche_gameplay.inc` "Pieces' collision"):
- `avalanche_object` (builtin 0 on a group instance with no entity): records the Object; for a collidable piece, the flags above (`stage_set_flags`, so a new race restores the countdown flags).
- `avalanche_spline` (builtin 95): attaches only to a recorded Object (else nothing, as 0x305D90), from the instance matrix; the attach bounds and radius are `originalSplineAttachBounds` (the same 0x350570).
- `avalanche_entities_tick` (race_begin, before the slots' step): 2D9C00 and the 3568B0 bounds, then `composeWorldCollisionEntity` on +0x40 (recomposed only when the matrix or bounds change). As for the chairlifts, the composed nodes and box also go to the instance's static fields, which the ray queries (air trajectory, landing, crash probes) read; the authored ones come back at the release, a new race (`avalanche_reset`) or when a streamed location leaves (`browser_avalanche_track_reset`).
- `browser_entity_rigid` answers 1 and `browser_entity_selected` does nothing for these entities (web/roller_gameplay.inc); `stage_world_instances` counts the Object as an entity draw (flags & 4).
- Not modelled: the 3291E0 octree move (the port keeps the load's order for the normal sum of several contacts, as for the chairlifts), and the rider scope list's entity admission (332DB8 admits a flag-0x40 entity by its bounds at the rebuild every third tick; the port tests entity-route instances at query time).
- Computer-rider contexts take the human's definitions (core57, see "Open").

**Verification:**
- `web/test-avalanche-collision.mjs`: much-2-much-event-tuck against the kept much-2-much-full savestates 820 .. 2819 (the five rocks' flags, AvaSpline radius / bounds / matrix bit for bit, 30 of 30), and three new captures (`local/ps2-capture/scripts/avalanche-eba3-rock-hit{,-a,-b}.json`: the same run steered with lx 1 after the trigger; watches of every rock's AvaSpline 0x5A3400 / 0x5A5000 / 0x5A3700 / 0x5A5900 / 0x5B4700 and instance header at this script's heap addresses): every rock on every record after the attach bit for bit (6390 / 6395 / 6390), the rider exact to the end.
  - `eba3-rock-hit` passes rockslide_1001 at 1.8 m around 1111: no contact on the PS2 (the first port, with the update after the tumbler step, touched it at 1112 and left).
  - `eba3-rock-hit-a` / `-b` hit it at 1113 / 1112 (105398 instance contact, the rigid response): exact to the end (1800 / 1799 ticks); core50 (static rocks) leaves at the contact.
- Gates `avalanche/eba3-rock-hit`, `-a`, `-b` in `web/test-ps2-captures.mjs`.

## Replay snapshot: save / restore (2026-09-28, engine only)

The PS2 reaches `0x2D9CB0` / `0x2D9D68` only through the race replay's world snapshots (docs/replay.md): cReplay `0x26D818` saves at the start gate, every 60 ticks and for the kept highlights; `0x26DBF0` restores at the replay start, an R1 / L1 skip and Exit replay. It restores after the entity groups were emptied (`0x355118`) and before the move nodes and their modifiers come back (`0x357D28`).
- **Save:** 0x80 bytes through the stream's vt+0x0C: per slot the playing avalanche's id (-1 free), then per slot its t (0 free).
- **Restore:**
  1. Every playing slot is released (`0x2D81B0`: the loop refcount goes down and up again).
  2. Each saved avalanche is triggered again in saved order (`0x2D97A8`, first free slot and tumblers).
  3. Slot t = t. When t != 0, per tumbler: +0x2EC = 1 for emitter groups, then `0x2D7C00(t)` and `0x2D5778(t)`.
  4. The event cursor advances while event t <= t; the sound cursor while float(tick) <= t.
- **Its quirks against the continuous run:**
  - The speed factor is not applied (0x2D7CA8 plays t x speed): ABC1's 0.75 groups come back ahead.
  - +96 / +112 keep the trigger's values (instance translation, identity) until the next 0x2D7CA8, so the next 0x2D9C00 (group 1, before the step) puts the AvaSpline pieces, with their collision, at the start with identity rotation for that tick.
  - The sound cursor is compared with t, not t x 30.
  - Released pieces get tumblers again for one tick.
- **Port:** `engine/avalanche.hpp` `originalAvalancheSave` / `originalAvalancheRestore`.
  - Live oracle (`tests/avalanche_live.cpp` part 3, `tools/test_avalanche_live.py`): the group instances' entities are cleared as 0x355118 leaves them, and a host stream takes the 0x80 bytes. The saved words, the restored state, the 0x2D9C00 matrices right after the restore and 60 more ticks are bit for bit on all 8 playing states (EBA3 820..2819, ERA5 2019 / 2418).
  - The combined result: 19 runs, 1072 exact ticks, 6808 AvaSpline matrices, 0 failures.
- **Not wired (known replay difference):** the browser's replay re-simulates the run instead of restoring snapshots (web/replay.js), so after an R1 / L1 skip or Exit replay with an avalanche running it shows the continuous state, not the PS2's restore quirks above. At the replay start no avalanche runs, so there is no difference there. Wiring it would be one core call (restore of the current save) where the replay lands on a skip target.

## Trails (2026-09-28, core59)

Every pool tumbler holds a colour dynamic emitter at +0xD0: 0x208 bytes, vtable 0x4930D0, the class of the rider snow. It is the dynamic emitter of `engine/set_piece_particles.hpp` plus +0x200 (the +720 flag) and +0x204, the colour ring ("DynEmitterData Colours", capacity x RGBA bytes). It is constructed once with the pool (0x3714B8) and persists: the kernel seeds 1..8 are never written by the avalanche.
- **Trigger** (an emitter group, `hasEmitter`, the builtin-96 block at group +0..+0xD7):
  1. 0x371600 clears: 0x370D60 zeroes the rings and the active count, then the colours are zeroed.
  2. 0x370DC8(emitter, block, seed 2.0) runs the 0x370DC8 body of the set pieces with that seed: 0x370058 writes kernel seed 0 = 2.0 only and draws nothing. The only visual draw is the flip phase, when NumFlipTextures >= 2. The ring is reallocated (0x3715B0 / 0x371548 / 0x371600) when the capacity ceil((Life + LifeR / 2) x 60) changes.
  3. +720 = 1.
  4. +0x1E0 (the kernel colour base) = (0x2EE7C8, 0x2EE810, 0x2EE858)(0), w 0: the human's Weather painter flake R, G, B (block 0 +0x20; web: `breathEnvironment` properties 12..14, default 1).
- **Every tick** (0x2D7CA8, an emitter group that is not type 2, still playing):
  - +0x1FC (kernel +0x10C, colour range 0 alpha) = the tumbler alpha.
  - 0x3717C0(emitter, +96, zero velocity 0x4FF120, colour +192, active = t' < duration, gp-0x3CD4 = 1/60):
    - +0x1D0 = colour;
    - the ring slot's bytes = trunc(clamp(c x 255, 0, 255)) (negative / NaN 0);
    - then 0x3710D0, whose active birth draws **one value of the presentation stream 0x4FF018**. That is 3 draws a tick for EBA3's dust trails and 8 for ERA5's nodes, which the port did not make before: its visual stream (rider snow seeds, flags, splash ...) fell behind from every trigger.
- **Release** (0x2D7DD8): 0x371600 on the released tumbler. +720 stays.
- **Replay restore:** +0x2EC = 1 makes the next 0x2D5778 emit one point per skipped sample (the catch-up). Not ported: it is reached only by the unwired restore.
- **Draw:** 0x2D9130 (priority 7, word2 |= 0xE0) -> 0x2D8EA8(slot) -> 0x371688(emitter, 7) for each tumbler of the list whose group has an emitter. It needs +0x174 (enabled) and +0x1E0 (live births) > 0. It rewrites the GS packet words +0x1E8 (blend 0x44B420), +0x1EC (priority 7 << 5) and the +0x1F4 halfword (texture).

**Port** (`engine/avalanche.hpp`):
- `OriginalAvalancheEmitter`, constructed lazily so the rider-local world stays constant-initialisable; `originalAvalancheEmitterClear / Setup / Emit`.
- `originalAvalancheTrigger(w, id, visual, flake)`, `originalAvalancheSlotTick(..., visual)`. The core (`web/avalanche_gameplay.inc`) passes `stage_visual_random()` (the camera's presentation stream in the human core; a computer rider's context draws a stream of its own, as the other world effects do) and `avalanche_flake()`.
- The world is reset in place (`avalanche_world_reset`): a whole-world temporary with the emitters overflowed the wasm stack.
- Rider TLS grows by ~35 KB per context (64 emitters).
- **Export** `avalanche_trails()` for the draw: per active slot in list order, per emitter tumbler:
  - `[resource, pool index, 130 image words (ring pointers 0), n, ring A (n x 4), ring B (n x 4, w = the birth seed, 0 none), colours (n words RGBA)]`;
  - the ring is newest-first from the cursor +0x17C.

**Verification:**
- Live oracle (`tests/avalanche_live.cpp`) compares every tumbler's emitter (image, rings, colours), +0x200, and the presentation stream words 0x4FF018 after every tick against the recompiled original. The trigger passes the painter getters' flake. The recompiled div.s gives an IEEE infinity for 1 / NumBlur (0), where the EE (PCSX2 savestates) holds 0x7F7FFFFF, so the oracle corrects that one word.
- Results: 19 runs, 6852 exact ticks at 400 ticks a state (through ERA5's releases), 0 failures.
- `web/test-avalanche-collision.mjs`: the core's EBA3 emitters bit-exact against the much-2-much-full 820 savestate. The draw-written packet words and the birth seeds of this solo replay's stream are left out.

**Draw** (`web/avalanche-trails.js`, pv `avalancheTrails`, off; 2026-09-28, visual-parity agent):
- The sprites are particle entry 0xA00 on the export's ring, the rider snow's model (`engine/snow_particles.cpp`
  OriginalSnowParticles::particles / originalSnowParticle) in JS with the EE float operations. Particles per birth = kernel N / ring
  slots (1 for every retail trail).
- Material: fog0 (4), MODULATE, GS 0x44, depth tested, no Z write. It draws in the encoded pass at priority 7, rank 1 (key
  `drawOrder({priority 7, mode 4, fx 4}, SUBMIT.avalanche)`).
- **The trails are nearly invisible, on the PS2 too.** The trigger writes the flake colour into the kernel's colour base with w 0,
  and the tick writes the tumbler alpha into colour range 0 alpha. So a sprite's alpha is at most trunc(alpha x [1, 2) - 16.8 x
  age) <= 1 or 2 of 128. Sprites with alpha 0 are skipped: GS 0x44 with As = 0 leaves the frame as it is, and with no Z write
  there is no side effect.
  - PS2 RAM: EBA3 820, 3 x 60 sprites, 15 with alpha > 0 (alpha 1). ERA5 2019 / 2418: 8 x 21 / 8 x 60 sprites, none.
  - PS2 frames at the projected positions (EBA3 820 near (370, 270), 70 m; ERA5 2019 near (456, 265), 233 m) show no dust.
- **Checked:**
  - At EBA3 820 and ERA5 2019 / 2418, stepped to the PS2 slot t, the page's trails have the PS2's counts, cursors, active
    births, ring positions and colours.
  - The birth seeds differ (presentation stream 0x4FF018 values), so the sprites' random offsets and sizes differ.
  - Chrome and WebKit, QA trigger: ERA5 28 evaluates 480 sprites and draws none; EBA3 45 evaluates 180 and draws 6..15.
  - No pixel changes against the switch off on the PS2 frames.
  - `web/test-avalanche-trails.mjs` runs the evaluator on 19 PS2 trails (local/reference/avalanche/trails.json): the counts equal the
    live births, newest first, and the GS colour ranges hold.

