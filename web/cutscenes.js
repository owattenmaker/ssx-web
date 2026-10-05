// The original in-engine cutscenes ("NIS" scripts, DATA/SCRIPTS/SCDAT.BIG): event intros in the start hut, podium
// celebrations, rival challenges, transport rides, lodge/hub arrivals and the backcountry heli drops.
// docs/cutscenes.md; assets from tools/export_cutscenes.py (web/public/assets/CUTSCENES/, git-ignored).
//
// Engine (SLUS_207.72, docs/cutscenes.md "Engine"): a script is a set of tracks; one alternative object per track
// is used per playback (0x279A70). Objects: cameras (kind 0 Manual / 1 Target / 2 Subject), the cut list (4),
// actors = riders (5) and one audio/control object (7). Time is in 60 Hz game ticks (0x274A30); every position is
// in an anchor frame (0x27A0D8: start gate, podium steps, grid nodes, heli/gondola locators ...) followed by the
// object's ext offset. Actors play clips of the script's own AFL bank (clip time = (start + (t - t0) x speed) / 60 s,
// 30 fps, looping at (frames - 1) / 30), their NIS head/eyes/hands (FE preview parts 5/6/8/9) animated by the clip's
// part streams. Choice lists come from scfilter<LOC>.dat (ScriptChoice 0x27B0C0); queued steps carry flags
// 1 skippable (NISSkip = Cross), 2 dropped with a skip, 8 hold/loop until advanced (0x278E50 .. 0x2792E0).
//
// API: playCutscene({kind, id, rider, location, ...}) -> Promise<{played, skipped, scripts}>. See createCutscenes().
import { FrontEndPreview, clipSample, channelValue, previewRoot } from './fe-preview.js';
import { inputDevice, keyFor, keyCapWidth, drawKeyCap, LUI_STRETCH, CAP_HEIGHT } from './input-glyphs.js';
import { texture as tslTexture, attribute, vec4, positionGeometry, normalWorld, varying, floor, clamp, round } from 'three/tsl';
import { toFrame } from './frame-space.js';
import { packageTexture } from './texture-archive.js';
import { activePad } from './gamepad.js';
import { pv } from './pv-flags.js';
import { WIDESCREEN_MODES } from './widescreen.js';
import { createStageSet, hideWorldCopy, NOSCRIPT } from './cutscene-stage-sets.js';
import { createPlaneFx } from './cutscene-plane-fx.js';

export const ROOT = '/assets/CUTSCENES/';
export const TICK_HZ = 60;
const DEG = Math.PI / 180;
const HELI_SETS = new Set(['ABC1', 'DBC2', 'EBC3']);   // backcountry locations with a locator-5 heli set (SETS/<LOC>HELI)
// CHARDB order: the filter / character-mask bit of a rider (docs/characters.md).
export const RIDER_BITS = Object.freeze(['moby', 'kaori', 'allegra', 'mac', 'zoe', 'griff', 'elise', 'nate', 'psymon', 'viggo']);
// scfilter choice lists (0x481E48 .., docs/cutscenes.md "Selection").
export const GROUP = Object.freeze({ MIDWAY: 0, MIDWAY_RIDER: 1, FLYOVER: 2, APPROACH: 3, GATE_VAR: 4, GATE_IDLE: 5,
  PODIUM: 6, PODIUM_2ND: 7, PODIUM_3RD: 8, PODIUM_WIN: 9, PODIUM_CHEAT: 10, TRANSPORT_ARRIVE: 11, HELI_DEP: 12,
  HELI_INAIR: 13, HELI_INAIR_RIDER: 14, HELI_INAIR_MP: 15, BC_ARRIVE: 16, BC_ARRIVE_RIDER: 17, GOND_DEP: 18,
  GOND_INAIR: 19, GOND_INAIR_RIDER: 20, GOND_INAIR_MP: 21, LODGE: 22, LODGE_2: 23, RIVAL_LOCATION: 24, RIVAL_RIDER: 25 });
export const FLAG = Object.freeze({ SKIP: 1, CHAIN: 2, PAUSE: 4, HOLD: 8 });
export const FMV = Object.freeze({ 29: 'ABC1', 30: 'DBC2', 31: 'EBC3' });   // list ids 29..31 (0x278E20)

// ---------------------------------------------------------------------------------------------------------------
// Pure evaluation (tested by web/test-cutscenes.mjs against the live PS2 camera of script 89 / 73).

