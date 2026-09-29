// Audio world painters (PS2 SSX 3): MusicTrigger (type 0), Mix (type 1), Ambience (type 2) at the rider position.
// Data: tools/export_world_audio.py (world/<EVENT>.json: locations[name].painters.{musicTrigger, mix, ambience}).
//
// Original path (per game tick, per rider with rider+0x870 < 2; only the primary listener acts on the result):
//   128AF0: loop 1218D0 over riders (2ED490 -> gp+0x770 = low byte of the human rider's rider+0x430, the TRACK of
//           the last contacted terrain patch = SDB location index), then loop 121950 -> 2898A8(audio, rider).
//   2898A8: pos = rider+0x110 (x, y; source cm, Z-up) via rider iface +0x6C0 slot 0x28 (1408F0);
//           query vfunc 0x10 = 2C0778 (weight -99999 = auto) on the per-rider WorldPainterQuery of each type,
//           created by 2C0A58 (ctor 2C0408, vtable 0x483E00) in 2867E8; getters: MusicTrigger A slot 0xC (2C1490,
//           obj+8), B slot 0x14 (2C1498, obj+0x10); Mix slot 0x1C (2C14A0, obj+8); Ambience slot 0x24 (2C14A8, obj+8).
//   2C0778: region record = kind-15 painter record of track gp+0x770 ([[[G+0x84]+0x10]+8]+4)[track*12], state 3,
//           +8 = record); section = record+8+4*type; 2C0A10/2BAF90/2C1CD8 point tree -> payload or none;
//           none / missing section / missing region -> defaults (cur = -1, B = -1); else blend (see blendInt).
//   Retail data: every audio payload rate is 0.0 -> auto weight path "rate >= 0 && rate <= distance" -> w = 1,
//   so the current value is exactly the sampled payload (no smoothing, no hysteresis).
//
// Coordinates: source cm, Z-up. From the browser (three.js metres, Y-up; web/core.cpp sourceVector):
//   sourceX = world.x * 100, sourceY = -world.z * 100.
// Region: pass the location NAME of the human rider's last contacted terrain patch (track byte of the patch
//   resource rid<<8|track; world_collision.json event_locations maps track -> name, e.g. ARA1 course: 3 A_ARA1,
//   8 ARA1, 9 ARA1_B). Keep the previous region while airborne / when the patch is unknown (2ED490 skips -1).

import { mul, add, sub } from './ee-scalar-float.js';
const cvtWS = (f) => (f >= 2147483647 ? 2147483647 : f <= -2147483648 ? -2147483648 : Math.trunc(f)); // cvt.w.s (chop)

export const AUTO_WEIGHT = -99999;
const TYPE_INDEX = { musicTrigger: 0, mix: 1, ambience: 2, speech: 3 };

// 2C1CD8 + 2BAF90 + 2C0A10: payload index at source (x, y), or -1 when the leaf has no payload (0xFFFFFFFF).
export function painterLeafPayload(section, x, y) {
  const px = mul(sub(Math.fround(x), section.origin[0]), section.scale);
  const py = mul(sub(Math.fround(y), section.origin[1]), section.scale);
  let p;
  if (!(px > -1 && px < 32768 && py > -1 && py < 32768)) p = section.outside[1];
  else {
    const n = section.nodes; // flat: 4 u16 per node
    let a = (Math.trunc(px) << 1) & 0xffff, b = (Math.trunc(py) << 1) & 0xffff, i = section.root;
    for (let steps = 0; ; steps++) {
      if (steps > n.length / 4) throw new Error('Cyclic painter tree');
      if (!(n[i * 4] & 1)) break;
      i = n[i * 4 + (((a >>> 15) << 1) | (b >>> 15))] >>> 1;
      a = (a << 1) & 0xfffc; b = (b << 1) & 0xfffc;
    }
    p = (n[i * 4 + 2] | (n[i * 4 + 3] << 16)) >>> 0;
  }
  if (p === 0xffffffff) return -1;
  if (p >= section.payloads.length) throw new Error('Painter payload index outside section');
  return p;
}

function regionEntry(data, location) {
  if (data && data.locations) { const l = location == null ? null : data.locations[location] ?? null; return l?.painters ?? l; }
  if (data && (location == null || data.location === location)) return data.painters ?? data; // single-location file
  return null;
}

