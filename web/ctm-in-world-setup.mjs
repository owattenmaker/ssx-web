// The human's setup for a capture that runs through a Peak 1 location arrival, free ride and a CTM event in the streamed world
// (docs/ctm-events-in-world.md stage 3 "The in-world race start"). One implementation for web/compare-ps2-capture.mjs
// (--peak-arrival, --ctm-in-world CODE) and web/compare-ai-capture.mjs (--in-world-ai).
//
// The arrival (tools/ps2_capture.py from a screen-10 state): the comparison starts at the placement record P (the first after
// the Transport's control 13). The words the placement 11D390 keeps are seeded from the records:
// - the retained speed limit +0x2E4 of record P-1. A record is taken at the provider exit 0x128630, after that tick's
//   120F20 -> 11B3F8 (0x121024): record k holds L(k), the limit tick k's motion uses, computed from L(k-1) and the crouch +0x220
//   as tick k-1 left it. The browser's first tick (record P's pad) runs 11B3F8 at its start, so it starts from L(P-1).
//   (Seeding record P's L(P) ran the browser one step ahead with the crouch a tick late: 0.42 cm/s on a tuck onset, bra2-tuck-late
//   2071.) LIMIT_AT_PLACEMENT=1: that one-step-ahead seed (QA);
// - the boost words +0x2E8..+0x304 of record P-1;
// - record P's route heading +0x4CC and location id +0x434 (taken before tick P's 121818 / 1218D0: what 13C948 reads on the first
//   tick) and its tick field (the game tick 1298C8, the rider manager's +8). NO_ARRIVAL_CARRY=1 leaves these out.
//
// The event (--ctm-in-world CODE, local/ctm-events/caps/c0a-full), as the page runs it:
// - the gate record G (0x535C10 leaves free ride's kind 4; 22D6C8): the event's start seeds (event_course_seed), its race
//   document (init_race; the session keeps the total tick count), the kind / mode;
// - after the tick before the hold record H (the first control 13: 123640), the NIS hold at the record's position and heading.
//   Nothing is compared under the hold;
// - WS1's last tick (the placement record C-1, control 6) is not stepped: the Continue's 1297C8(C, 1) places the riders again
//   at the countdown's tick 0 (record C, the game tick restart);
// - at record C, the page's startRun: event_route_seed (the event's grid route) and event_grid_start at the event's grid
//   spawn, which resets the race session and keeps the world (the Big Challenge markers' Hide nodes, pickups, rollers).

const riderF = (buf, k, R0, off) => buf.readFloatLE(k * R0 + 32 + off - 0x100);
const riderI = (buf, k, R0, off) => buf.readInt32LE(k * R0 + 32 + off - 0x100);

// From the whole capture (a Buffer, before the comparer drops the records before P): the placement index and its seeds.
export function arrivalSeeds(raw, R0, env = process.env) {
  const n = Math.floor(raw.length / R0), ctl = (k) => raw.readUInt32LE(k * R0 + 20);
  let P = -1; for (let k = 1; k < n; k++) if (ctl(k - 1) === 13 && ctl(k) !== 13) { P = k; break; }
  if (P < 0) throw new Error('--peak-arrival: no placement record (control 13 -> other)');
  const limit = riderF(raw, env.LIMIT_AT_PLACEMENT ? P : P - 1, R0, 0x2E4);
  const b = (off) => riderF(raw, P - 1, R0, off), bi = (off) => riderI(raw, P - 1, R0, off);
  const boost = [b(0x2E8), b(0x2EC), b(0x2F0), bi(0x2F4), b(0x2F8), b(0x2FC), bi(0x304)];
  const carry = env.NO_ARRIVAL_CARRY ? null : { heading: riderF(raw, P, R0, 0x4CC), location: riderI(raw, P, R0, 0x434), tick: raw.readUInt32LE(P * R0 + 4) };
  return { P, limit, boost, carry };
}