// Piecewise cubic (channel type 0): flat [t, a, b, c, d]*; the last key with key.t <= t, u = t - key.t.
export function curveAt(k, t) {
  let at = 0;
  for (let i = 5; i < k.length; i += 5) { if (k[i] <= t) at = i; else break; }
  const u = t - k[at];
  return ((k[at + 1] * u + k[at + 2]) * u + k[at + 3]) * u + k[at + 4];
}
const rz = (a) => { const c = Math.cos(a), s = Math.sin(a); return [c, -s, 0, s, c, 0, 0, 0, 1]; };
const ry = (a) => { const c = Math.cos(a), s = Math.sin(a); return [c, 0, s, 0, 1, 0, -s, 0, c]; };
const rx = (a) => { const c = Math.cos(a), s = Math.sin(a); return [1, 0, 0, 0, c, -s, 0, s, c]; };
const mul3 = (a, b) => {
  const o = new Array(9);
  for (let i = 0; i < 3; i++)
    for (let j = 0; j < 3; j++) o[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
  return o;
};
const app3 = (m, p) => [m[0] * p[0] + m[1] * p[1] + m[2] * p[2], m[3] * p[0] + m[4] * p[1] + m[5] * p[2], m[6] * p[0] + m[7] * p[1] + m[8] * p[2]];

// Object frame (camera 0x27D970, actor 0x27F9F8): p_world = Rz(yaw)Ry(-pitch)[Rz(e.yaw)Ry(-e.pitch)Rx(-e.roll)p + e.t] + pos.
// anchor {pos: [x, y, z] cm (PS2 world, z up), yaw, pitch (radians)}; offset = ext.offset {translate, yaw/pitch/roll deg
// (actors: yaw, rot_y, rot_x)}. Returns {R (3x3 row-major), T, yaw, pitch, roll}.
export function objectFrame(anchor, offset = {}) {
  const ey = (offset.yaw ?? 0) * DEG, ep = (offset.pitch ?? offset.rot_y ?? 0) * DEG, er = (offset.roll ?? offset.rot_x ?? 0) * DEG;
  const A = mul3(rz(anchor.yaw || 0), ry(-(anchor.pitch || 0)));
  const R = mul3(A, mul3(mul3(rz(ey), ry(-ep)), rx(-er)));
  const t = app3(A, offset.translate || [0, 0, 0]);
  return { R, T: [t[0] + anchor.pos[0], t[1] + anchor.pos[1], t[2] + anchor.pos[2]], yaw: ey + (anchor.yaw || 0), pitch: ep + (anchor.pitch || 0), roll: er };
}
export const framePoint = (f, p) => { const q = app3(f.R, p); return [q[0] + f.T[0], q[1] + f.T[1], q[2] + f.T[2]]; };

// Camera kinds 0-2 at tick t (FORMAT: Manual eye/target, Target target+distance/pitch/yaw, Subject eye + subject).
// Returns {eye, target, roll (rad), fov (half-horizontal of a 4:3 view, rad), near, far} in PS2 cm.
export function cameraAt(object, frame, t, subject = null) {
  const c = object.ch.map((ch) => (ch.t === 'c' ? curveAt(ch.k, t) : 0)), e = object.ext;
  let eye, target, roll, fov;
  if (object.kind === 0) { eye = framePoint(frame, c.slice(0, 3)); target = framePoint(frame, c.slice(3, 6)); roll = c[6]; fov = c[7]; }
  else if (object.kind === 1) {
    target = framePoint(frame, c.slice(0, 3));
    const yaw = c[5] * DEG + frame.yaw, pitch = c[4] * DEG + frame.pitch, d = c[3];
    eye = [target[0] - d * Math.cos(pitch) * Math.cos(yaw), target[1] - d * Math.cos(pitch) * Math.sin(yaw), target[2] - d * Math.sin(pitch)];
    roll = c[6]; fov = c[7];
  } else {
    eye = framePoint(frame, c.slice(0, 3));
    if (subject) {
      const s = subject.scale ?? 1;
      target = [0, 1, 2].map((i) => subject.pos[i] + subject.forward[i] * c[3] * s + subject.up[i] * c[4] * s);
    } else target = framePoint(frame, [0, 0, 100]);
    roll = c[5]; fov = c[6];
  }
  // 0x15E668: fov clamped to [0, pi/4], near >= 30 cm, far <= 30000 cm.
  return { eye, target, roll: roll * DEG + frame.roll, fov: Math.min(Math.max(fov * DEG, 0), Math.PI / 4),
    near: Math.max(e.near ?? 30, 30), far: Math.min(e.far ?? 30000, 30000) };
}

// Actor root at tick t (0x1241C0): position (x, y, z) in the actor frame; rotation = Euler(rotY + pitch, rotX + roll)
// then yaw rotZ + frame yaw - 90 deg about Z (PS2 quaternion [x, y, z, w]).
export function actorRootAt(object, frame, t) {
  const c = object.ch, v = (i) => (c[i]?.t === 'c' ? curveAt(c[i].k, t) : 0);
  const pos = framePoint(frame, [v(3), v(4), v(5)]);
  const yaw = v(8) * DEG + frame.yaw - Math.PI / 2, pitch = v(7) * DEG + frame.pitch, roll = v(6) * DEG + frame.roll;
  const q = qmul(qmul(qaxis(2, yaw), qaxis(1, pitch)), qaxis(0, roll));
  return { pos, quat: q, yaw };
}
// Live-actor anchor 40 + subject (0x27A0D8 -> 279F18 -> 27B948) of an actor at tick t: position = rider+0x110 through the rider
// interface (vt+0x2C 0x1408F0), which the NIS actor update writes with its root (0x1237E4: frame matrix x the curve position);
// yaw / pitch = heading / elevation of the physical forward rider+0x1B0 (11E098 of the root quaternion: +Y), roll 0.
// the view tangents (half width, half height of the page's 3D area) of a NIS camera with half-angle tangent
// t. 376C58 builds the GS scales X = 0.5 w / t x (+0x6BA0), Y = X x 1.3333 h / w x (+0x6BA4) from the render context's block
// (+0x6B94 mode, +0x6B98 top, +0x6B9C height, +0x6BA0 / +0x6BA4 scales; the player's Widescreen mode, web/widescreen.js). A NIS
// letterbox (2EAA28) saves that block and targets {mode 1, top 0.125, height 0.75, 0.75, 0.63} (Anamorphic: {mode 2, 0, 1,
// 0.75, 0.63}); 2EA900 slides the four floats linearly with the bars (f 0..1) and 2EA820 restores the block. So a letterboxed
// step has P11 / P00 = 0.84 / 0.75 = 1.12 (PS2 states cam5a, nis153, peak2-arr: 0.75 / 0.63; slide states in between), an idle
// step the player's own block (state-round-objectives: 1 / 1). The page draws the player's band (448 lines, 16:9: 336).
// a lit static-model instance (instance flag 0x4000; tools/export_cutscene_sets.py lighting_bank). 2F5400 gives the
// instance ten irradiance rows (here the location's object bank; no local light within 10 m of the helipads), VU1 program 3
// scales them by 128 at 0x2170 (w: 128 on the constant row) and at 0x8B8 evaluates per vertex, on the ITOF15 normal through
// the node's rotation, L = FTOI0(clamp(r0 + r1 x^2 + r2 y^2 + r3 z^2 + r4 xy + r5 zx + r6 yz + r7 x + r8 y + r9 z, 0, 255)) in
// source axes; L replaces the baked colour in TFX MODULATE: Cs = T x L >> 7, As = Ta x L.a >> 7 (bytes, Gouraud).
// The same per-vertex L on the CPU (tests, QA): n = the transformed normal in source axes -> [r, g, b, a] bytes (FTOI0).
export function litVertexColour(lighting, [x, y, z]) {
  const w = [1, x * x, y * y, z * z, x * y, z * x, y * z, x, y, z], out = [0, 0, 0, 128];
  for (let lane = 0; lane < 3; lane++) {
    let v = 0;
    for (let i = 0; i < 10; i++) v += Math.fround(lighting.rows[i][lane] * lighting.scale) * w[i];
    out[lane] = Math.floor(Math.min(255, Math.max(0, v)));
  }
  return out;
}
// pv setBlendClass (docs/visual-parity.md section 46): a cutscene set's instances are world static models, drawn by 37E238 with the
// material state of 37F2A4..37F7E0. tools/export_cutscene_sets.py static_model_class gives a batch its class (`blend`, as
// web/prepare.py does for the world): material word +0x0C (group flag bit 3 adds 0x40000) & 0x660000 =
// - 0x20000, class 1: ALPHA 0x44, ATST GREATER 92 (37F604);
// - 0x40000 / 0x60000, class 2: ALPHA 0x44, ATST GREATER 20 (37F6B8 -> 37F750..37F7E0, AREF from the 0x14000 at 37F208);
// - every test is AFAIL FB_ONLY and ZTST GEQUAL, and Z is written: ZMSK is set only for an additive model (header +0x10 bit 3,
//   37ECA0..37ED18), which no set has.
// Class 2 is depth sorted per node: the render-list key (word2 bits 10..28, 364240) is the view depth of the node's world origin (the
// node x instance matrix of 37ED9C..37F078, row 3 read at 37F6E8), so a model's panels draw back to front. The export splits such a
// batch per node with that origin (`sort_pivot`); the mesh's bounding sphere is centred there, which is the point three sorts by.
// The TRANSP gondola cabin's walls and windows are 14 class-2 nodes (texture 10, word 0x70001; walls alpha 128, windows 74).
// Without this the batch drew in one blended pass without depth writes, so from outside its far walls covered the near ones.
const SET_AREF = [0, 92, 20];
// The two passes of web/world-material.js: texels above AREF blend and write depth; the fringe at or below it blends without depth.
function setBlendPasses(m, aref) {
  const reference = aref / 128;
  const alpha = m.colorNode.a;
  m.transparent = true;
  m.depthWrite = true;
  m.alphaTest = reference;
  const fringe = m.clone();
  fringe.colorNode = m.colorNode;
  fringe.alphaTest = 0;
  fringe.depthWrite = false;
  fringe.maskNode = alpha.lessThanEqual(reference);
  return [m, fringe];
}
// The sort point of a depth-sorted node batch: its node origin, with the radius of the batch's vertices around it.
function sortSphere(T, pivot, vertices, indices) {
  const centre = new T.Vector3(...pivot);
  let radius = 0;
  for (const k of indices) {
    const dx = vertices[k * 10] - centre.x;
    const dy = vertices[k * 10 + 1] - centre.y;
    const dz = vertices[k * 10 + 2] - centre.z;
    radius = Math.max(radius, Math.hypot(dx, dy, dz));
  }
  return new T.Sphere(centre, radius);
}
// The static-model MODULATE on GS bytes (web/world-material.js modelColour): Cs = T x (c5 << 3) >> 7, c5 = colour x 31.
const modulateBytes = (tex, col) => toFrame(clamp(floor(round(tex.rgb.mul(255)).mul(round(col.rgb.mul(31)).mul(8)).div(128)), 0, 255).div(255));
export function litInstanceColour(tex, lighting) {
  const rows = lighting.rows.map((r) => vec4(...r.slice(0, 3).map((x) => Math.fround(x * lighting.scale)), 0));   // 0x2170: MULi.xyz, w = 0
  const n = normalWorld, x = n.x, y = n.z.negate(), z = n.y;   // native (x, z, -y) -> source (x, y, z)
  const w = [x.mul(x), y.mul(y), z.mul(z), x.mul(y), z.mul(x), y.mul(z), x, y, z];
  let acc = rows[0]; for (let i = 1; i < 10; i++) acc = acc.add(rows[i].mul(w[i - 1]));
  const L = varying(floor(clamp(acc.add(vec4(0, 0, 0, 128)), 0, 255)));   // per vertex; w = 128 (the constant row's)
  const T8 = round(tex.mul(255));
  return vec4(toFrame(clamp(floor(T8.rgb.mul(L.rgb).div(128)), 0, 255).div(255)), clamp(floor(T8.a.mul(L.a).div(128)), 0, 255).div(255));
}
export function nisTangents(t, mode, f) {
  const m = WIDESCREEN_MODES[mode] ?? WIDESCREEN_MODES[0];
  const sx = m.scaleX + (0.75 - m.scaleX) * f, sy = m.scaleY + (0.63 - m.scaleY) * f;
  return [t / sx, (224 * m.height) * t / (256 * (4 / 3) * (448 / 512) * sy)];
}
export function liveActorAnchor(object, frame, t) {
  const r = actorRootAt(object, frame, t), f = qrot(r.quat, [0, 1, 0]);
  return { pos: r.pos, yaw: Math.atan2(f[1], f[0]), pitch: Math.atan2(f[2], Math.hypot(f[0], f[1])) };
}
// The frame a camera on a live actor takes when it is cut to (27D850 -> 27D970 at the camera's activation): the actor bound to
// anchor - 40 + 1, as it is at tick t; null for a camera on any other anchor (its frame is fixed at the step start).
export function liveCutFrame(cam, actors, t) {
  const id = cam.ext?.anchor; if (!(id >= 40 && id <= 60)) return null;
  const a = actors.find((x) => x.object.ext.binding === id - 40 + 1);
  return a?.frame ? objectFrame(liveActorAnchor(a.object, a.frame, t), cam.ext.offset) : null;
}
function qrot(q, v) {   // q (x, y, z, w) applied to v
  const [x, y, z, w] = q, tx = 2 * (y * v[2] - z * v[1]), ty = 2 * (z * v[0] - x * v[2]), tz = 2 * (x * v[1] - y * v[0]);
  return [v[0] + w * tx + y * tz - z * ty, v[1] + w * ty + z * tx - x * tz, v[2] + w * tz + x * ty - y * tx];
}
function qaxis(axis, a) { const s = Math.sin(a / 2), q = [0, 0, 0, Math.cos(a / 2)]; q[axis] = s; return q; }
function qmul(a, b) {
  return [a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
    a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
}

// Cut list (kind 4): the item active at t (t0 <= t < t1) -> camera id; the last item holds at the end.
export function activeCut(cuts, t) {
  let hit = null;
  for (const it of cuts.ch[0].i) if (it.t0 <= t && t < it.t1) hit = it;
  if (!hit && cuts.ch[0].i.length) { const last = cuts.ch[0].i.reduce((a, b) => (b.t1 > a.t1 ? b : a)); if (t >= last.t1) hit = last; }
  return hit;
}

// Clip time of a type-1 range item at tick t (0x124788): seconds.
export const clipSeconds = (item, t) => (item.o + (t - item.t0) * item.sp) / TICK_HZ;

// ScriptChoice 0x27B0C0: entries whose non-zero masks all intersect the condition words; the least played; rand % n.
// words[21]: 0-2 roles, 3-4 humans, 5-9 AI, 10-14 list B, 15-20 race riders (1 << CHARDB, 0 = nobody).
export function chooseScript(list, words, playCount = {}, random = Math.random) {
  const ok = (list || []).filter((e) => e.masks.every((m, k) => !m || (m & (words[k] || 0))));
  if (!ok.length) return null;
  const least = Math.min(...ok.map((e) => playCount[e.script] || 0));
  const pool = ok.filter((e) => (playCount[e.script] || 0) === least);
  return pool[Math.floor(random() * pool.length) % pool.length].script;
}
export const riderBit = (character) => (character >= 0 && character < 10 ? 1 << character : 0);

// Which rider an actor binding id resolves to (0x280458 -> 0x279F18: binding = subject + 1).
// cast: {roles: [..3], humans: [..2], ai: [..5], race: [..6]} of rider records or null.
export function bindingRider(cast, binding) {
  const s = binding - 1;
  if (s < 0) return cast.humans?.[0] ?? null;
  if (s <= 2) return cast.roles?.[s] ?? null;
  if (s <= 4) return cast.humans?.[s - 3] ?? null;
  if (s <= 9) return cast.ai?.[s - 5] ?? null;
  if (s <= 14) return cast.ai?.[s - 10] ?? null;         // list C+0x5C falls back to the AI riders in single player
  if (s <= 20) return cast.race?.[s - 15] ?? null;
  return null;
}
export const subjectRider = (cast, subject) => bindingRider(cast, subject + 1);
// The condition words of a cast (0x27BDB8).
export function castWords(cast) {
  const w = new Array(21).fill(0), bit = (r) => riderBit(r?.character ?? -1);
  for (let i = 0; i < 3; i++) w[i] = bit(cast.roles?.[i]);
  for (let i = 0; i < 2; i++) w[3 + i] = bit(cast.humans?.[i]);
  for (let i = 0; i < 5; i++) { w[5 + i] = bit(cast.ai?.[i]); w[10 + i] = bit(cast.ai?.[i]); }
  for (let i = 0; i < 6; i++) w[15 + i] = bit(cast.race?.[i]);
  return w;
}

// The alternative of each track (0x279A70): actor tracks with >= 2 alternatives keep those whose character mask
// matches the bound rider (0 = any); one is picked with rand % n.
export function pickAlternatives(script, cast, random = Math.random) {
  return script.tracks.map((alts) => {
    let pool = alts;
    if (alts.length > 1 && alts[0].kind === 5) {
      const rider = bindingRider(cast, alts[0].ext.binding), bit = riderBit(rider?.character ?? -1);
      const mask = (a) => parseInt(a.ext.character_mask, 16) || 0;
      const fit = alts.filter((a) => !mask(a) || (mask(a) & bit));
      if (fit.length) pool = fit;
    }
    return pool[Math.floor(random() * pool.length) % pool.length];
  });
}

// Container copy of script `number` at `location` (docs/cutscenes.md "Files"): the location's scdat when it has
// one (its indices refer to the combined bank), else the resident scdat_main, else the standalone group.
export function scriptContainer(entry, location, hub = null) {
  const c = entry.containers;
  for (const name of [`scdat_${location}`, hub && `scdat_${hub}`, 'scdat_main', String(entry.number).padStart(8, '0')])
    if (name && c.includes(name)) return name;
  return c[0];
}

// Anchor frame of an anchor id (0x27A0D8, jump table 0x481D00) from CUTSCENES/locators.json.
// ctx: {locations (locators.json .locations), location, loop, subjectSlot(subject) -> start-grid slot, snap(pos) -> z | null}.
const LOCATOR_OF = { 19: 7, 23: 0, 24: 8, 25: 1, 26: 2, 27: 3, 28: 4, 29: 5, 30: 6, 31: 9, 32: 10, 33: 11, 34: 12, 35: 13, 36: 14, 37: 15, 38: 16, 39: 17 };
const SNAPPED = new Set([19, 23, 25, 26, 27, 28, 31, 34, 35, 36]);
export function anchorFor(id, ctx) {
  const origin = { pos: [0, 0, 0], yaw: 0, pitch: 0 };
  if (!id || id > 60) return origin;
  const loc = ctx.locations?.[ctx.location];
  if (id <= 18) {   // start-grid node of subject id + 2 (0x27B750: AIP kind-0 row of the rider's start slot)
    const slot = ctx.subjectSlot?.(id + 2); const g = loc?.start_grid?.find((r) => r.slot === slot);
    return g ? { pos: g.pos.slice(), yaw: g.yaw, pitch: g.pitch } : null;
  }
  if (id >= 40) return ctx.liveActor?.(id - 40) || null;
  if (id >= 20 && id <= 22) {   // podium steps: locator 0 + (-20,0,340) / (-20,-275,280) / (-20,275,280) (0x4D3770)
    const l = loc?.locators?.['0']; if (!l) return null;
    return { pos: l.podium_steps[String(id)].slice(), yaw: l.yaw, pitch: l.pitch };
  }
  let set = loc;
  if ((id === 29 || id === 30) && ctx.loop) set = ctx.locations?.TRANSP || loc;   // looping scripts: locator set 43
  const l = set?.locators?.[String(LOCATOR_OF[id])]; if (!l) return null;
  const pos = l.pos.slice();
  if (id === 25) pos[2] -= 1000;
  if (SNAPPED.has(id)) { const z = ctx.snap?.(pos, l.pos); if (z != null && Number.isFinite(z)) pos[2] = z; }
  return { pos, yaw: l.yaw, pitch: l.pitch };
}

// PS2 view matrix of a camera pose (row vectors; x right, y up, z depth; the FE view_matrix_bits convention) in the
// scene-relative PS2 frame the FE lighting uses (hips = scene position x 100): 0x389CB8's rim needs the real camera.
export function viewMatrix(pose, origin) {
  const o = [origin.x * 100, -origin.z * 100, origin.y * 100], e = [0, 1, 2].map((i) => pose.eye[i] - o[i]);
  let f = [0, 1, 2].map((i) => pose.target[i] - pose.eye[i]); const fl = Math.hypot(...f) || 1; f = f.map((x) => x / fl);
  let r = [f[1] * 1 - f[2] * 0, f[2] * 0 - f[0] * 1, 0]; const rl = Math.hypot(...r) || 1; r = r.map((x) => x / rl);   // f x (0, 0, 1)
  let u = [r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]];                          // r x f
  if (pose.roll) { const c = Math.cos(pose.roll), s = Math.sin(pose.roll); const r2 = r.map((x, i) => x * c + u[i] * s), u2 = u.map((x, i) => u[i] * c - r[i] * s); r = r2; u = u2; }
  const dot = (a) => a[0] * e[0] + a[1] * e[1] + a[2] * e[2];
  return new Float32Array([r[0], u[0], f[0], 0, r[1], u[1], f[1], 0, r[2], u[2], f[2], 0, -dot(r), -dot(u), -dot(f), 1]);
}

// Course index (profile / free-ride course, stream slot order 0x442168) -> location code of its scfilter / scdat.
export const COURSE_CODES = Object.freeze(['ARA1', 'BRA2', 'CRA3', 'DRA4', 'ERA5', 'ASS1', 'DSS2', 'ESS3', 'ABA1', 'CBA2', 'EBA3',
  'BHP1', 'CHP2', 'EHP3', 'ABC1', 'DBC2', 'EBC3', 'A', 'B', 'C', 'D', 'E']);

// PS2 world cm (z up) -> three.js scene metres relative to the course origin (web/main.js: (x, z, -y) / 100).
export const toScene = (p, origin) => [p[0] / 100 - origin.x, p[2] / 100 - origin.y, -p[1] / 100 - origin.z];

// A last full-screen draw that sets the frame's alpha to 1 and keeps its colour (blend Zero / One on RGB, One / Zero on alpha):
// acrossLoop, see acrossSwitch.
export function opaqueAlphaFill(T) {
  const g = new T.BufferGeometry(); g.setAttribute('position', new T.Float32BufferAttribute([-1, -1, 0, 3, -1, 0, -1, 3, 0], 3));
  const m = new T.MeshBasicNodeMaterial({ transparent: true, depthTest: false, depthWrite: false, fog: false, toneMapped: false, side: T.DoubleSide });
  m.vertexNode = vec4(positionGeometry.xy, 0, 1); m.fragmentNode = vec4(0, 0, 0, 1);
  m.blending = T.CustomBlending; m.blendEquation = T.AddEquation; m.blendSrc = T.ZeroFactor; m.blendDst = T.OneFactor; m.blendSrcAlpha = T.OneFactor; m.blendDstAlpha = T.ZeroFactor;
  const mesh = new T.Mesh(g, m); mesh.frustumCulled = false; mesh.renderOrder = 1e9; mesh.name = 'cutscene opaque alpha';
  return mesh;
}

// ---------------------------------------------------------------------------------------------------------------
// Runtime

const once = new Map();
async function fetchJson(url) { if (!once.has(url)) once.set(url, fetch(url).then((r) => { if (!r.ok) throw Error(`${url}: ${r.status}`); return r.json(); })); return once.get(url); }
async function fetchBuffer(url) { if (!once.has(url)) once.set(url, fetch(url).then((r) => { if (!r.ok) throw Error(`${url}: ${r.status}`); return r.arrayBuffer(); })); return once.get(url); }

// One loaded script copy with its bank data.
async function loadScript(index, number, location, hub) {
  const entry = index.scripts[number]; if (!entry) throw Error(`No cutscene script ${number}`);
  const container = scriptContainer(entry, location, hub);
  const script = await fetchJson(`${ROOT}scripts/${container}.${String(number).padStart(3, '0')}.json`);
  const info = index.containers[container] || {};
  let clips = null, samples = null;
  if (info.anim) { const lib = await fetchJson(ROOT + info.anim.file); clips = lib.clips; samples = new Float32Array(await fetchBuffer(ROOT + info.anim.file.replace(/\.json$/, '.f32'))); }
  return { number, name: entry.name, container, script, clips, samples, bank: info.bank ? container : null };
}

// A step of a list player: one script instance with its chosen alternatives, actors and clock.
class Sequence {
  constructor(loaded, cast, random, alts = null) {
    this.loaded = loaded; this.script = loaded.script; this.t = 0; this.done = false;
    this.objects = pickAlternatives(this.script, cast, random);
    if (alts) alts.forEach((k, i) => { if (k != null && this.script.tracks[i]?.[k]) this.objects[i] = this.script.tracks[i][k]; });   // QA: forced alternatives
    this.duration = Math.max(1, ...this.objects.map((o) => o.dur || 0));
    this.loop = !!this.script.loop;
    this.cameras = this.objects.filter((o) => o.kind <= 3);
    this.cuts = this.objects.find((o) => o.kind === 4) || null;
    this.control = this.objects.find((o) => o.kind === 7) || null;
    this.actors = this.objects
      .filter((o) => o.kind === 5)
      .map((o) => ({
        object: o,
        rider: bindingRider(cast, o.ext.binding),
        model: null,
        frame: null,
        root: null,
        fired: new Set(),
        voices: new Map()
      }));
    this.fired = new Set(); this.voices = new Map();
  }
}

// Screen fade of a step at tick t (0x277980 -> 0x2E4370 / 0x2E44F0; see fadeAlpha): {alpha, colour}. A list's first step
// fades in with its own fade-in record; a later step with the previous step's fade-out record's in ticks (PS2: the CTM
// approach is bright at t3 after the fly-over, whose fade-out in = 0). The fade-out's out ticks run before the end of a
// step that plays once (a looping script played one cycle too), or before a held step's known release.
// loops: the number of times a looping step (idle, flag 8) wrapped; its fade-in belongs to the step's first pass only (loopFadeOnce:
// the start-gate idle under a heat's card faded back in from black on every loop, PS2 menus/race/r3-rr-dense: a steady picture).
export function fadeAt({ script, t, duration, prevFadeOut = null, step = null, held = 0, loops = 0 }) {
  const fo = script.fade_out, fi = prevFadeOut ? (prevFadeOut.type === 1 ? { type: 1, colour: prevFadeOut.colour, in_ticks: prevFadeOut.in_ticks } : null) : script.fade_in;
  let alpha = 0, colour = '#000';
  if (fi && fi.type === 1 && fi.in_ticks > 0 && t < fi.in_ticks && !loops) { alpha = 1 - t / fi.in_ticks; colour = fi.colour === 'white' ? '#fff' : '#000'; }
  const hold = step?.flags & FLAG.HOLD;
  if (fo && fo.type === 1 && !step?.idle && (!hold || step.holdTicks) && fo.out_ticks > 0) {
    const left = hold ? step.holdTicks - held : duration - t;
    if (left < fo.out_ticks && 1 - left / fo.out_ticks > alpha) { alpha = 1 - left / fo.out_ticks; colour = fo.colour === 'white' ? '#fff' : '#000'; }
  }
  return { alpha, colour };
}

export function createCutscenes(host) {
  // host: {T, scene, camera, core, origin, compile(group), audio (web/game-audio.js), ui, anchor(id, info) -> frame|null,
  //        cast() -> {roles, humans, ai, race}, location() -> code, hub() -> code|null, onActive(bool)}
  const T = host.T;
  let index = null, active = null, pending = Promise.resolve(); const playCount = {};
  const pool = [];            // FrontEndPreview instances (actors), reused
  const listeners = new Set();

  async function ensureIndex() { index ??= await fetchJson(ROOT + 'index.json'); locators ??= await fetchJson(ROOT + 'locators.json').catch(() => null); return index; }

  function filterList(location, group) { return index?.filters?.[location]?.[group] || null; }

  // Resolve queued steps [{group | script, flags}] to script numbers (ScriptChoice), dropping empty lists.
  function resolve(steps, cast, location) {
    const words = castWords(cast), out = [];
    for (const s of steps) {
      if (s.fmv) { out.push(s); continue; }
      let number = s.script ?? null;
      if (number == null) { number = api.qaScripts?.[s.group] ?? chooseScript(filterList(s.location ?? location, s.group), words, playCount); }   // qaScripts: PS2 comparisons
      if (number == null) continue;
      playCount[number] = (playCount[number] || 0) + 1;
      out.push({ ...s, script: number });
    }
    return out;
  }

  async function actorModel(rider) {
    const entry = rider?.entry; const root = previewRoot(entry); if (!root) return null;
    let fp = pool.find((p) => !p.busy && p.root === root) || pool.find((p) => !p.busy && !p.root) || null;
    if (!fp) { fp = new FrontEndPreview(); fp.compile = host.compile || null; pool.push(fp); }
    fp.busy = true;
    try { const model = await fp.fetch(T, root); fp.root = root; fp.swap(model); return fp; }
    catch (error) { console.warn('Cutscene actor unavailable', root, error); fp.busy = false; return null; }
  }
  // pv nisPreload: a rider's cast model built into an idle pool entry ahead of its cut (actorModel then takes that entry from its cache)
  async function preloadActor(rider) {
    const root = previewRoot(rider?.entry);
    if (!root || pool.some((p) => p.root === root)) return;
    let fp = pool.find((p) => !p.busy && !p.root);
    if (!fp) { fp = new FrontEndPreview(); fp.compile = host.compile || null; pool.push(fp); }
    try {
      const model = await fp.fetch(T, root);
      if (!fp.busy && !fp.root) { fp.root = root; fp.swap(model); }
    } catch (e) { console.warn('Cutscene actor preload failed', root, e); }
  }
  // Keep at most 8 idle actor models (the next event usually reuses the same riders); dispose the rest.
  function trimPool() {
    const idle = pool.filter((p) => !p.busy);
    for (const p of idle.slice(0, Math.max(0, idle.length - 8))) {
      for (const m of p.cache.values()) { m.group.parent?.remove(m.group); m.dispose(); }
      p.cache.clear(); p.model = null; p.root = null; pool.splice(pool.indexOf(p), 1);
    }
  }
  function releaseActors(seq) {
    for (const a of seq.actors) {
      if (a.model?.model?.pda) a.model.model.pda.visible = false;
      if (a.model) {
        a.model.show(false);
        a.model.model?.group.parent?.remove(a.model.model.group);
        a.model.busy = false;
      }
      a.model = null;
      stopVoices(a.voices);
    }
    stopVoices(seq.voices);
  }

  // ---- props: the handheld (board_PDA_NIS, rider slot 11) of the station scenes, tools/export_cutscene_props.py.
  // Skinned rigidly to the FE skeleton's handright (file 0 bone 15); shown by clip event id 0, hidden by id 1.
  let pdaJob = null;
  function pdaAsset() {
    pdaJob ??= (async () => {
      const base = `${ROOT}PROPS/pda/`, d = await fetchJson(base + 'prop.json');
      const [vb, ib, mb] = await Promise.all([fetchBuffer(base + 'vertices.bin'), fetchBuffer(base + 'indices.bin'), fetchBuffer(base + 'morphs.bin').catch(() => new ArrayBuffer(0))]);
      const map = await new T.TextureLoader().loadAsync(base + d.texture.path);
      map.flipY = false; map.colorSpace = T.NoColorSpace;
      const inter = new T.InterleavedBuffer(new Float32Array(vb), 10), g = new T.BufferGeometry(), n = d.vertex_count;
      g.setAttribute('position', new T.InterleavedBufferAttribute(inter, 3, 0)); g.setAttribute('normal', new T.InterleavedBufferAttribute(inter, 3, 3));
      g.setAttribute('uv', new T.InterleavedBufferAttribute(inter, 2, 6)); g.setIndex(new T.BufferAttribute(new Uint32Array(ib), 1));
      if (d.morphs?.length && mb.byteLength) {
        g.morphAttributes.position = d.morphs.map((x) => new T.BufferAttribute(new Float32Array(mb.slice(x.offset, x.offset + n * 12)), 3));
        g.morphTargetsRelative = true;
      }
      const m = new T.MeshBasicNodeMaterial({ side: T.DoubleSide, alphaTest: 0.35, fog: false });
      const tex = tslTexture(map); m.colorNode = vec4(toFrame(clamp(round(tex.rgb.mul(255)).mul(2), 0, 255).div(255)), tex.a);   // PS2 texels (128 = 1.0) x 2: bytes or linear light
      return { d, g, m, n };
    })().catch((e) => { console.warn('Cutscene PDA prop unavailable (python3 tools/export_cutscene_props.py)', e); return null; });
    return pdaJob;
  }
  function pdaEvents(clip) {
    const ev = clip?.events || [];
    const show = ev.find((e) => e[1] === 0),
      hide = ev.find((e) => e[1] === 1);
    return show ? { show: show[0], hide: hide ? hide[0] : -1 } : null;
  }
  async function attachPda(a) {
    const fp = a.model, model = fp?.model; if (!model || model.pda) return;
    const asset = await pdaAsset(); if (!asset || model.pda) return;
    const bone = model.rig.bones.findIndex((b) => b.file === asset.d.bone.file && b.index === asset.d.bone.index); if (bone < 0) return;
    const g = asset.g.clone(), idx = new Uint16Array(asset.n * 4), w = new Float32Array(asset.n * 4);
    for (let i = 0; i < asset.n; i++) { idx[i * 4] = bone; w[i * 4] = 1; }
    g.setAttribute('skinIndex', new T.BufferAttribute(idx, 4)); g.setAttribute('skinWeight', new T.BufferAttribute(w, 4));
    const mesh = new T.SkinnedMesh(g, asset.m); mesh.frustumCulled = false; mesh.renderOrder = 600; mesh.visible = false;
    model.group.add(mesh); mesh.bind(model.meshes[0].skeleton, new T.Matrix4()); mesh.updateMorphTargets?.();
    model.pda = mesh;
  }

  // ---- sets: cutscene-only locations (TRANSP: the tilt-rotor and gondola cabins of the in-air loops),
  // tools/export_cutscene_sets.py. Texture x baked colour (16/31 = x1), alpha test; absolute world metres.
  const sets = new Map();
  function ensureSet(code) {
    if (sets.has(code)) return sets.get(code);
    const job = (async () => {
      const base = `${ROOT}SETS/${code}/`, d = await fetchJson(base + 'world.json');
      const [vb, ib, cb] = await Promise.all(['vertices.bin', 'indices.bin', 'colors.bin'].map((f) => fetchBuffer(base + f)));
      const inter = new T.InterleavedBuffer(new Float32Array(vb), 10), colors = new T.BufferAttribute(new Float32Array(cb), 4), indices = new Uint32Array(ib);
      const loader = new T.TextureLoader(), group = new T.Group(); group.name = `cutscene set ${code}`; group.visible = false;
      const staged = !!d.livecomp, meshes = [];   // a set with a LiveComp record (the midway plane, a backcountry heli): web/cutscene-stage-sets.js
      for (const b of d.batches) {
        const t = d.textures[`9-${b.texture}`]; const map = await packageTexture(loader, base, t); // world texture library entry (or an older PNG export)
        map.flipY = false; map.wrapS = map.wrapT = T.RepeatWrapping; map.colorSpace = T.NoColorSpace;
        const g = new T.BufferGeometry();
        g.setAttribute('position', new T.InterleavedBufferAttribute(inter, 3, 0)); g.setAttribute('normal', new T.InterleavedBufferAttribute(inter, 3, 3));
        g.setAttribute('uv', new T.InterleavedBufferAttribute(inter, 2, 6)); g.setAttribute('color', colors);
        g.setIndex(new T.BufferAttribute(indices.slice(b.first_index, b.first_index + b.index_count), 1));
        const m = new T.MeshBasicNodeMaterial({ side: T.DoubleSide, transparent: !!b.has_alpha, alphaTest: b.has_alpha ? 0.02 : 0, fog: false, depthWrite: !b.has_alpha });
        if (d.lighting && staged) { map.colorSpace = T.NoColorSpace; m.colorNode = litInstanceColour(tslTexture(map), d.lighting); }   // a lit instance (the backcountry heli)
        else { const tex = tslTexture(map), col = attribute('color', 'vec4'); m.colorNode = vec4(modulateBytes(tex, col), tex.a.mul(col.a)); }
        // pv setBlendClass: the batch's static-model class (classes 1 / 2; an export without `blend` keeps the one blended pass)
        const aref = b.has_alpha && pv('setBlendClass') ? (SET_AREF[b.blend] ?? 0) : 0;
        let drawn = m;
        if (aref) {
          drawn = setBlendPasses(m, aref);
          g.addGroup(0, b.index_count, 0);
          g.addGroup(0, b.index_count, 1);
          if (b.sort_pivot) g.boundingSphere = sortSphere(T, b.sort_pivot, inter.array, g.index.array);
        }
        const mesh = new T.Mesh(g, drawn); mesh.frustumCulled = false; mesh.renderOrder = 500; group.add(mesh);
        if (staged && b.livecomp_resource !== undefined) { mesh.userData.liveComp = [b.livecomp_resource, b.livecomp_node]; meshes.push(mesh); }
      }
      if (staged) { group.userData.meta = d; group.userData.stage = createStageSet(d, meshes); }
      // pv worldWarm (host.compileHidden, main.js compileFor): compiled before it joins the scene, a two-pass transparent material once per
      // side (BackSide, then FrontSide, as it draws), so its first cut frame builds no pipeline (TRANSP: 5-10 blocking builds at the departure)
      if (host.compileHidden) { try { await host.compileHidden(group); } catch {} group.visible = false; host.scene.add(group); }
      else {
      host.scene.add(group);
      if (host.compile) { group.visible = true; try { await host.compile(group); } catch {} group.visible = false; }
      }
      return group;
    })().catch((e) => { console.warn(`Cutscene set ${code} unavailable (python3 tools/export_cutscene_sets.py)`, e); return null; });
    sets.set(code, job); return job;
  }
  // A staged set (the midway plane) stands in for the world's own copy of its instance, which is hidden meanwhile.
  let stageShown = null, restoreCopy = null;
  function showSets(code) {
    for (const [k, p] of sets) p.then((g) => {
      if (!g) return;
      if (linger && g === linger.group && code == null) return;   // the heli hovering on after its arrival (heliHover)
      g.visible = k === code; g.position.set(-host.origin.x, -host.origin.y, -host.origin.z);
      if (!g.userData.stage) return;
      if (k === code && stageShown !== g) { stageShown = g; restoreCopy?.(); restoreCopy = hideWorldCopy(host.scene, g.userData.meta, g); }
      else if (k !== code && stageShown === g) { g.userData.stage.reset(); stageShown = null; restoreCopy?.(); restoreCopy = null; }
    });
  }
  const stage = () => stageShown?.userData.stage ?? null;
  // the plane's engine loop and snow spray (web/cutscene-plane-fx.js)
  const planeFx = createPlaneFx(host), stagedSet = () => (stageShown?.userData.stage ? { meta: stageShown.userData.meta, stage: stageShown.userData.stage } : null);
  // pv departCalls: a kind-7 channel-0 call (0x2808E8) that no staged set owns runs on the step's hub location, the script's scdat
  // container (27C070 -> 22E098 -> 309E50: that track's stage globals): gond_dep #126 / heli_dep #147 call the station's functions
  // (the depart LiveComp, the loops, the heli spray, SetNodeState); the recorded cleanup runs at the step end (0x2807B0). The world
  // ticks under the Transport (pv nisTick), so the run's entity pass advances what they build. scdat_main (TRANSP: no stage in the
  // streamed worlds' seeds) has no track: its in-air calls stay unported.
  const TRANSPORT_KINDS = new Set(['transport-ride', 'transport-depart', 'transport-loop']), hubCleanups = new Map();
  function hubCall(seq, symbol, cleanup) {
    const m = /^scdat_(.+)$/.exec(seq.loaded?.container || ''), track = m && m[1] !== 'main' ? host.stageTrack?.(m[1]) ?? -1 : -1, c = host.core;
    if (!(track >= 0) || !c?._stage_global_call) return;
    if (c._stage_global_call(track, symbol >>> 0, NaN, NaN, NaN) && cleanup != null && (cleanup >>> 0) !== NOSCRIPT) hubCleanups.set(cleanup >>> 0, track);
  }
  function hubStepEnd() {
    const c = host.core;
    for (const [k, t] of hubCleanups) {
      try {
        c?._stage_global_call?.(t, k, NaN, NaN, NaN);
      } catch (e) {
        console.warn('Cutscene hub cleanup', e);
      }
    }
    hubCleanups.clear();
  }
  // after a backcountry arrival the PS2 heli hovers on at the helipad (its cleanup 0x0AE69AB4 started frames 551..677
  // in loop mode, engine sound 201 looping at the heli); the set stays in for the world's frame-0 copy, advanced once per game tick
  // (api.linger from main.js while no cutscene plays), until the next cutscene, a course change or the camera is 3 km away.
  let linger = null;
  const LINGER_RANGE = 3000;   // metres
  function startLinger() {
    const g = stageShown, st = g?.userData.stage;
    if (g?.userData.meta?.anchor !== 29 || !st?.anim.active.size) return false;
    linger = { group: g, t: 0 }; planeFx.call(stagedSet(), 0x0AE69AB4, NOSCRIPT, true);   // the cleanup's engine loop (owned: the voice)
    return true;
  }
  function endLinger() {
    const l = linger; if (!l) return; linger = null; planeFx.end();
    l.group.visible = false; l.group.userData.stage.reset();
    if (stageShown === l.group) { stageShown = null; restoreCopy?.(); restoreCopy = null; }
  }
  function lingerFrame(dt) {
    const l = linger; if (!l) return;
    const set = stagedSet(); if (!set) { endLinger(); return; }
    const m = set.stage.anim.matrices?.(set.meta.livecomp.instances[0].resource), p = m?.[0]?.[3], cam = host.camera?.position;
    if (p && cam && Math.hypot(p[0] / 100 - host.origin.x - cam.x, p[2] / 100 - host.origin.y - cam.y, -p[1] / 100 - host.origin.z - cam.z) > LINGER_RANGE) { endLinger(); return; }
    l.t += dt * TICK_HZ; set.stage.sync(l.t); set.stage.apply(); planeFx.frame(set);
  }
  // anchor 37 = ABC1 locator 15, the midway plane (#153 abc1_heli_arr_midway, #154-163 heli_arrb_<char>_midwayabc1)
  // anchor 29 outside a loop = the location's locator 5, the backcountry heli (#123 / #124 / #127 <bc>_heli_arr,
  // #128-137 heli_arrb_<char>; tools/export_cutscene_sets.py --helis -> SETS/<LOC>HELI, docs/presentation.md 16)
  const setOf = (seq) => (seq.loop && seq.objects.some((o) => o.ext?.anchor === 29 || o.ext?.anchor === 30) ? 'TRANSP'
    : seq.objects.some((o) => o.ext?.anchor === 37) ? 'ABC1PLANE'
    : !seq.loop && HELI_SETS.has(active?.location) && seq.objects.some((o) => o.ext?.anchor === 29) ? `${active.location}HELI` : null);

  // ---- audio (web/sfx.js through web/game-audio.js; the script's load-group bank on its own slot) ----
  const NIS_SLOT = 0x11;
  function sfx() { return host.audio?.sfx || null; }
  function loadBank(loaded) { const s = sfx(); if (!s || !loaded.bank) return null; return s.loadBank(NIS_SLOT, `../../CUTSCENES/banks/${loaded.bank}`); }
  // 0x280F3C: every NIS sound goes to 287968(audio, 4, 0) = the CHARACTER bus at speaker slot 0's gain, then 2906B8.
  // Positional (actor) sounds pass the actor root in PS2 cm (z up; web/sfx.js spatial), a live array updated per tick.
  // Returns the voice, null when the request failed for good (no bank entry / voices), undefined when the audio is not
  // up yet (the caller retries on the next tick instead of latching a silent voice).
  function playSound(sound, { bus = 'CHARACTER', volume = 127, position = null } = {}) {
    const s = sfx(); if (!s || !host.audio?.engine?.unlocked && host.audio?.engine) return undefined;
    if (!s.bankOf?.(NIS_SLOT)?.bnk) return undefined;   // the step's bank is still decoding: try again next tick
    try { return s.play({ slot: NIS_SLOT, sound, bus, speaker: 0, volume, position, tag: 'nis' }) || null; } catch { return null; }
  }
  function stopVoices(map) { for (const v of map.values()) { try { v?.stop?.(0.05); } catch {} } map.clear(); }

  // ---- per-tick evaluation ----
  let locators = null;
  function anchorFrame(id, seq) {
    const cast = active?.cast || {};
    // anchor 40 + subject = the actor bound to subject + 1 (the heli_arrb_* subject cameras: anchor 43 = binding 4, the human)
    const f =
      host.anchor?.(id, { location: active?.location, loop: seq.loop, cast }) ||
      anchorFor(id, {
        locations: locators?.locations,
        location: active?.location,
        loop: seq.loop,
        subjectSlot: (subject) => {
          const r = subjectRider(cast, subject);
          return r?.slot ?? (subject >= 15 ? subject - 15 : subject === 3 ? 0 : null);
        },
        snap: host.snap,
        liveActor: (i) => {
          const a = seq.actors.find((x) => x.object.ext.binding === i + 1) ?? null;
          return a?.frame ? { pos: a.frame.T, yaw: a.frame.yaw, pitch: a.frame.pitch } : null;
        }
      });
    if (!f) console.warn(`Cutscene anchor ${id} unavailable at ${active?.location}`);
    return f || { pos: [0, 0, 0], yaw: 0, pitch: 0 };
  }
  function prepareSequence(seq) {
    for (const a of seq.actors) a.frame = objectFrame(anchorFrame(a.object.ext.anchor, seq), a.object.ext.offset);
    for (const c of seq.cameras) c.frame = objectFrame(anchorFrame(c.ext.anchor, seq), c.ext.offset);
  }

  // Speech (web/audio-speech.js through game-audio's speech engine): actor events 0x123E30 and the kind-7 PA/DJ cues
  // 0x2A19D8; music codes 0x28E8C0 (21-25 next song) go to game-audio when it offers them.
  const speaker = (r) => ({ id: r && r.character >= 0 && r.character < 10 ? r.character : -1 });
  function riderSpeech(ev, rider, targetWord, seq) {
    const sp = host.audio?.speechEngine; if (!sp || !rider) return;
    const tgt = subjectRider(active?.cast || {}, ((targetWord ?? 3) << 24) >> 24);
    try {
      if (ev === 2) sp.finishLineRider(speaker(rider), active?.place ?? 0);
      else if (ev === 100) sp.bcChallenge(speaker(rider), speaker(tgt));
      else if (ev === 101) sp.hey(speaker(rider), speaker(tgt));
    } catch (e) { console.warn('Cutscene speech', e); }
  }
  function cue(code) {
    const sp = host.audio?.speechEngine; if (!sp) return;
    const loc = COURSE_CODES.indexOf(active?.location);
    try {
      // 0x2A19D8: 0 Sponsor_Intro, 1 Rider_Intro (freestyle approaches), 2 Rider_Race_Intro (race approaches), 3 Medal_Run_Intro,
      // 7 Medals (every podium scene), 0xC Event_Intro, 0xF Venue_Intro (the fly-overs); the subject is the first human.
      const human = speaker(active?.cast?.humans?.[0]);
      if (code === 0) sp.sponsorIntro(); else if (code === 1) sp.riderIntro?.(human); else if (code === 2) sp.riderRaceIntro?.(human);
      else if (code === 3 && loc >= 0) sp.medalRunIntro(loc); else if (code === 7) sp.medals?.(human, active?.place ?? -1, true);
      else if (code === 0xC && loc >= 0) sp.eventIntro(loc); else if (code === 0xF && loc >= 0) sp.venueIntro(loc);
    } catch (e) { console.warn('Cutscene cue', e); }
  }
  function music(code) { if (code !== 0xff) host.audio?.cutsceneMusic?.(code); }
  function stepAudio(seq, t0, t1) {
    const ctl = seq.control;
    if (ctl) {
      // ch0 stage-script calls (0x2808E8; the plane's LiveComp, web/cutscene-stage-sets.js), ch1 music codes (0x28E8C0),
      // ch2 PA/DJ cues (0x2A19D8), ch3 non-positional sounds (CHARACTER bus).
      for (const it of ctl.ch[0]?.i || [])
        if (it.t >= t0 && it.t < t1 && !seq.fired.has('p' + it.t + ':' + it.w[0])) {
          seq.fired.add('p' + it.t + ':' + it.w[0]);
          const owned = stage()?.call(it.w[0], it.w[1], it.t);
          if (!stagedSet() && pv('departCalls') && TRANSPORT_KINDS.has(active?.kind)) hubCall(seq, it.w[0], it.w[1]);
          else planeFx.call(stagedSet(), it.w[0], it.w[1], owned);
        }
      for (const it of ctl.ch[1]?.i || []) if (it.t >= t0 && it.t < t1 && !seq.fired.has('m' + it.t)) { seq.fired.add('m' + it.t); music(it.w[0]); }
      for (const it of ctl.ch[2]?.i || []) if (it.t >= t0 && it.t < t1 && !seq.fired.has('c' + it.t)) { seq.fired.add('c' + it.t); cue(it.w[0]); }
      for (const it of ctl.ch[3]?.i || []) {
        const on = it.t0 <= t1 && t1 < it.t1, key = 's' + it.s + ':' + it.t0;
        if (on && !seq.voices.has(key)) { const v = playSound(it.v, { volume: Math.min(127, it.tail?.[0] || 127) }); if (v !== undefined) seq.voices.set(key, v); }
        else if (!on && seq.voices.has(key)) { seq.voices.get(key)?.stop?.(0.05); seq.voices.delete(key); }
      }
    }
    for (const a of seq.actors) {
      const o = a.object;
      for (const it of o.ch[1]?.i || []) {
        const on = it.t0 <= t1 && t1 < it.t1, key = it.s + ':' + it.t0;
        if (on && !a.voices.has(key)) { const v = playSound(it.v, { volume: Math.min(127, it.tail?.[0] || 127), position: a.soundPos || null }); if (v !== undefined) a.voices.set(key, v); }
        else if (!on && a.voices.has(key)) { a.voices.get(key)?.stop?.(0.05); a.voices.delete(key); }
      }
      // ch2 speech events: 2 Finish_Line, 100 BC_Challenge, 101 Hey (0x123E30).
      for (const it of o.ch[2]?.i || []) if (it.t >= t0 && it.t < t1 && !a.fired.has(it.t)) { a.fired.add(it.t); riderSpeech(it.w[0], a.rider, it.w[1], seq); }
    }
  }

  function poseActors(seq, t) {
    const origin = host.origin;
    const race = host.onRaceActors ? [] : null;   // pv eventInWorldAi: the computer riders' actors (main.js holds their rider contexts there)
    for (const a of seq.actors) {
      const root = a.frame ? actorRootAt(a.object, a.frame, t) : null;
      if (race && root && a.rider?.slot > 0) race.push({ slot: a.rider.slot, pos: root.pos, yaw: root.yaw + Math.PI / 2 });
      if (root) { if (a.soundPos) { a.soundPos[0] = root.pos[0]; a.soundPos[1] = root.pos[1]; a.soundPos[2] = root.pos[2]; } else a.soundPos = root.pos.slice(); }   // PS2 cm (web/sfx.js)
      const m = a.model; if (!m?.ready || !root) continue;
      if (!a.object.ch[0]?.i?.length) { m.show(false); continue; }   // no clip: parked out of sight (heli_inair's rider sits inside the fuselage)
      const g = m.model.group;
      const p = toScene(root.pos, origin); a.scenePos = p;
      if (g.parent !== host.scene) host.scene.add(g);
      g.position.set(p[0], p[1], p[2]);
      g.quaternion.set(root.quat[0], root.quat[2], -root.quat[1], root.quat[3]);   // PS2 (x, y, z) -> three (x, z, -y)
      g.scale.setScalar(a.rider?.scale ?? 1);
      // clip: the item active at t (type-1 ranges, pool 2), crossfaded from the previous one over its blend ticks
      const items = a.object.ch[0]?.i || [];
      let cur = null, prev = null;
      for (const it of items) { if (it.t0 <= t && t < it.t1) cur = it; else if (it.t1 <= t && (!prev || it.t1 > prev.t1)) prev = it; }
      if (!cur && items.length) cur = items.reduce((x, y) => (y.t1 > x.t1 ? y : x));   // the last clip holds its end pose
      const clip = cur && seq.loaded.clips?.[cur.v];
      const pe = pdaEvents(clip);
      if (pe && !m.model.pda && !a.pdaLoading) { a.pdaLoading = true; attachPda(a); }
      if (m.model.pda) {
        let on = false;
        if (pe) { const f = (clipSeconds(cur, Math.min(t, cur.t1)) * clip.fps) % Math.max(1, clip.frame_count - 1); on = f >= pe.show && !(pe.hide > pe.show && f >= pe.hide); }
        m.model.pda.visible = on;
        const st = clip?.streams?.['11'], inf = m.model.pda.morphTargetInfluences;   // flip open: part-11 stream channel 0
        if (on && st && inf?.length) { const smp = clipSample(clip, clipSeconds(cur, Math.min(t, cur.t1)), true); inf[0] = channelValue(seq.loaded.samples, smp, st, 0); }
      }
      if (clip) {
        const A = clipSample(clip, clipSeconds(cur, Math.min(t, cur.t1)), true);
        let B = null, w = 0;
        if (prev && cur.b > 0 && t - cur.t0 < cur.b && seq.loaded.clips?.[prev.v]) { B = clipSample(seq.loaded.clips[prev.v], clipSeconds(prev, prev.t1), true); w = 1 - (t - cur.t0) / cur.b; }
        m.apply(T, A, B, w, seq.loaded.samples, { board: true });
      }
      g.updateMatrixWorld(true);
      // The human's rider actor (binding 4): its posed board root (board_rootg, the gameplay rig's bone 22 = rider+0x8A0) in PS2 cm, and
      // the record's root-motion flag (123640: +0xAFC = record byte 7), for 120F20's re-probe from the board root (core nis_hold_probe 2,
      // main.js pv nisBoneProbe; docs/ctm-parity.md "120F20's re-probe under the hold")
      if (a.object.ext?.binding === 4) {
        const k = m.model.rig.bones.findIndex((b) => b.name === 'board_rootg'),
          bone = k >= 0 ? m.model.bones[k] : null;
        if (bone) {
          const w = new T.Vector3();
          bone.getWorldPosition(w);
          seq.humanBoard = {
            pos: [(w.x + origin.x) * 100, -(w.z + origin.z) * 100, (w.y + origin.y) * 100],
            afc: a.object.ext.root_motion_velocity ? 1 : 0,
            t
          };
        }
      }
      if (host.core) m.light(T, host.core, seq.view || null);
      m.show(true);
    }
    if (race?.length) host.onRaceActors(race);
  }

  function cameraPose(seq, t) {
    const cut = seq.cuts ? activeCut(seq.cuts, t) : null;
    const id = cut ? cut.cam : seq.cameras[0]?.ext.camera_id;
    const cam = seq.cameras.find((c) => c.ext.camera_id === id) || seq.cameras[0];
    if (!cam) return null;
    // a camera on a live actor (anchors 40..60) is framed when it is cut to (27D850 -> 27D970, the camera's
    // activation) on its rider as it is then (liveActorAnchor), not on the actor's object frame: #154-163 / #128-137 camera 5
    // sits 1.5 m behind the rider at the door (PS2 new-career s3050..3250), not 1.5 m behind the plane's locator.
    if (seq.liveCut !== cam) { seq.liveCut = cam; const f = liveCutFrame(cam, seq.actors, t); if (f) cam.frame = f; }
    let subject = null;
    if (cam.kind === 2) {
      const a = seq.actors.find((x) => x.object.ext.binding === (cam.ext.subject ?? -1) + 1) || seq.actors[0];
      if (a?.frame) {
        const r = actorRootAt(a.object, a.frame, t);
        subject = {
          pos: r.pos,
          forward: [Math.cos(r.yaw + Math.PI / 2), Math.sin(r.yaw + Math.PI / 2), 0],
          up: [0, 0, 1],
          scale: a.rider?.scale ?? 1
        };
      }
    }
    const pose = cameraAt(cam, cam.frame, t, subject);
    seq.view = viewMatrix(pose, host.origin);
    return pose;
  }

  // ---- list player ----
  // Fades (0x277980 -> the screen fader 0x2E4370 / 0x2E44F0). A step's own fade-in record (header +0x14: out, hold, in)
  // is used when a list starts; once a step has ended with its fade-out record (+0x1C: out before the end, then in),
  // the next step fades in with THAT record's in ticks (2E44F0 rewrites the running fade's in phase) and its own
  // fade-in record is not used. PS2 (frame means, ps2b/intro + semi-intro): the CTM approach after the fly-over
  // (fly-over fade-out in = 0) is bright at t3 although its own record says in 30; the gate idle after the approach
  // fades in over 30 (approach fade-out in 30); gond_inair_zoe after gond_inair fades in over ~30, not its own 60.
  // 0x2E47E8 (the fade's render): at opacity >= 0.93 (gp-0x3A1C) every world painter is reset (0x2C03E8) on each drawn frame
  // (web/weather.inc weather_fade_reset; the rider reset fade runs in the core, per tick).
  const FADE_RESET = Math.fround(0.93), fadePainterReset = (alpha) => { if (alpha >= FADE_RESET) host.core?._weather_fade_reset?.(); };
  function fadeAlpha(state) {
    if (!state.seq) return state.black ? 1 : 0;
    const seq = state.seq,
      r = fadeAt({
        script: seq.script,
        t: seq.fadeT ?? seq.t,
        duration: seq.duration,
        prevFadeOut: state.prevFadeOut,
        step: state.steps[state.at],
        held: seq.held || 0,
        loops: seq.loops || 0
      });
    // 277980 -> 2E4370: the release's fade-out, n / 30 on list tick n
    if (state.releaseT != null) {
      const fo = seq.script.fade_out,
        a = Math.min(1, (state.releaseT + 1) / Math.max(1, fo?.out_ticks || 30));
      if (a > r.alpha) {
        r.alpha = a;
        r.colour = fo?.colour === 'white' ? '#fff' : '#000';
      }
    }
    state.fadeColour = r.colour; fadePainterReset(r.alpha); return r.alpha;
  }

  async function startStep(state) {
    const step = state.steps[state.at];
    if (!step) return finish(state);
    if (step.location) state.location = step.location;   // pv transportFade: the heli drop appended to the Transport list plays at the destination (anchors, sets, cues)
    if (step.fmv) {   // ABC1/DBC2/EBC3 movie (list 29..31): no audio track (the pktrans stream plays over it)
      state.seq = null; host.onMovie?.(true);   // pv worldUnderCuts: only a movie step stops the world (main.js pushes its HOLD)
      let played; try { played = await (host.movie || playMovie)(FMV[step.fmv] || step.fmv, { skippable: !!(step.flags & FLAG.SKIP) }); } finally { host.onMovie?.(false); }
      if (played?.skipped && (step.flags & FLAG.SKIP)) return skip(state);
      state.at++; return startStep(state);
    }
    const loaded = state.loaded[state.at];
    const seq = new Sequence(loaded, state.cast, Math.random, step.alts ?? api.qaAlts?.[loaded.number] ?? null);   // qaAlts: PS2 comparisons
    state.seq = seq; state.hold = !!(step.flags & FLAG.HOLD) || !!step.idle;
    prepareSequence(seq);
    await Promise.all(seq.actors.map(async (a) => { a.model = a.rider ? await actorModel(a.rider) : null; }));
    await loadBank(loaded);
    const set = setOf(seq); if (set) await ensureSet(set); showSets(set);
    if (state !== active) { releaseActors(seq); return; }
    seq.ready = true;   // the clock starts once the step's actors, bank and set are in (the PS2 list waits for its loads)
    poseActors(seq, 0);
    // The human's rider actor (binding 4) of this step at its first tick: 123640 runs at each step's actor start (host.onHumanActor:
    // main.js holds the rider there under a Transport's ride, pv nisTick)
    {
      const h = seq.actors.find((a) => a.object.ext?.binding === 4 && a.frame);
      if (h) {
        const r = actorRootAt(h.object, h.frame, 0);
        host.onHumanActor?.({ pos: r.pos, quat: r.quat, yaw: r.yaw + Math.PI / 2, kind: state.kind, script: loaded.number });
      }
    }
    try { state.onStep?.(step, state.at); } catch (e) { console.warn('Cutscene onStep', e); }   // pv transportFade: the Transport's held loop start requests the destination (0x2366C4)
    if (step.idle) state.onIdle?.();   // the idle loop runs on under the next screen (objectives card) until stop(); the card's context 1 holds it still (update)
  }

  function nextStep(state, skipped = false) {
    // a skip cuts without the fade (0x276F48); a step that ran to its end hands its fade-out record to the next one
    const released = state.releaseT != null; state.releaseT = null;   // pv transportFade: 27A9F0's 2766D0(list, 1, fade 1): the held step ended through its fade_out record
    state.prevFadeOut = !skipped && state.seq && (released || !(state.steps[state.at]?.flags & FLAG.HOLD && !state.steps[state.at]?.holdTicks)) ? state.seq.script.fade_out || null : null;
    planeFx.stepEnd(stagedSet()); hubStepEnd();
    stage()?.end();   // 0x2807B0: the step's recorded cleanups
    if (state.seq) releaseActors(state.seq);
    state.at++;
    if (skipped) while (state.steps[state.at] && (state.steps[state.at].flags & FLAG.CHAIN)) state.at++;
    state.seq = null;
    if (state.at >= state.steps.length) return finish(state);
    state.stepping = startStep(state);
  }
  function skip(state) { state.skipped = true; nextStep(state, true); }

  function finish(state) {
    hubStepEnd(); planeFx.end();
    startLinger();
    showSets(null); queueMicrotask(trimPool);
    if (state.seq) releaseActors(state.seq);
    state.seq = null;
    sectionEnd();
    if (active === state) { active = null; host.onActive?.(false); host.audio?.cutscene?.(false); }
    state.resolve({ played: true, skipped: !!state.skipped, scripts: state.steps.map((s) => s.script ?? s.fmv) });
  }

  // Queue and play. steps: [{group|script|fmv, flags, idle?}], cast, location. The promise resolves when the last
  // non-idle step ends (an `idle` step keeps looping under the next screen until stop()).
  async function play({ steps, cast, location, hub = null, kind = 'script', onIdle = null, place = -1, restore = true, prevFadeOut = null, barsFull = false, skipLock = 0, onStep = null }) {
    await ensureIndex();
    const resolved = resolve(steps, cast, location);
    if (!resolved.length) { overlay = null; return { played: false, skipped: false, scripts: [] }; }
    endLinger();
    const loaded = await Promise.all(resolved.map((s) => (s.fmv ? null : loadScript(index, s.script, s.location ?? location, hub))));
    stopNow();
    // the overlay (letterbox, skip, Loading) is drawn on the 'cutscene' screen; callers that did not switch to it get it here
    const ui = host.ui, back = ui?.screen;
    if (ui && back !== 'cutscene') ui.set('cutscene');
    const done = (r) => { if (ui && restore !== false && back && back !== 'cutscene' && ui.screen === 'cutscene') ui.set(back); return r; };
    return new Promise((resolveDone0) => { const resolveDone = (r) => resolveDone0(done(r));
      const state = {
        kind,
        steps: resolved,
        loaded,
        cast,
        location,
        at: 0,
        seq: null,
        skipped: false,
        resolve: resolveDone,
        onIdle,
        place,
        prevFadeOut,
        barsFull: barsFull || !!overlay,
        skipLock,
        onStep,
        releaseT: null
      };
      active = state; overlay = null; host.onActive?.(true); host.audio?.cutscene?.(true);
      if (kind === 'heat') host.audio?.heat?.(); // world state 13 -> 27A860 -> 28E8C0(20, 1): heats 2 / 3 get a new song (heatSong, web/game-audio.js)
      state.stepping = startStep(state);
    });
  }

  // Idle steps (the start-gate loop under the objectives card) resolve the play() promise when they start.
  // A list held across a course switch (acrossSwitch) ignores the page's stop() (main.js stopRun / startRun) until released.
  function stop() { if (active?.persist) return; stopNow(); }
  function stopNow() {
    endLinger(); hubStepEnd(); planeFx.end();
    across = null;
    const s = active; if (!s) return;
    showSets(null);
    if (s.seq) releaseActors(s.seq);
    sectionEnd();
    s.seq = null; active = null; host.onActive?.(false); host.audio?.cutscene?.(false);
    s.resolve({ played: true, skipped: !!s.skipped, stopped: true, scripts: s.steps.map((x) => x.script ?? x.fmv) });
  }

  // Per rendered frame: advance in 60 Hz ticks, pose actors, return the camera for main.js (or null).
  let skipLatch = false, spaceDown = false;
  if (typeof addEventListener === 'function') {
    addEventListener('keydown', (e) => { if (e.code === 'Space') { spaceDown = true; if (active) e.preventDefault(); } });
    addEventListener('keyup', (e) => { if (e.code === 'Space') spaceDown = false; });
    addEventListener('blur', () => { spaceDown = false; });
  }
  const crossDown = host.crossDown || (() => {
    if (spaceDown) return true;
    try { if (activePad()?.buttons?.[0]?.pressed) return true; } catch {}   // Cross on the active pad of any slot, mapped (web/gamepad.js)
    return false;
  });
  const qa = { frozen: false };
  // FMV step (VidEnginePlayer 0x2838E8): /assets/MOVIES/<key>.mp4 from tools/export_movies.py (needs ffmpeg); the
  // widescreen master when the display is 16:9. Skipped (resolves {missing}) when it was not exported.
  let movieIndex = null;
  async function playMovie(key, { skippable = true } = {}) {
    if (typeof document === 'undefined') return { missing: true };
    movieIndex ??= fetchJson('/assets/MOVIES/movies.json').catch(() => ({ movies: [] }));
    const list = (await movieIndex).movies || [], wide = host.ui?.widescreen && host.ui.widescreen !== 0;
    const m = (wide && list.find((v) => v.key === key + 'WS')) || list.find((v) => v.key === key);
    if (!m) { console.info(`Cutscene movie ${key} not exported (python3 tools/export_movies.py ${key})`); return { missing: true }; }
    const v = document.createElement('video');
    v.src = '/assets/' + m.src; v.playsInline = true; v.muted = true; v.preload = 'auto';
    // under the UI canvas (#ui, z 2; after #game in the stage): the cutscene overlay's bars and skip prompt draw over it
    v.style.cssText = 'position:absolute;inset:0;width:100%;height:100%;object-fit:fill;background:#000;z-index:1';
    (host.ui?.stage || document.body).appendChild(v);
    return new Promise((resolve) => {
      let done = false, latch = true;
      const end = (skipped) => { if (done) return; done = true; v.pause(); v.remove(); resolve({ skipped }); };
      v.onended = () => end(false); v.onerror = () => end(false);
      const poll = () => { if (done) return; const c = crossDown(); if (skippable && c && !latch) return end(true); latch = c; requestAnimationFrame(poll); };
      v.play().then(() => requestAnimationFrame(poll), () => end(false));
    });
  }
  let lastPose = null, mainAt = 0;
  // The NIS director's camera point in the section activation (host.sectionPoint -> core section_point): a script's director adds its
  // +0x100 at its start (0x281370 -> 0x1033B0), sets it to the outer camera's +0x20 (the rendered eye) in its update (0x281100) and
  // removes it at its end (0x281400 -> 0x1033F8), so the stage pieces around the NIS camera activate (PS2 c0a-snap: the Snow Jam
  // fly-over lists 42 more; a new step's script re-adds it, A+0xD0 = -1 at 3130). Unconfirmed: which scripts' directors register
  // (279370's test; seen for the fly-over, the approach / idle and the Transport ride), and the eye's tick alignment (the PS2 copies the
  // previous tick's camera; this gives the last update's pose).
  let sectionSeq = null;
  const sectionEnd = () => { if (sectionSeq) { sectionSeq = null; host.sectionPoint?.(null); } };
  const sectionFeed = (seq, pose) => {
    if (!pose || !host.sectionPoint) return pose;
    if (sectionSeq !== seq) {
      if (sectionSeq) host.sectionPoint(null);
      sectionSeq = seq;
    }
    host.sectionPoint(pose.eye);
    return pose;
  };
  function update(dt, self = false) {
    if (!self) mainAt = performance.now();
    const s = active; if (!s) { lastPose = null; return null; }
    if (s.skipLock > 0 && !qa.frozen) s.skipLock -= dt * TICK_HZ;   // 278DE8(nis, 30) at 0x236550: nis+0x554 counts down every NIS update, a list or not
    if (!s.seq || !s.seq.ready) return lastPose;   // between steps (the next one is loading): hold the last camera
    if (qa.frozen) dt = 0;
    const seq = s.seq, step = s.steps[s.at];
    // The NIS tick runs only while no pause context stops it (docs/pause-contexts.md; docs/ctm-decomp-world-states.md 1.3): mask bit 0x08
    // skips the NIS update (0x230BDC: the script clock, camera and actors hold). The view update 22E840 still draws the held pose, and
    // the fade (a view effect, 277980 -> 2E4370) runs on its own clock, which no context stops. The round card (WS2 enter 0x236BB0,
    // context 1, mask 0xFFFFFFDB) is pushed one tick into the idle (PS2 t = 1.0): the update that first sees a stopped NIS runs it to its
    // next whole tick (the idle's first: t = 1.0 exactly, whatever the frame's dt), then it holds; nothing thaws it before the card's
    // Cross stops the list (WS2 exit 0x236C88). ARMSX2 card-idle: 330 samples, identical pixels.
    const nisStopped = !!host.nisStopped?.();
    if (!nisStopped) s.heldSeq = null;
    else if (s.heldSeq === seq) { seq.fadeT = (seq.fadeT ?? seq.t) + dt * TICK_HZ; return lastPose; }
    const stopTick = nisStopped; if (stopTick) s.heldSeq = seq;
    const adv = stopTick ? Math.floor(seq.t) + 1 - seq.t : dt * TICK_HZ;   // this update's NIS ticks
    // NISSkip (Cross) on skippable steps: a hard cut to the next step without flag 2 (0x276F48).
    const cross = crossDown();
    // the skip lock: 0x278828 skips the Cross test
    if (!stopTick && cross && !skipLatch && (step.flags & FLAG.SKIP) && seq.t > 0 && !(s.skipLock > 0)) { skipLatch = true; skip(s); return update(0); }
    skipLatch = cross;
    const t0 = seq.t; let t1 = t0 + adv;
    if (s.releaseT != null) {   // pv transportFade: the released held step plays on under its fade-out; the list stops (or moves on) when +0xB8 reaches 0 (276CC8 @0x276D74, R+29)
      s.releaseT += adv;
      if (s.releaseT >= Math.max(1, (seq.script.fade_out?.out_ticks || 30) - 1)) { nextStep(s); return s === active ? update(0) : null; }
    }
    if (step.holdTicks) {   // a held step with no loader to wait for: released after the PS2's measured load time
      seq.held = (seq.held || 0) + adv;
      if (seq.held >= step.holdTicks) { nextStep(s); return s === active ? update(0) : null; }
    }
    if (t1 >= seq.duration) {
      // idle steps and flag-8 steps loop until stop() / advance() (the transport ride holds until the destination is
      // loaded, 0x279070); a looping script without flag 8 (heli_inair / gond_inair) plays one cycle, then the list moves on
      if (step.idle || (step.flags & FLAG.HOLD)) { t1 %= seq.duration; seq.loops = (seq.loops || 0) + 1; }
      else { stepAudio(seq, t0, seq.duration); nextStep(s); return s === active ? update(0) : null; }
    }
    seq.t = t1;
    if (seq.fadeT != null) seq.fadeT += t1 >= t0 ? t1 - t0 : t1 + seq.duration - t0;   // the fade's own clock, once a stopped NIS left it ahead
    if (planeFx.active) { planeFx.advance(adv); planeFx.frame(stagedSet()); }
    stepAudio(seq, t0, t1 < t0 ? t1 + seq.duration : t1);
    const st = stage(); if (st) { st.sync(seq.t); st.apply(); }
    if (t1 < t0) { seq.fired.clear(); for (const a of seq.actors) a.fired.clear(); }
    poseActors(seq, seq.t);
    return (lastPose = sectionFeed(seq, cameraPose(seq, seq.t)));
  }

  // Apply a camera pose to a three.js camera (FOV: half-horizontal angle of 4:3, main.js 376C58 convention).
  function applyCamera(camera, pose) {
    const o = host.origin, e = toScene(pose.eye, o), g = toScene(pose.target, o);
    camera.position.set(e[0], e[1], e[2]); camera.up.set(0, 1, 0); camera.lookAt(g[0], g[1], g[2]);
    if (pose.roll) camera.rotateZ(-pose.roll);
    camera.near = pose.near / 100; camera.far = pose.far / 100;
    if (api.qaFovAspect == null) {
      // The horizontal tangent through a view offset: camera.aspect stays the stage's (main.js clears the offset every frame).
      const [tx, ty] = nisTangents(Math.tan(pose.fov), host.ui?.widescreen ?? 0, letterbox()), A = camera.aspect, k = tx / (A * ty);
      camera.fov = 2 * Math.atan(ty) / DEG;
      if (Math.abs(k - 1) > 1e-9) camera.setViewOffset(A, 1, A * (1 - k) / 2, 0, A * k, 1);
    } else camera.fov = 2 * Math.atan(Math.tan(pose.fov) * (api.qaFovAspect ?? 1)) / DEG;
    camera.updateProjectionMatrix();
  }
  // The letterbox of the current step, 0..1: the bars' slide (draw below), which the PS2's letterbox 2EA900 also applies to the
  // projection block.
  function letterbox() {
    const s = active; if (!s) return 0;
    const step = s.steps[s.at], seq = s.seq;
    if (!((seq && !step?.idle) || step?.fmv)) return 0;
    return s.barsFull || s.at > 0 || !seq ? 1 : Math.min(1, seq.t / 30);
  }

  // Overlay (ui.draw when the screen is 'cutscene'), PS2 480-line frame units (ui.js HUD convention): the cinematic
  // bars (60 lines each, sliding in over the first 30 ticks of a non-idle step; PS2 zoe-sj 942..970), the blinking
  // "Press X to skip" (kT_OVRCMNPress + X + kT_OVRCMNToSkip, 0x1E9A30/0x1EAB0C) while the step is skippable, the fades.
  // Pre-fade (a script header's fade_in {out, hold} ticks, e.g. the venue fly-overs and podiums: out 30): before the
  // script clock starts, the current game view fades to the colour while the world runs on (PS2 ps2b/intro s735..766:
  // world state 1, HUD gone, the bars slide in, "Loading..." shows, the rider keeps riding). Drawn over the 'game'
  // screen (ui.js draws api.overlay there instead of the HUD); timed in 60 Hz ticks of real time.
  let overlay = null;
  function preFade({ ticks = 30, hold = 0, colour = 'black', loading = false } = {}) {
    return new Promise((resolve) => {
      const o = overlay = { t: 0, ticks, hold, colour: colour === 'white' ? '#fff' : '#000', loading, done: resolve };
      let last = performance.now();
      const step = (now) => {
        if (overlay !== o) return resolve(false);
        if (!qa.frozen) o.t += Math.min(Math.max(0, now - last), 250) * TICK_HZ / 1000; last = now;
        if (o.t >= o.ticks + o.hold) { resolve(true); return; }   // the overlay stays up (black) until play() replaces it
        requestAnimationFrame(step);
      };
      requestAnimationFrame(step);
    });
  }
  // A fade from white over the running world (WS15 same-course return from the post-event map: 2E4CE8 white fade, PS2
  // ctm/caps sj-return: near-white at the course start, clear after ~58 ticks).
  // hud: the fade covers the world only and the HUD draws over it (arrivalFade: a transport arrival fades in from black over
  // 30 ticks under the HUD, PS2 menus/fr-courses/aba1-screen10 arrival: black at the placement, clear ~30 ticks later).
  // bars (pv transportFade): the list stop's letterbox slide-out 2EA780 over the fade's remaining time, 60 lines -> 0 (PS2 to-c-fade 1470..1502).
  function fadeFrom({ ticks = 58, colour = 'white', hud = false, bars = false } = {}) {
    const o = overlay = { t: 0, ticks, hold: 0, colour: colour === 'white' ? '#fff' : '#000', loading: false, reverse: true, hud, bars };
    let last = performance.now();
    const step = (now) => {
      if (overlay !== o) return;
      if (!qa.frozen) o.t += (Math.min(Math.max(0, now - last), 250) * TICK_HZ) / 1000;
      last = now;
      if (o.t >= o.ticks) {
        overlay = null;
        return;
      }
      requestAnimationFrame(step);
    };
    requestAnimationFrame(step);
  }
  function drawOverlay(c, ui) {
    const o = overlay; if (!o) return;
    fadePainterReset(o.reverse ? 1 - o.t / o.ticks : Math.min(1, o.t / Math.max(1, o.ticks)));
    if (o.reverse) { c.save(); c.globalAlpha = Math.max(0, 1 - o.t / o.ticks); c.fillStyle = o.colour; c.fillRect(0, 0, 640, 448); c.globalAlpha = 1;
      if (o.bars) { const bar = 60 * Math.max(0, 1 - o.t / o.ticks) * 448 / 480; c.fillStyle = '#000'; c.fillRect(0, 0, 640, bar); c.fillRect(0, 448 - bar, 640, bar); }
      c.restore(); return; }
    c.save(); c.scale(1, 448 / 480);
    const bar = 60 * Math.min(1, o.t / 30);
    c.fillStyle = '#000'; c.fillRect(0, 0, 640, bar); c.fillRect(0, 480 - bar, 640, bar);
    const a = Math.min(1, o.t / Math.max(1, o.ticks));
    if (a > 0) { c.globalAlpha = a; c.fillStyle = o.colour; c.fillRect(0, 0, 640, 480); c.globalAlpha = 1; }
    if (o.loading) loadingCaption(c, ui, 396);   // over the fade: the world's caption stays bright (PS2 s745..765)
    c.restore();
  }
  // The world's "Loading..." caption (0x1F30D8): yellow text with a dark shadow and a white spinning snowflake.
  function loadingCaption(c, ui, y) {
    // the snowflake turns about its vertical axis (PS2 ps2b/intro frames: it narrows to a vertical line and widens again)
    const x = 518, sx = Math.cos(performance.now() / 1000 * Math.PI);
    c.save(); c.strokeStyle = '#fff'; c.lineWidth = 2; c.translate(x - 16, y - 6); c.scale(Math.max(Math.abs(sx), 0.08), 1);
    for (let k = 0; k < 3; k++) { const r = Math.PI / 2 + k * Math.PI / 3; c.beginPath(); c.moveTo(-8 * Math.cos(r), -8 * Math.sin(r)); c.lineTo(8 * Math.cos(r), 8 * Math.sin(r)); c.stroke(); }
    c.restore();
    ui.text(c, 'Loading...', x + 2, y + 2 - 12, 18, '#1a1a10', 'FEFONT');
    ui.text(c, 'Loading...', x, y - 12, 18, '#d8d820', 'FEFONT');
  }
  // A black hold with the caption (the in-world event load after the fly-over: web/ctm-event.js), drawn by the loading
  // screen's world mode (web/loading-screen.js) while the course switches in the page.
  function drawCover(c, ui) {
    c.save(); c.scale(1, 448 / 480); c.fillStyle = '#000'; c.fillRect(0, 0, 640, 480); loadingCaption(c, ui, 396); c.restore();
  }

  function draw(c, ui) {
    const s = active; if (!s) { drawOverlay(c, ui); return; }
    const step = s.steps[s.at], seq = s.seq;
    c.save(); c.scale(1, 448 / 480);
    if ((seq && !step?.idle) || step?.fmv) {
      // the bars slide in over the first 30 ticks of a list (over the pre-fade when there is one: PS2 metro-intro s174..206
      // 2 lines per tick, full at the fly-over's t0) and stay in for its later steps
      // a movie step (list ids 29..31) is letterboxed like the NIS steps around it (PS2 ctm/caps new-career s650..2325: the
      // ABC1 movie between 60-line bars with the blinking "Press X to skip")
      const bar = s.barsFull || s.at > 0 || !seq ? 60 : 60 * Math.min(1, seq.t / 30);
      c.fillStyle = '#000'; c.fillRect(0, 0, 640, bar); c.fillRect(0, 480 - bar, 640, bar);
    }
    if (step && (step.flags & FLAG.SKIP) && (seq || step.fmv) && !(s.skipLock > 0)) {   // 2792E0 returns 0 under the skip lock: no "Press X to skip"
      const loc = ui.careerUI?.loc, press = loc?.text?.('kT_OVRCMNPress', 'Press') || 'Press', to = loc?.text?.('kT_OVRCMNToSkip', 'to skip') || 'to skip';
      c.globalAlpha = 0.45 + 0.4 * (0.5 + 0.5 * Math.cos(performance.now() / 1000 * Math.PI));
      ui.text(c, to, 622, 384, 18, '#3c3c46', 'FEFONT', 'right');
      const toW = measure(ui, to, 18), iconX = 622 - toW - 32;
      if (inputDevice() === 'keyboard') {             // keyboard: the Space key cap in the icon's place (web/input-glyphs.js; skip = crossDown)
        const label = keyFor('cross', { context: 'race' }), h = 20 * CAP_HEIGHT, w = keyCapWidth(ui.fonts?.FEFONT, label, h, LUI_STRETCH), x = iconX + 21 - w;
        drawKeyCap(c, ui, label, x, 392 - h / 2, w, h, LUI_STRETCH);
        ui.text(c, press, x - 5, 384, 18, '#3c3c46', 'FEFONT', 'right');
      } else {
        if (ui.images?.['OV_1-2']) c.drawImage(ui.images['OV_1-2'], 55, 122, 24, 24, iconX, 381, 24, 24);
        ui.text(c, press, iconX - 6, 384, 18, '#3c3c46', 'FEFONT', 'right');
      }
      c.globalAlpha = 1;
    }
    // The world's "Loading..." caption (0x1F30D8, PS2 ctm-transport frames): yellow text with a dark shadow and a white
    // spinning snowflake, while a held step waits for the destination (the in-air transport loops).
    // ... and while the world loads under a step (step.loading: the CTM venue fly-over, 0x27AAF8 list 2 flags 0; PS2
    // ps2b/intro s745..1040 shows it over the whole fly-over, drawn over the fades).
    const a = fadeAlpha(s);
    if (a > 0) { c.globalAlpha = Math.min(1, a); c.fillStyle = s.fadeColour || '#000'; c.fillRect(0, 0, 640, 480); c.globalAlpha = 1; }
    // Over the script's fades (PS2 s770: bright over the fly-over's fade-in); it goes when the riders are loaded (PS2
    // t~258-262, 12A180), which in the browser happens after the fly-over, behind the course-switch cover.
    // released: WS10 enter clears the caption bit (R+1)
    if (seq && s.releaseT == null && ((step?.flags & FLAG.HOLD) || seq.loop || step?.loading)) loadingCaption(c, ui, seq && !step?.idle ? 396 : 440);
    c.restore();
  }
  function measure(ui, text, size) { const g = ui.fonts?.FEFONT; if (!g) return text.length * size * 0.5; let w = 0; for (const ch of text) w += (g[ch]?.advance || 10) * size / 22; return w; }

  // ---- a held step across the page's course switch (heli; docs/presentation.md "Heli ride over a peak change") ----
  // PS2 (ctm-parity f95-after / peak2-arr): Yes to "Go to this peak now?" -> WS14 heli_inair #149 -> WS11 heli_inair_<char>
  // #122 held (flags 8) with "Loading..." while the destination streams in -> WS10 at the new peak. The browser's course
  // switch draws no world (main.js idleFrame) and stops the cutscene (stopRun): acrossSwitch() keeps the list alive and draws
  // it itself (host.render: the TRANSP set and the actors, which stay in the scene; `background` behind them) whenever
  // the main loop has not updated it for 100 ms, until release() lets the held step go.
  let across = null;
  // the PS2 draws the world's sky dome (22DE98, camera-anchored, depth off, before the world; 382AF0 clears to
  // black) under the held loop too: the transport rows stream the destination while the departure's sky row stays until a
  // skybox trigger switches it. The page's switch unloads its sky, so the list keeps its own copy of the departure dome
  // (the course package's SKY world.json, 9 batches) and draws it first: the gondola's 58 %-opaque windows (TRANSP 9-10)
  // show it as on the PS2 (ctm-parity to-final s320) instead of the last fog colour.
  const SKY_ROWS = { 44: '/assets/SKY/', 45: '/assets/BRA2/sky/', 46: '/assets/CRA3/sky/', 47: '/assets/DRA4/sky/', 48: '/assets/ERA5/sky/' };   // ASKY..ESKY rows (web/main.js skyRoots)
  const skies = new Map();
  // The sky dome being drawn now: a streamed world's current sky row (22DE98, the core's peak_world_sky), else the course's.
  function skyRootNow(fallback = null) {
    const c = host.core, streamed = /^(PEAK\d|MOUNTAIN)$/.test(host.location?.() ?? '');
    return (streamed && c?._peak_world_sky ? SKY_ROWS[c._peak_world_sky()] : null) ?? fallback;
  }
  function ensureSky(root) {
    if (!root) return Promise.resolve(null);
    if (skies.has(root)) return skies.get(root);
    const job = (async () => {
      const d = await fetchJson(root + 'world.json');
      const [vb, ib, cb] = await Promise.all(['vertices.bin', 'indices.bin', 'colors.bin'].map((f) => fetchBuffer(root + f)));
      const inter = new T.InterleavedBuffer(new Float32Array(vb), 10), colors = new T.BufferAttribute(new Float32Array(cb), 4), indices = new Uint32Array(ib);
      const loader = new T.TextureLoader(), group = new T.Group(); group.name = `cutscene sky ${root}`; group.visible = false;
      for (const b of d.batches) {
        const t = d.textures[`9-${b.texture}`]; if (!t) continue;
        const map = await packageTexture(loader, root, t); map.flipY = false; map.wrapS = map.wrapT = T.RepeatWrapping; map.colorSpace = T.NoColorSpace;
        const g = new T.BufferGeometry();
        g.setAttribute('position', new T.InterleavedBufferAttribute(inter, 3, 0)); g.setAttribute('uv', new T.InterleavedBufferAttribute(inter, 2, 6)); g.setAttribute('color', colors);
        g.setIndex(new T.BufferAttribute(indices.slice(b.first_index, b.first_index + b.index_count), 1));
        // in the opaque list (drawn before the set; depth off): the alpha batches blend without being sorted after the cabin
        const m = new T.MeshBasicNodeMaterial({ side: T.DoubleSide, transparent: false, fog: false, depthTest: false, depthWrite: false });
        if (b.has_alpha) { m.blending = T.CustomBlending; m.blendSrc = T.SrcAlphaFactor; m.blendDst = T.OneMinusSrcAlphaFactor; m.blendEquation = T.AddEquation; }
        const tex = tslTexture(map), col = attribute('color', 'vec4');
        m.colorNode = vec4(modulateBytes(tex, col), b.has_alpha ? tex.a.mul(col.a) : 1);   // texture x colour: bytes or linear light
        const mesh = new T.Mesh(g, m); mesh.frustumCulled = false; mesh.renderOrder = b.has_alpha ? -10 : -11; group.add(mesh);
      }
      host.scene.add(group);
      return group;
    })().catch((e) => { console.warn(`Cutscene sky ${root} unavailable`, e); return null; });
    skies.set(root, job); return job;
  }
  // The departure's sky, loaded while the ride's first steps play (web/ctm-transport.js).
  function prepareAcrossSky(fallback) { const root = skyRootNow(fallback); if (root) ensureSky(root); return root; }
  // the list's own draw over the switch (renderer.render straight to the canvas, not the page's fog pipeline, whose
  // last pass writes alpha 1) left the canvas alpha below 1 where the sky dome's blended batches (alpha = As^2 + Ad(1 - As)) or,
  // without heliSky, the gondola's 58 %-opaque windows over the transparent clear were drawn. three's output pass treats the frame as
  // premultiplied (rgb / a -> sRGB -> x a) and the page composites the canvas premultiplied over black: those pixels showed
  // sRGB(rgb / a) x a, darker by a^0.55 (Chrome and WebKit alike on screen; a 2D-canvas capture unpremultiplies it again, which made
  // Chrome's QA frames look brighter than WebKit's snapshots). The PS2 shows the frame's RGB (the GS alpha is not displayed), as the
  // page path does: a last full-screen draw sets alpha to 1 and keeps the colour (blend Zero / One on RGB, One / Zero on alpha).
  let alphaFill = null;
  const opaqueAlpha = () => (alphaFill ??= opaqueAlphaFill(T));
  function acrossSwitch({ background = null, sky = null } = {}) {
    const s = active; if (!s || !host.render) return false;
    s.persist = true; if (across) return true;
    const me = across = { background, sky: null }; let prev = performance.now();
    if (sky) { me.background ??= new T.Color(0, 0, 0); ensureSky(sky).then((g) => { if (across === me) me.sky = g; }); }
    const tick = (now) => {
      if (across !== me || active !== s) return;
      if (now - mainAt > 100) {
        const pose = update(Math.min(Math.max(0, now - prev) / 1000, 0.25), true);
        for (const p of sets.values()) p.then((g) => { if (g?.visible) g.position.set(-host.origin.x, -host.origin.y, -host.origin.z); });
        if (pose) { host.camera.clearViewOffset?.(); applyCamera(host.camera, pose); }
        const scene = host.scene, bg = scene.background; if (me.background) scene.background = me.background;
        if (me.sky) { me.sky.visible = true; me.sky.position.copy(host.camera.position); }
        const fill = opaqueAlpha(); scene.add(fill);
        try { host.render(); } catch (e) { console.warn('Cutscene draw across the course switch', e); } finally { scene.background = bg; if (me.sky) me.sky.visible = false; scene.remove(fill); }
      }
      prev = now; requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
    return true;
  }
  // The destination is in: the held step ends (0x279070) and the list moves on / ends.
  function release() {
    across = null; const s = active; if (!s) return false;
    s.persist = false;
    if (s.seq && (s.steps[s.at]?.flags & FLAG.HOLD)) { nextStep(s); return true; }
    return false;
  }

  const api = {
    acrossSwitch,
    release,
    planeFx,
    prepareAcrossSky,
    // the human actor's posed board root this frame (PS2 cm), its +0xAFC (root_motion_velocity) and the NIS time: null when no binding-4 actor is posed
    humanBoard() {
      return active?.seq?.humanBoard ?? null;
    },
    // QA: the active step's actors (binding, model ready / visible, clip items)
    actorsInfo() {
      return (active?.seq?.actors || []).map((a) => ({
        binding: a.object.ext?.binding ?? null,
        rider: !!a.rider,
        model: !!a.model,
        ready: !!a.model?.ready,
        items: a.object.ch?.[0]?.i?.length ?? 0,
        rmv: a.object.ext?.root_motion_velocity ?? null
      }));
    },
    // main.js, per frame while no cutscene plays (dt: game time, 0 while paused): the heli hovering on after its arrival
    linger(dt) {
      if (linger) lingerFrame(dt);
    },
    endLinger,
    get lingering() {
      return !!linger;
    },
    get held() {
      return !!active?.persist;
    },
    get active() {
      return !!active;
    },
    get state() {
      return active
        ? {
            kind: active.kind,
            step: active.at,
            script: active.steps[active.at]?.script,
            t: active.seq?.t ?? 0,
            duration: active.seq?.duration ?? 0,
            skippable: !!(active.steps[active.at]?.flags & FLAG.SKIP)
          }
        : null;
    },
    ensureIndex,
    play,
    stop,
    update,
    applyCamera,
    draw,
    introSteps,
    preFade,
    drawCover,
    fadeFrom,
    // The idle step's fade under another screen (the round card opens while the start-gate idle fades in: PS2
    // metro-intro s722..746, the approach's fade-out in 30).
    drawFade(c) {
      const s = active;
      if (!s?.seq) return;
      const a = fadeAlpha(s);
      if (a > 0) {
        c.save();
        c.globalAlpha = Math.min(1, a);
        c.fillStyle = s.fadeColour || '#000';
        c.fillRect(0, 0, 640, 448);
        c.restore();
      }
    },
    get overlay() {
      return !!overlay;
    },
    get overlayUnderHud() {
      return !!overlay?.hud;
    },
    clearOverlay() {
      overlay = null;
    },
    // pv transportFade: the release as 0x236AA8 -> 27A9F0 -> 2766D0(list, 1, 1) -> 277980 does it: the held step plays on for its
    // fade_out record's out ticks (#122: black, 30) and the list moves on at full black. Refused while a step transition fade runs
    // (279298: list +0xB0, the loop's own fade-in); the caller retries.
    releaseFade() {
      const s = active;
      if (s?.releaseT != null) return true;
      if (!(s?.seq?.ready && s.steps[s.at].flags & FLAG.HOLD)) return false;
      if (fadeAlpha(s) > 0) return false;
      s.releaseT = 0;
      return true;
    },
    get releasing() {
      return active?.releaseT != null;
    },
    // The HUD inside the letterboxed picture area while the bars slide out (the render block 2EAA28 / 2EA900: top 0.125, height 0.75):
    // the bar fraction (ui.js squeezes the HUD by it).
    hudSqueeze() {
      const o = overlay;
      return o?.bars && o.hud ? Math.max(0, 1 - o.t / o.ticks) : 0;
    },
    // The host's "loaded" signal for a flag-8 step (transport in-air loop): the list moves on (0x279070).
    advance() {
      const s = active;
      if (s?.seq && s.steps[s.at].flags & FLAG.HOLD) {
        nextStep(s);
        return true;
      }
      return false;
    },
    // Event intro (world state 10 -> 1 -> 2): onIdle when the start-gate idle loop begins (the objectives card
    // then opens over it); resolves {played:false} when the location has no intro lists (free ride, backcountry).
    // onStep(step, at): each list step's start (main.js pv eventRiderWarm: the warm begins once the approach draws, its cast built)
    playEventIntro({ mode = 'single', cast, location, onIdle = null, onStep = null }) {
      // the fly-over already played in free ride; the approach continues its list: its fade-in comes from the fly-over's
      // fade-out record (every sga fly-over: in 0), so it starts bright (PS2 s1045)
      if (mode === 'career' && host.ui?.careerUI?.active?.rideIn)
        return play({
          steps: introSteps('career-ridein'),
          cast,
          location,
          kind: 'intro',
          onIdle,
          prevFadeOut: { type: 1, colour: 'black', in_ticks: 0 },
          barsFull: true,
          onStep
        });
      return play({ steps: introSteps(mode), cast, location, kind: 'intro', onIdle, onStep });
    },
    // The scripts and animation banks of a list (fetched once per page, like prepare's): riderPrefetch loads the event intro's
    // under the warm-up, before prepare() (its actors) runs.
    async prepareData({ steps, cast, location, hub = null }) {
      await ensureIndex();
      const words = castWords(cast),
        nums = new Set();
      for (const st of steps) {
        if (st.script != null) nums.add(st.script);
        else
          for (const e of filterList(location, st.group) || []) if (e.masks.every((m, k) => !m || m & (words[k] || 0))) nums.add(e.script);
      }
      await Promise.all([...nums].map((n) => loadScript(index, n, location, hub).catch(() => null)));
    },
    // Prefetch the scripts/banks/actor packages of a list (under the loading screen).
    async prepare({ steps, cast, location, hub = null }) {
      await this.prepareData({ steps, cast, location, hub });
      const riders = [...(cast.race || []), ...(cast.humans || []), ...(cast.roles || [])].filter(Boolean);
      await Promise.all(
        [...new Set(riders.map((r) => previewRoot(r.entry)).filter(Boolean))].map(async (root) => {
          if (pool.some((p) => p.root === root)) return;
          const fp = new FrontEndPreview();
          fp.compile = host.compile || null;
          pool.push(fp);
          try {
            const m = await fp.fetch(T, root);
            fp.root = root;
            fp.swap(m);
          } catch {
            fp.root = null;
          }
        })
      );
    },
    listen(f) {
      listeners.add(f);
      return () => listeners.delete(f);
    },
    // QA (PS2 frame comparisons): freeze the clock and seek the current step to tick t.
    freeze(on = true) {
      qa.frozen = !!on;
    },
    seekOverlay(t) {
      if (!overlay) return false;
      overlay.t = t;
      return true;
    },
    seek(t) {
      const seq = active?.seq;
      if (!seq) return false;
      seq.t = Math.max(0, Math.min(t, seq.duration - 1e-3));
      return true;
    },
    get seq() {
      return active?.seq || null;
    },
    get ui() {
      return host.ui;
    },
    freeRideCourse() {
      return host.freeRideCourse?.() ?? -1;
    },
    location() {
      return host.location?.() ?? null;
    },
    // An anchor frame now (0x27A0D8), for the rider hold under a station cut (main.js, pv nisTick: 123640 places the rider at
    // the actor's anchor, 19 = NIS_Lodge / 28 = NIS_Transport, ground-snapped); null until locators.json is in.
    anchorOf(id, location) {
      return locators ? anchorFor(id, { locations: locators.locations, location, snap: host.snap }) : null;
    },
    // The human's rider actor (kind 5, binding 4) of a script at its first tick, as 123640 places the rider: +0x110 = M(+0x700) . key 0
    // (the anchor frame x the actor's offset, 27F9F8; key 0 of its x / y / z channels, 27C9B0) and the root rotation of 0x1241C0.
    // Every station copy of lodge_arr3 (#148) and hub_trans_arr (#166) has key 0 = (0, 0, -15): the PS2 rider stands 15 cm under the
    // snapped NIS_Lodge / NIS_Transport locator (ctm-parity door-no pre.p2s, fr-booth2 record 2644). prefetchActor(number, location)
    // fetches the script copy (a few KB); actorStart returns null until it and locators.json are in.
    prefetchActor(number, location) {
      const key = `${number}@${location}`;
      if (actorScripts.has(key)) return;
      actorScripts.set(key, null);
      ensureIndex()
        .then(() => {
          const entry = index.scripts[number];
          if (!entry) return null;
          return fetchJson(`${ROOT}scripts/${scriptContainer(entry, location)}.${String(number).padStart(3, '0')}.json`);
        })
        .then((s) => {
          if (s) actorScripts.set(key, s);
        })
        .catch(() => actorScripts.delete(key));
    },
    // pv nisPreload: a station's door / booth cut loaded before its hold (main.js on entering the station course): the scripts with
    // their container's clips (loadScript), the human's cast model (binding 4 = humans[0]) and the container's bank on the NIS slot, so
    // play() starts the step with no load between the hold and the NIS's first tick (PS2 fr-booth2: the NIS on the hold's tick, record
    // 2644). Not while a list plays: the slot and the pool are its.
    async preloadStation(numbers, location) {
      if (active) return false;
      await ensureIndex();
      const loaded = await Promise.all(numbers.map((n) => loadScript(index, n, location, null)));
      await preloadActor(api.defaultCast?.({})?.humans?.[0] ?? null);
      if (active) return false;
      for (const l of loaded) await loadBank(l);
      return true;
    },
    actorStart(number, location) {
      const s = actorScripts.get(`${number}@${location}`);
      if (!s || !locators) return null;
      const obj = s.tracks.map((t) => t?.[0]).find((o) => o?.kind === 5 && o.ext?.binding === 4);
      if (!obj) return null;
      const a = anchorFor(obj.ext.anchor, { locations: locators.locations, location, snap: host.snap });
      if (!a) return null;
      const root = actorRootAt(obj, objectFrame(a, obj.ext.offset), 0);
      return { pos: root.pos, quat: root.quat, yaw: root.yaw + Math.PI / 2 }; // yaw: the forward's heading (actorRootAt's is minus 90 degrees)
    }
  };
  const actorScripts = new Map();
  singleton = api;
  return api;
}

// ---------------------------------------------------------------------------------------------------------------
// Trigger lists (docs/cutscenes.md "Triggers"): what the original queues for each situation.
let singleton = null;
export function cutscenes() { return singleton; }

// Event intro (WS10 enter -> 0x27AAF8, table 0x445488 by the game mode 0x535C11): Conquer the Mountain = venue
// fly-over (not skippable) + approach (skippable) + gate idle; Single Event / online = start-hut variant + idle.
export function introSteps(mode) {
  // The fly-over plays while the event loads in the world: "Loading..." over it (PS2 ps2b/intro s770..1040). A
  // ride-in from free ride plays it in the streamed world before the page's course switch (web/ctm-event.js), so the
  // event's own intro starts at the approach ('career-ridein').
  if (mode === 'career') return [{ group: GROUP.FLYOVER, flags: 0, loading: true }, { group: GROUP.APPROACH, flags: 3 }, { group: GROUP.GATE_IDLE, flags: 0, idle: true }];
  if (mode === 'career-ridein') return [{ group: GROUP.APPROACH, flags: 3 }, { group: GROUP.GATE_IDLE, flags: 0, idle: true }];
  if (mode === 'flyover') return [{ group: GROUP.FLYOVER, flags: 0, loading: true }];
  return [{ group: GROUP.GATE_VAR, flags: 3 }, { group: GROUP.GATE_IDLE, flags: 0, idle: true }];
}
// Podium (0x27AC60, table 0x481E90 by the best human place; Conquer the Mountain, one human, top 3).
export function podiumSteps(place, winnerCheat = false) {
  const g = place === 0 ? (winnerCheat ? GROUP.PODIUM_CHEAT : GROUP.PODIUM_WIN) : place === 1 ? GROUP.PODIUM_2ND : place === 2 ? GROUP.PODIUM_3RD : GROUP.PODIUM;
  return [{ group: g, flags: 1 }];
}
// Rival challenge issued (queued after the podium): location shot + the rival's scene.
export const rivalSteps = () => [{ group: GROUP.RIVAL_LOCATION, flags: 1 }, { group: GROUP.RIVAL_RIDER, flags: 3 }];
// Transport ride (0x27A860): heli when either end is a backcountry, else gondola; departure only at stations.
export function transportSteps({ heli = false, departure = false, multiplayer = false } = {}) {
  const s = [];
  if (departure) s.push({ group: heli ? GROUP.HELI_DEP : GROUP.GOND_DEP, flags: 1 });
  s.push({ group: heli ? GROUP.HELI_INAIR : GROUP.GOND_INAIR, flags: 0 });
  s.push({ group: multiplayer ? (heli ? GROUP.HELI_INAIR_MP : GROUP.GOND_INAIR_MP) : (heli ? GROUP.HELI_INAIR_RIDER : GROUP.GOND_INAIR_RIDER), flags: FLAG.HOLD });
  return s;
}
// Backcountry arrival (WS10 update 0x235080): first visit = FMV + midway/heli drop, then the rider's landing.
export function arrivalSteps(location, firstVisit) {
  if (location === 'ABC1' && firstVisit) return [{ fmv: 29, flags: 3 }, { group: GROUP.MIDWAY, flags: 3 }, { group: GROUP.MIDWAY_RIDER, flags: 3 }];
  const s = [];
  if (firstVisit && location === 'DBC2') s.push({ fmv: 30, flags: 3 });
  if (firstVisit && location === 'EBC3') s.push({ fmv: 31, flags: 3 });
  s.push({ group: GROUP.BC_ARRIVE, flags: 3 }, { group: GROUP.BC_ARRIVE_RIDER, flags: 3 });
  return s;
}
// Station arrivals (0x236208): the lodge walk-in / hub or end-of-event transport arrival.
export const lodgeSteps = () => [{ group: GROUP.LODGE, flags: 3 }, { group: GROUP.LODGE_2, flags: 1 }];
export const transportArriveSteps = () => [{ group: GROUP.TRANSPORT_ARRIVE, flags: 3 }];
// Next heat of a Conquer the Mountain race (world state 13, 0x235AA0): the gondola ride back up (gond_inair, then the
// rider's gond_inair_<char> held while the event restarts; PS2 semi-intro/final-intro: released at t 340), then the
// start-gate idle loop under the heat's objectives card.
// The final heat queues the start hut first (WS13 enter 0x235C58: 27AAF8(nis, GMM+0x98 ? 3 : 2) -> [4 var1/var2 (flags 3), 5]).
export const heatSteps = (final = false) => [{ group: GROUP.GOND_INAIR, flags: 0 }, { group: GROUP.GOND_INAIR_RIDER, flags: FLAG.HOLD, holdTicks: 340 },
  ...(final ? [{ group: GROUP.GATE_VAR, flags: 3 }] : []), { group: GROUP.GATE_IDLE, flags: 0, idle: true }];
// Restart from the results or the pause menu (0x20D7DC -> 2302A8 -> world state 1 arg 2): the start-gate idle loop under
// the round's card, without the fly-over / approach.
export const restartSteps = () => [{ group: GROUP.GATE_IDLE, flags: 0, idle: true }];

// The public entry point (Peak 1 / transport / station code calls this). kind: 'intro' | 'podium' | 'rival' |
// 'transport' | 'arrival' | 'lodge' | 'transport-arrive' | 'script'. Resolves {played:false} when the cutscene
// system is not up (tests, QA pages) so callers can always await it.
export async function playCutscene(opts = {}) {
  const cs = singleton; if (!cs) return { played: false, skipped: false, scripts: [] };
  const { kind = 'script', id = null, location = null, mode = 'single', place = -1 } = opts;
  // cast: explicit, or the host's (human + lineup) with opts.roles = [rider id | record | null (= the human)] for the
  // three script participants (podium places 1-3, or the rival).
  const record = (r, i) => (r == null ? null : typeof r === 'object' && r.entry ? r : cs.riderRecord?.(r, i) ?? null);
  const cast = opts.cast || { ...(cs.defaultCast?.(opts) || { humans: [record(opts.rider, 0)].filter(Boolean), race: [record(opts.rider, 0)].filter(Boolean), ai: [] }), roles: [] };
  if (!opts.cast && opts.roles) cast.roles = opts.roles.map((r, i) => (r == null ? cast.humans?.[0] ?? null : record(r, i)));
  else if (!opts.cast && kind === 'rival') cast.roles = [cast.ai?.[0] ?? null];   // 0x27AC60: D = first computer rider (+0x48)
  const winner = cast.roles?.[0]; const cheat = winner?.entry?.kind === 'cheat' && ((winner.entry.character >= 10 && winner.entry.character <= 20) || winner.entry.character === 28);
  // Peak 1 free ride (web/free-ride.js): location is a course index; the departure and the in-air loop belong to the
  // location being left (its scdat/scfilter), `until` releases the held loop when the destination rows are in.
  const code = (n) => (typeof n === 'number' ? COURSE_CODES[n] ?? null : n);
  // opts.from: the event's course after an event (web/ctm-transport.js, stationFlow)
  const here = opts.from ?? cs.freeRideCourse?.() ?? -1, dest = typeof location === 'number' ? location : -1, bc = (n) => n >= 14 && n <= 16;
  let where = code(location);
  let steps;
  switch (kind) {
    case 'lodge-walkin': case 'station-arrival': steps = lodgeSteps(); break;
    case 'transport-booth': steps = transportArriveSteps(); break;
    case 'transport-depart':
      steps = here >= 17 ? [{ group: bc(here) || bc(dest) ? GROUP.HELI_DEP : GROUP.GOND_DEP, flags: 1 }] : [];
      where = code(here); break;
    case 'transport-ride': {   // pv transportFade: 27A860's one list [departure 12/18 flags 1 (stations only), in-air 13/19, the held loop 14/20 flags 8] + WS10's heli drop (0x235220)
      const heli = bc(here) || bc(dest);
      steps = transportSteps({ heli, departure: here >= 17 });
      if (bc(dest)) steps.push(...arrivalSteps(opts.firstVisit ? code(dest) : dest, !!opts.firstVisit).map((s) => ({ ...s, location: code(dest) })));
      where = code(here) || where;
      if (opts.until) Promise.resolve(opts.until).then(() => { const t = setInterval(() => { if (!cs.active || cs.releaseFade() || !cs.active) clearInterval(t); }, 50); });
      break;
    }
    case 'transport-loop': {
      const heli = bc(here) || bc(dest);
      steps = transportSteps({ heli }).filter((s) => s.group !== GROUP.HELI_DEP && s.group !== GROUP.GOND_DEP);
      where = code(here) || where;
      if (opts.until) Promise.resolve(opts.until).then(() => { const t = setInterval(() => { if (!cs.active || cs.advance() || !cs.active) clearInterval(t); }, 50); });
      break;
    }
    case 'intro': steps = introSteps(mode); break;
    case 'podium': steps = podiumSteps(place, opts.winnerCheat ?? cheat); break;
    case 'rival': steps = rivalSteps(); break;
    case 'transport': steps = transportSteps(opts); break;
    case 'arrival': steps = arrivalSteps(location, !!opts.firstVisit); break;
    case 'lodge': steps = lodgeSteps(); break;
    case 'transport-arrive': steps = transportArriveSteps(); break;
    case 'heat': steps = heatSteps(!!opts.final); break;
    case 'restart': steps = restartSteps(); break;
    default: steps = id == null ? [] : [{ script: +id, flags: opts.flags ?? 1 }];
  }
  if (opts.steps) steps = opts.steps;
  if (!steps.length) return { played: false, skipped: false, scripts: [] };
  return cs.play({
    steps,
    cast,
    location: where || cs.location?.() || 'ARA1',
    hub: opts.hub ?? null,
    kind,
    place,
    onIdle: opts.onIdle ?? null,
    restore: opts.restore,
    skipLock: opts.skipLock ?? 0,
    onStep: opts.onStep ?? null
  });
}