// Stateless query (valid for the retail data, where every audio rate is 0): the sampled values, -1 when unpainted.
// position = {x, y} in source cm (or [x, y]); location = region name (see header). Speech is exported but never
// queried by the original (no query object is created for type 3).
export function queryAudioPainters(data, position, location) {
  const x = Array.isArray(position) ? position[0] : position.x, y = Array.isArray(position) ? position[1] : position.y;
  const entry = regionEntry(data, location ?? position.location);
  const sample = (key) => {
    const s = entry?.[key];
    if (!s) return null;
    const p = painterLeafPayload(s, x, y);
    return p < 0 ? null : s.payloads[p].values;
  };
  const mt = sample('musicTrigger'), mix = sample('mix'), amb = sample('ambience');
  return { mix: mix ? mix[0] : -1, musicTrigger: { a: mt ? mt[0] : -1, b: mt ? mt[1] : -1 }, ambience: amb ? amb[0] : -1 };
}

// Stateful port of one WorldPainterQuery (2C0778 driver + the int class methods), for exactness with any rate.
// Class methods: MusicTrigger 2BCC00 blend / 2BDA98 compare / 2BE0B0 defaults; Mix 2BCCC8 / 2BDAC8 / 2BE0C8;
// Ambience 2BCD68 / 2BDAE0 / 2BE0D8. Object: +0 distance (init -99999), +8 cur, +C last sample (+10/+14 for B).
export class AudioPainterQuery {
  constructor(type) {
    this.type = type; this.key = Object.keys(TYPE_INDEX).find((k) => TYPE_INDEX[k] === type);
    this.pairs = type === 0 ? 2 : 1;
    this.distance = -99999; this.lastX = 0; this.lastY = 0;
    this.cur = new Array(this.pairs).fill(0); this.last = new Array(this.pairs).fill(0);
  }
  reset() { this.distance = 0; this.cur.fill(-1); } // 2BE0B0 / 2BE0C8 / 2BE0D8: +0 = 0, cur = -1 (samples kept)
  matches(values) { for (let k = 0; k < this.pairs; k++) if (this.cur[k] !== values[k]) return false; return true; }
  blend(values, weight) { // cur = cvt.w.s(w^2 * sample + (1 - w^2) * cur); last = sample
    const w2 = mul(weight, weight), inv = sub(1, w2);
    for (let k = 0; k < this.pairs; k++) { this.cur[k] = cvtWS(add(mul(w2, values[k]), mul(inv, this.cur[k]))); this.last[k] = values[k]; }
  }
  // 2C0778 (engine/painter_driver.hpp). Returns this.cur ([value] or [A, B]).
  step(entry, x, y, weight = AUTO_WEIGHT) {
    x = Math.fround(x); y = Math.fround(y);
    const initial = this.distance === -99999;
    if (!initial) { const dx = sub(this.lastX, x), dy = sub(this.lastY, y); this.distance = add(this.distance, Math.fround(Math.sqrt(add(mul(dx, dx), mul(dy, dy))))); }
    this.lastX = x; this.lastY = y;
    const section = entry?.[this.key];
    if (!entry || !section) { if (entry && initial) this.reset(); this.reset(); return this.cur; } // missing region / section
    const p = painterLeafPayload(section, x, y);
    if (p < 0) { this.reset(); return this.cur; }
    const { rate, values } = section.payloads[p];
    if (this.matches(values)) this.distance = 0;
    if (initial) { this.blend(values, -1); this.distance = 0; return this.cur; }
    if (weight !== AUTO_WEIGHT) { this.blend(values, weight); return this.cur; }
    if (rate >= 0 && rate <= this.distance) { this.blend(values, 1); this.distance = 0; } else this.blend(values, -rate);
    return this.cur;
  }
}

// One rider's three audio queries (2867E8 creates MusicTrigger/Ambience/Mix per player side).
export function createAudioPainterState() {
  return { musicTrigger: new AudioPainterQuery(0), mix: new AudioPainterQuery(1), ambience: new AudioPainterQuery(2) };
}
// Per game tick (2898A8). The original queries MusicTrigger only in radio modes 0/1/3 (28D960) and Ambience only
// in mode 2 (2899E8); an unqueried object keeps its old value. Pass modes accordingly (defaults: all).
export function stepAudioPainters(state, data, position, location, { musicTrigger = true, ambience = true } = {}) {
  const x = Array.isArray(position) ? position[0] : position.x, y = Array.isArray(position) ? position[1] : position.y;
  const entry = regionEntry(data, location);
  if (musicTrigger) state.musicTrigger.step(entry, x, y);
  state.mix.step(entry, x, y);
  if (ambience) state.ambience.step(entry, x, y);
  return { mix: state.mix.cur[0], musicTrigger: { a: state.musicTrigger.cur[0], b: state.musicTrigger.cur[1] }, ambience: state.ambience.cur[0] };
}

// Browser helper: three.js world metres (Y-up) -> painter source cm.
export const sourceXY = (world) => ({ x: Math.fround(world.x * 100), y: Math.fround(-world.z * 100) });
