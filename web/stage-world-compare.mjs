// Stage world (web/stage_world.inc) against PS2 savestates: the particle effects the core holds after tick T-1
// compared word for word with the Particle / DynamicParticle objects of the savestate RUN.tickT.p2s
// (tools/export_particle_snapshots.py). Used by compare-ai-capture.mjs / compare-ps2-capture.mjs (STAGE_WORLD_PS2)
// and web/test-stage-world.mjs. Seed words (visual RNG 0x4FF018: kernel seeds, DynamicParticle flip phase / ring
// seeds) and words the original never writes (allocation contents) are excluded.
import fs from 'node:fs';

// Load the stage world data the browser loads (web/set-pieces-renderer.js) into a core: particles.json (+ livecomp.json).
export function loadStageWorld(core, root, courseCode) {
  const particlesFile = new URL(`${courseCode}/PARTICLES/particles.json`, root), liveCompFile = new URL(courseCode === 'ARA1' ? 'LIVECOMP/livecomp.json' : `${courseCode}/LIVECOMP/livecomp.json`, root);
  if (!core._init_stage_world || !fs.existsSync(particlesFile) || !fs.existsSync(liveCompFile)) return false;
  const put = (text) => { const b = Buffer.from(text + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const stageFile = new URL(`${courseCode}/STAGE/stage-world.json`, root);
  const a = put(fs.readFileSync(particlesFile, 'utf8')), b = put(fs.readFileSync(liveCompFile, 'utf8')), c = put(fs.existsSync(stageFile) ? fs.readFileSync(stageFile, 'utf8') : '');
  let ok; try { ok = core._init_stage_world(a, b, c) > 0; } finally { core._free(a); core._free(b); core._free(c); }
  // Flag manager bookkeeping (grid builds / wind on the visual stream), as set-pieces-renderer.js with native sections.
  const flagsFile = new URL(courseCode === 'ARA1' ? 'FLAGS/flags.json' : `${courseCode}/FLAGS/flags.json`, root), readyFile = new URL(`${courseCode}/SECTIONS/ready-state.json`, root);
  if (ok && core._init_stage_flags && fs.existsSync(flagsFile) && fs.existsSync(readyFile)) { const f = put(fs.readFileSync(flagsFile, 'utf8')), r = put(fs.readFileSync(readyFile, 'utf8')); try { core._init_stage_flags(f, r); } finally { core._free(f); core._free(r); } }
  // Weather (web/weather.inc, tools/export_weather.py): the camera's Weather painter, snowfall layers, camera splash and lightning
  // in the post-rider pass, as main.js loads it. WEATHER=0 leaves it out (the pre-weather visual stream).
  const weatherFile = new URL(`${courseCode}/weather.json`, root);
  if (ok && process.env.WEATHER !== '0' && core._init_weather && fs.existsSync(weatherFile)) { const w = put(fs.readFileSync(weatherFile, 'utf8')); try { core._init_weather(w); } finally { core._free(w); } }
  // Avalanches (web/avalanche_gameplay.inc, tools/export_avalanches.py): <LOC>/avalanches.json, or AVALANCHE_DIR/<LOC>.avalanches.json.
  const avaFile = process.env.AVALANCHE_DIR ? new URL(`file://${process.env.AVALANCHE_DIR}/${courseCode}.avalanches.json`) : new URL(`${courseCode}/avalanches.json`, root);
  if (ok && core._init_avalanches && fs.existsSync(avaFile)) { const t = put(fs.readFileSync(avaFile, 'utf8')); try { core._init_avalanches(t, 0); } finally { core._free(t); } }
  return ok;
}

export function coreEffects(core) {
  const U = new Uint32Array(core.HEAPU8.buffer); let at = core._stage_world_effects() >> 2; const n = U[at++], out = [];
  for (let k = 0; k < n; k++) {
    const e = { resource: U[at], kind: U[at + 1], dead: U[at + 2], type: U[at + 3] }; at += 4;
    if (e.kind === 0) { e.matrix = Array.from(U.subarray(at, at + 16)); at += 16; e.emitter = Array.from(U.subarray(at, at + 100)); at += 100; }
    else { e.node = U[at]; e.stopped = U[at + 1]; at += 2; e.emitter = Array.from(U.subarray(at, at + 128)); at += 128; const cap = U[at++]; e.ringA = Array.from(U.subarray(at, at + 4 * cap)); at += 4 * cap; e.ringB = Array.from(U.subarray(at, at + 4 * cap)); at += 4 * cap; }
    out.push(e);
  }
  return out;
}

// Static emitter (Particle +0x50): kernel +0x10 seeds (+0x38..+0x48, +0x50..+0x5C) and the never-written kernel +0x24..+0x2C.
export const SEED_STATIC = new Set([0x34, 0x38, 0x3C, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x60, 0x64, 0x68, 0x6C].map((o) => o / 4));
// Dynamic emitter (DynamicParticle +0x60): flip phase +0x10, +0x18/+0x1C (never written), kernel +0x20 seeds and +0x24..+0x2C.
export const SEED_DYNAMIC = new Set([0x10, 0x18, 0x1C, 0x44, 0x48, 0x4C, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x70, 0x74, 0x78, 0x7C].map((o) => o / 4));

// One snapshot: {ps2, web, matched, exact, missing, extra, diffs}. Effects are matched by (resource, kind), then by the
// fewest differing words. DynamicParticle ring buffers (+0x1A0 pointers) are skipped; stopped/dead are compared.
export function compareStageWorld(core, snap, { allDiffs = false } = {}) {
  const web = coreEffects(core);
  const ps2 = snap.effects.map((e) => ({ resource: e.resource, kind: e.kind === 'Particle' ? 0 : 1, dead: e.dead, stopped: e.stopped, emitter: e.emitter }));
  const key = (e) => `${e.resource}:${e.kind}`, pool = new Map();
  for (const e of web) { if (!pool.has(key(e))) pool.set(key(e), []); pool.get(key(e)).push(e); }
  const row = { tick: snap.tick, ps2: ps2.length, web: web.length, matched: 0, exact: 0, missing: [], extra: 0 };
  for (const e of ps2) {
    const list = pool.get(key(e)); if (!list || !list.length) { row.missing.push(key(e)); continue; }
    const skip = e.kind === 0 ? SEED_STATIC : SEED_DYNAMIC; let best = null, bestDiffs = null;
    for (const w of list) {
      const diffs = [];
      for (let i = 0; i < Math.min(e.emitter.length, w.emitter.length); i++) if (!skip.has(i) && !(e.kind === 1 && i >= 0x1A0 / 4) && e.emitter[i] !== w.emitter[i]) diffs.push(i);
      if (!best || diffs.length < bestDiffs.length) { best = w; bestDiffs = diffs; }
    }
    list.splice(list.indexOf(best), 1); row.matched++;
    if (e.kind === 1 && best.stopped !== e.stopped) bestDiffs.push(-1);
    if (!bestDiffs.length && best.dead === e.dead) row.exact++;
    else if (allDiffs || !row.diffs) (row.diffs ??= []).push({ key: key(e), dead: [best.dead, e.dead],
      words: bestDiffs.slice(0, 8).map((i) => (i < 0 ? ['stopped', best.stopped, e.stopped] : [(i * 4).toString(16), best.emitter[i].toString(16), e.emitter[i].toString(16)])) });
  }
  for (const l of pool.values()) { row.extra += l.length; for (const e of l) (row.extras ??= []).push(`${key(e)}${e.dead ? ' dead' : ''}`); }
  // MeshAnim break pieces: deterministic words (flags without the renderer bits 0x100/0x200, life, fade, spin max, end mode, gravity, node count) exact; the
  // pieces' random velocity / spin come from the visual RNG, so positions are reported, not compared.
  if (snap.meshanims && core._stage_world_meshanim_state) {
    const U = new Uint32Array(core.HEAPU8.buffer), F = new Float32Array(core.HEAPU8.buffer); let at = core._stage_world_meshanim_state() >> 2; const n = U[at++], web = new Map();
    for (let k = 0; k < n; k++) { const e = { resource: U[at], flags: U[at + 1], life: F[at + 2], fade: F[at + 3], spinMax: F[at + 4], endMode: U[at + 5], gravity: F[at + 6], nodes: U[at + 7] }; at += 8; e.pos = []; e.vel = []; for (let i = 0; i < e.nodes; i++) { e.pos.push(Array.from(F.subarray(at + 4, at + 7))); e.vel.push(Array.from(F.subarray(at + 8, at + 11))); at += 17; } web.set(e.resource, e); }
    row.meshanims = snap.meshanims.map((p) => { const w = web.get(p.resource); if (!w) return { resource: p.resource, missing: true };
      const same = (w.flags & ~0x300) === (p.flags & ~0x300) && Math.fround(w.life) === Math.fround(p.life) && w.endMode === p.end_mode && Math.fround(w.gravity) === Math.fround(p.gravity) && w.nodes === p.nodes && Math.fround(w.spinMax) === Math.fround(p.spin_max) && Math.fround(w.fade) === Math.fround(p.fade);
      const drift = Math.max(0, ...p.pos.map((q, i) => Math.hypot(q[0] - w.pos[i][0], q[1] - w.pos[i][1], q[2] - w.pos[i][2])));
      return { resource: p.resource, exact: same, web: [w.flags.toString(16), w.life, w.endMode, w.gravity, w.nodes, w.spinMax], ps2: [p.flags.toString(16), p.life, p.end_mode, p.gravity, p.nodes, p.spin_max], maxPieceOffsetCm: +drift.toFixed(1), meanVel: { web: [0, 1, 2].map((c) => +(w.vel.reduce((a, v) => a + v[c], 0) / w.nodes).toFixed(2)), ps2: [0, 1, 2].map((c) => +(p.vel.reduce((a, v) => a + v[c], 0) / p.nodes).toFixed(2)) }, piece0: { web: w.pos[0]?.map((x) => +x.toFixed(2)), ps2: p.pos[0]?.slice(0, 3).map((x) => +x.toFixed(2)) } }; });
    row.meshanimExtra = [...web.keys()].filter((r) => !snap.meshanims.some((p) => p.resource === r));
  }
  // MultiParticle groups (roadflare flames): members in order, emitter words except the seeds and the words the draw
  // 0x370888 writes per member (kernel position base +0xC0, bounds +0x160..+0x17C).
  const DRAWN = new Set([0xC0, 0xC4, 0xC8, 0xCC, 0x160, 0x164, 0x168, 0x16C, 0x170, 0x174, 0x178, 0x17C].map((o) => o / 4));
  if (snap.multi && core._stage_world_multi) {
    const U = new Uint32Array(core.HEAPU8.buffer); let at = core._stage_world_multi() >> 2; const n = U[at++], web = new Map();
    for (let k = 0; k < n; k++) { const g = U[at], cap = U[at + 1], m = U[at + 2]; at += 3; const members = Array.from(U.subarray(at, at + m)); at += m; web.set(g, { cap, members, emitter: Array.from(U.subarray(at, at + 100)) }); at += 100; }
    row.multi = snap.multi.map((p) => { const w = web.get(p.group); if (!w) return { group: p.group, missing: true };
      const words = []; for (let i = 0; i < 100; i++) if (!SEED_STATIC.has(i) && !DRAWN.has(i) && w.emitter[i] !== p.emitter[i]) words.push([(i * 4).toString(16), w.emitter[i].toString(16), p.emitter[i].toString(16)]);
      return { group: p.group, exact: !words.length && w.cap === p.capacity && JSON.stringify(w.members) === JSON.stringify(p.members), members: [w.members.length, p.members.length], words: words.slice(0, 6), memberDiff: JSON.stringify(w.members) === JSON.stringify(p.members) ? undefined : [w.members, p.members] }; });
  }
  // One-way volumes (Boost entities): direction, speed, gain, duration, countdown, mode words exact.
  if (snap.boosts && core._stage_world_boosts) {
    const U = new Uint32Array(core.HEAPU8.buffer); let at = core._stage_world_boosts() >> 2; const n = U[at++], web = new Map();
    for (let k = 0; k < n; k++) { web.set(U[at], Array.from(U.subarray(at + 1, at + 10))); at += 10; }
    row.boosts = snap.boosts.map((p) => { const w = web.get(p.resource); return { resource: p.resource, exact: !!w && w.every((x, i) => x === p.words[i]), web: w?.map((x) => x.toString(16)), ps2: p.words.map((x) => x.toString(16)) }; });
    row.boostExtra = [...web.keys()].filter((r) => !snap.boosts.some((p) => p.resource === r));
  }
  return row;
}

// One or more snapshot files (comma separated), merged by tick.
export function loadSnapshots(paths) {
  const out = new Map();
  for (const path of String(paths).split(',')) for (const x of JSON.parse(fs.readFileSync(path, 'utf8')).snapshots) out.set(x.tick, x);
  return new Map([...out].sort((a, b) => a[0] - b[0]));
}