// WS15 (236058, the return from an event to its location; compare-ps2-capture.mjs --peak-ws15 SEED): record 0 is already the placement
// (11DE60(human, Session point 1, kind 2) + 11DF18 in WS15's enter, the hook's first record), so P = 0 and the words the placement keeps
// come from the map state before it (SEED: the baseline savestate's rider, local/ctm-events/caps/c0d-ws15.ws15-seed.json); no Transport.
export function ws15Seeds(raw, R0, seed) {
  const carry = { heading: riderF(raw, 0, R0, 0x4CC), location: riderI(raw, 0, R0, 0x434), tick: raw.readUInt32LE(4) };
  return { P: 0, limit: seed.limit, boost: seed.boost, carry, transport: 0 };
}
// The placement itself (web/peak-capture.mjs arrive: 11D390 at the location's entry row), then the seeds.
export function applyArrival(core, peakWorld, seeds) {
  if (!peakWorld || !core._place_rider_region) throw new Error('--peak-arrival needs --course PEAK1 and a core with place_rider_region');
  const a = peakWorld.arrive(() => core._boost_state_seed(...seeds.boost), seeds.transport ?? 1); core._speed_limit_seed(seeds.limit);
  if (seeds.carry) { if (!core._arrival_carry_seed) throw new Error('--peak-arrival needs a core with arrival_carry_seed');
    core._arrival_carry_seed(seeds.carry.heading, seeds.carry.location); core._game_tick_restart(seeds.carry.tick); }
  return a;
}

// The event's plan over the records (after P): gate G, hold H, countdown tick 0 C, the kind / mode, the document, the spawn.
// records: [{tick, control}], dv / RECORD over the same records, layout = the capture manifest's layout, json(path) reads
// public/assets.
export function planCtmInWorld({ code, core, dv, RECORD, records, layout, json }) {
  if (!core._event_grid_start || !core._event_course_seed || !core._event_route_seed) throw new Error('--ctm-in-world needs a core with event_grid_start / event_course_seed / event_route_seed');
  let gameAt = -1, o = 0; for (const w of layout.watches || []) { if (Number(w.address) === 0x535C08) gameAt = o; o += w.length; }
  if (gameAt < 0) throw new Error('--ctm-in-world needs the 0x535C08 watch');
  const kindAt = (i) => dv.getInt8(i * RECORD + layout.watch_offset + gameAt + 8), modeAt = (i) => dv.getInt8(i * RECORD + layout.watch_offset + gameAt + 10);
  const G = records.findIndex((r, i) => i > 0 && kindAt(i) !== kindAt(0)), H = records.findIndex((r, i) => i > G && r.control === 13), C = records.findIndex((r, i) => i > H && r.tick < records[i - 1].tick);
  if (G < 0 || H < 0 || C < 0) throw new Error(`--ctm-in-world: gate ${G}, hold ${H}, countdown ${C} not all found`);
  const entry = json('courses.json').courses?.find?.((c) => c.code === code) ?? null, eventRoot = entry?.root ? entry.root.replace(/^\/assets\//, '') : `${code}/`;
  const docPath = entry?.initial ? entry.initial.replace(/^\/assets\//, '') : code === 'ARA1' ? 'ANIMATIONS/initial.json' : `${eventRoot}initial.json`;
  const rr = (i, off) => dv.getFloat32(i * RECORD + 32 + off - 0x100, true);
  return { code, G, H, C, kind: kindAt(G), mode: modeAt(G), docPath, start: json(`${eventRoot}start.json`), hold: [rr(H, 0x110), rr(H, 0x114), rr(H, 0x118), rr(H, 0x1B0), rr(H, 0x1B4)] };
}

// Before record i's tick. Returns 'skip' when that tick is not stepped (WS1's last tick). cstr(text) / cfile(path) -> a C string
// in the core's heap that the caller's core owns (freed here with core._free).
export function ctmInWorldBeforeTick(core, plan, i, { cstr, cfile }) {
  const call = (p, f) => { try { return f(p); } finally { core._free(p); } };
  if (i === plan.G) {
    if (!call(cstr(plan.code), (p) => core._event_course_seed(p))) throw new Error(`event_course_seed ${plan.code} failed`);
    call(cfile(plan.docPath), (p) => core._init_race(p)); core._peak_world_event_kind(plan.kind); core._peak_world_game_mode(plan.mode);
  }
  if (i === plan.C - 1) return 'skip';
  if (i === plan.C) {
    core._nis_hold(0, 0, 0, 0, 1, 0); core._reset_pad_history();
    if (!call(cfile(plan.docPath), (p) => core._event_route_seed(p))) throw new Error(`${plan.docPath}: no event route`);
    const [x, y, z] = plan.start.position; core._event_grid_start(x, y, z, plan.start.heading);
  }
  return null;
}

// After record i's tick. Returns true when record i + 1 falls under WS1's hold (not compared); the hold starts after tick H - 1.
export function ctmInWorldAfterTick(core, plan, i) {
  if (i === plan.H - 1) { const [x, y, z, fx, fy] = plan.hold; core._nis_hold(1, x, y, z, fx, fy); }
  return i >= plan.H - 1 && i < plan.C;
}
