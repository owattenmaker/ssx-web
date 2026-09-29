// Backcountry autopilot (docs/backcountry.md): writes a ps2_capture pad script that rides the human down the course
// (tuck, steering toward the retained route lookahead) by running the browser's own rider, which matches the PS2 bit for
// bit over long spans, so the same script replayed open loop on ARMSX2 follows the same line.
// Usage: node bc-autopilot.mjs OUT.json [--course ABC1] [--ticks 9000] [--seg 10] [--gain 2.5] [--jump EVERY]
import fs from 'node:fs';
import { createNodeRace } from './ai-race-node.mjs';
import { loadStageWorld } from './stage-world-compare.mjs';
const args = process.argv.slice(2);
const out = args[0];
const opt = (k, d) => (args.includes(k) ? args[args.indexOf(k) + 1] : d);
const course = opt('--course', 'ABC1'), maxTicks = +opt('--ticks', 9000), seg = +opt('--seg', 10), gain = +opt('--gain', 2.5), tricks = +opt('--tricks', 0);
const docMode = opt('--doc', null); // race | jam: the rivals.json document (docs/backcountry.md)
const rival = opt('--rival', null); // the rivals.json document of this rival (default: Mac on Happiness, else the first: Nate on Ruthless, docs/peak2.md)
const document = docMode ? (() => { const d = JSON.parse(fs.readFileSync(new URL(`public/assets/${course}/rivals.json`, import.meta.url), 'utf8')).documents[docMode]; return d[rival ?? 'mac'] ?? Object.values(d)[0]; })() : null;
const r = await createNodeRace({ humanPackage: 'RIDER_ZOE', course, document });
// the browser's world (as compare-ai-capture.mjs): section activation 0x101B60 and the stage world, in the human core
{ const h = r.human, root = new URL('public/assets/', import.meta.url), put = (t) => { const b = Buffer.from(t + '\0'), q = h._malloc(b.length); h.HEAPU8.set(b, q); return q; };
  const sections = new URL(`${course}/SECTIONS/sections.json`, root); if (h._init_sections && fs.existsSync(sections)) h._init_sections(put(fs.readFileSync(sections, 'utf8')));
  loadStageWorld(h, root, course);
  if (document?.game_mode) for (const c of [h, ...r.racers.npcs.map((n) => n.core)]) c._event_kind?.(document.game_mode.kind); }
r.start();
if (document?.anchor_rng) r.racers.setSharedRng(document.anchor_rng.words); // the ready state's game RNG: the rival rides the PS2's line (web/lineup.js rivalAnchorWords)
const segments = [{ frames: 10, buttons: ['Cross'] }]; // the overlay Continue is still held on the first race ticks (PS2 nav)
const pad = new Float32Array(24);
// The PS2 pad path (tools/ps2_capture.py script -> 18-byte frame; compare-ai-capture.mjs decodePad): buttons 0/1 and
// 0..255 pressures, sticks as bytes with the 79..176 dead zone -- the same decode the PS2 replay sees.
const BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown', 'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
const tz = (x) => { let r = Math.fround(x); if (Math.abs(r) > Math.abs(x)) { const b = new Float32Array([r]); new Uint32Array(b.buffer)[0] -= 1; r = b[0]; } return r; };
const axisByte = (v) => Math.floor((Math.max(-1, Math.min(1, v)) + 1) * 127.5 + 0.5);
const setPad = (seg) => { pad.fill(0); const held = new Set(seg.buttons || []);
  BUTTONS.forEach((n, i) => { const on = held.has(n); pad[i] = i >= 4 ? tz((on ? 255 : 0) * R255) : (on ? 1 : 0); });
  [axisByte(seg.rx || 0), axisByte(-(seg.ry || 0)), axisByte(seg.lx || 0), axisByte(-(seg.ly || 0))].forEach((b, a) => {
    const neg = Math.max(Math.trunc((79 - b) * 255 / 79), 0), pos = Math.max(Math.trunc((b - 176) * 255 / 79), 0); pad[16 + 2 * a] = tz(neg * R255); pad[17 + 2 * a] = tz(pos * R255); }); };
let t = 0, finished = null;
const run = (s) => { setPad(s); for (let k = 0; k < s.frames; k++) { const race = r.tick(pad); t++; if (!finished && race[3]) finished = t; } };
run(segments[0]);
while (t < maxTicks && !finished) {
  const m = Array.from(r.f32(r.human._reference_motion(), 20)), route = Array.from(r.f32(r.human._route_info(), 14));
  const pos = m.slice(0, 3), vel = m.slice(3, 6), look = route.slice(11, 14);
  const dx = look[0] - pos[0], dy = look[1] - pos[1];
  const cross = vel[0] * dy - vel[1] * dx, dot = vel[0] * dx + vel[1] * dy;
  const angle = Math.atan2(cross, dot); // + = lookahead to the left of the velocity
  let lx = Math.max(-0.9, Math.min(0.9, -gain * angle)); if (Math.abs(lx) < 0.15) lx = 0; else if (Math.abs(lx) < 0.45) lx = Math.sign(lx) * 0.45; const q = +opt("--quant", 0.1); lx = Math.round(lx / q) * q; lx = Math.round(lx * 100) / 100;
  const s = { frames: seg, ly: 1 };
  if (lx) s.lx = lx;
  const last = segments[segments.length - 1];
  if (tricks && t % tricks < seg && t > 120) {   // a jump with a grab: hold Cross 20 ticks, release, hold R1 30 ticks
    for (const j of [{ frames: 20, ly: 1, buttons: ['Cross'] }, { frames: 30, ly: 1, buttons: ['R1'] }]) { segments.push(j); run(j); }
    continue;
  }
  if (last.ly === s.ly && (last.lx || 0) === (s.lx || 0) && !last.buttons && !s.buttons) { last.frames += seg; run({ ...s }); }
  else { segments.push(s); run(s); }
}
fs.writeFileSync(out, JSON.stringify({ segments }) + '\n');
const score = new Int32Array(r.human.HEAPU8.buffer, r.human._score_object_dump(), 0x1d0 / 4)[0x198 / 4];
console.log(JSON.stringify({ ticks: t, finished, segments: segments.length, score, rival: r.racers.npcs.map((n) => new Int32Array(n.core.HEAPU8.buffer, n.core._score_object_dump(), 0x1d0 / 4)[0x198 / 4]), standings: r.racers.standings() }));
