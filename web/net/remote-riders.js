// Remote racers on this client: their streamed state (web/net/rider-packet.js), rebuilt skin palettes, the
// kinematic "ghost" the local rider collides with, and what web/opponent-riders.js draws.
//
// Time base: every racer counts race ticks from the shared GO instant (web/net/mp-game.js), so a packet's tick says
// where on the common 60 Hz timeline its state belongs.
//   * Pose: drawn DELAY ticks behind this client's tick, interpolated between the two packets around that tick
//     (a remote rider's palette is rebuilt bit for bit from each packet's pose, web/net/pose-codec.js, then blended).
//   * Position: the drawn body is moved to where the rider is *now* -- the newest packet's physical position
//     advanced by its velocity to this client's tick (dead reckoning, at most MAX_LEAD ticks) -- and the correction
//     is eased in, so a remote rider is drawn where this client's pair contacts find it.
//   * Ghost: the newest packet's pair view (body spheres, velocity, attack state; web/npc_gameplay.inc pair_view)
//     moved by the same prediction; web/net/pair-net.js hands it to the original 107888 pair system.
// A placement serial change (reset / rescue teleport) is never interpolated or predicted across.
import { decodeFrame, STATE } from './rider-packet.js';
import { skinLayout, buildPalette } from './pose-codec.js';

export const DELAY_TICKS = 6;        // 100 ms: two packets at 20 Hz are always around the drawn tick
export const MAX_LEAD = 20;          // dead reckoning at most 1/3 s past the newest packet
export const STALE_TICKS = 45;       // no packet for 3/4 s: the ghost leaves the pair world (disconnect / hitch)
const EASE_SECONDS = 0.12;
let SECOND_ORDER = false; const MAX_ACCEL = 2500; // cm/s^2 (off: measured to add spurious contacts, docs/multiplayer.md)
export function setSecondOrder(on) { SECOND_ORDER = !!on; } // QA (latency measurements)
// Position-type words of the pair view that move with the rider (broad centre, 20 sphere centres, position,
// presentation origin, physical position); everything else (velocity, normals, frames' axes) is kept.
const MOVING = [3, ...Array.from({ length: 20 }, (_, i) => 7 + 4 * i), 99, 120, 132];

// Stand-in "core" for web/opponent-riders.js capture(): the palette/indices exports it reads.
export function createRemoteCore(layout) {
  const groups = layout.groups.length, paletteOffset = 16, indicesOffset = paletteOffset + groups * 64;
  const heap = new ArrayBuffer(indicesOffset + layout.slots.length * 4);
  const core = { HEAPU8: new Uint8Array(heap), HEAPF32: new Float32Array(heap), groups, hasPose: false,
    _rider_skin_palette_count: () => (core.hasPose ? groups : 0), _rider_skin_palette: () => paletteOffset, _rider_skin_palette_indices: () => indicesOffset,
    palette: new Float32Array(heap, paletteOffset, groups * 16) };
  new Uint32Array(heap, indicesOffset, layout.slots.length).set(layout.slots);
  return core;
}

export function createRemoteRiders({ delayTicks = DELAY_TICKS } = {}) {
  const racers = new Map(); // slot -> racer
  const stats = { packets: 0, bytes: 0, rejected: 0, palettes: 0 };
  function paletteOf(r, snap) {
    if (!snap.palette) {
      if (snap.pose.length !== r.layout.bones * 7) throw new Error(`remote rider ${r.slot}: ${snap.pose.length / 7} bones, package has ${r.layout.bones}`);
      snap.palette = buildPalette(r.layout, snap.pose, snap.scale); stats.palettes++;
    }
    return snap.palette;
  }
  // Snapshots a, b around tick t and the blend fraction (no blend across a teleport).
  function bracket(r, t) {
    const s = r.snapshots; if (!s.length) return null;
    if (t <= s[0].tick) return { a: s[0], b: s[0], f: 0 };
    for (let i = s.length - 1; i >= 0; i--) if (s[i].tick <= t) {
      const a = s[i], b = s[i + 1] ?? a;
      if (b === a || b.serial !== a.serial) return { a, b: a, f: 0 };
      return { a, b, f: Math.min(1, (t - a.tick) / Math.max(1, b.tick - a.tick)) };
    }
    return { a: s[0], b: s[0], f: 0 };
  }
  // Physical position (source cm) at tick t from the newest packet: dead reckoning, bounded lead, plus the
  // predicted responses of contacts this client resolved after that packet (correct(): the rider's own client
  // applies them, its later packets carry them).
  function predicted(r, t, out = [0, 0, 0]) {
    const n = r.latest, dt = Math.max(-MAX_LEAD, Math.min(MAX_LEAD, t - n.tick)) / 60, acc = accelerationOf(r);
    for (let k = 0; k < 3; k++) out[k] = n.position[k] + n.velocity[k] * dt + (acc && dt > 0 ? 0.5 * acc[k] * dt * dt : 0);
    for (const c of r.corrections) {
      const since = Math.max(0, Math.min(MAX_LEAD, t - c.tick)) / 60;
      for (let k = 0; k < 3; k++) out[k] += (c.dx ? c.dx[k] : 0) + (c.dv ? c.dv[k] * since : 0);
    }
    return out;
  }
  // Second-order dead reckoning: the acceleration between the two newest packets (same placement serial), bounded.
  function accelerationOf(r) {
    if (!SECOND_ORDER) return null;
    const s = r.snapshots, n = r.latest, p = s.length > 1 ? s[s.length - 2] : null;
    if (!p || p.serial !== n.serial || n.tick <= p.tick) return null;
    const dt = (n.tick - p.tick) / 60, a = [0, 1, 2].map((k) => (n.velocity[k] - p.velocity[k]) / dt), m = Math.hypot(...a);
    return m > MAX_ACCEL ? a.map((x) => x * MAX_ACCEL / m) : a;
  }
  const correctedVelocity = (r) => { const v = r.latest.velocity.slice(); for (const c of r.corrections) if (c.dv) for (let k = 0; k < 3; k++) v[k] += c.dv[k]; return v; };
  const api = {
    racers, stats,
    // slot -> the racer's package rig (rider.json) and lobby info.
    add(slot, rig, info = {}) { const layout = skinLayout(rig); racers.set(slot, { slot, info, layout, core: createRemoteCore(layout), snapshots: [], history: new Map(), corrections: [], latest: null, offset: [0, 0, 0], offsetSerial: -1, lastFrameMs: 0, receivedMs: 0, gone: false }); },
    remove(slot) { racers.delete(slot); },
    has(slot) { return racers.has(slot); },
    // One relayed state frame ([slot, ...packet]); returns the decoded packet (or null).
    receive(frame, nowMs = performance.now()) {
      const s = decodeFrame(frame);
      if (!s || s.kind !== STATE) { if (!s) stats.rejected++; return s; }
      const r = racers.get(s.slot); if (!r) return s;
      stats.packets++; stats.bytes += frame.length;
      if (r.latest && s.tick <= r.latest.tick) return s; // TCP keeps order; a restarted sender restarts at a later tick
      r.snapshots.push(s); if (r.snapshots.length > 12) r.snapshots.shift();
      r.history.set(s.tick, [s.position[0], s.position[1], s.position[2], s.remaining]); // the ranking's exact inputs
      if (r.history.size > 160) for (const k of r.history.keys()) { if (k >= s.tick - 480) break; r.history.delete(k); }
      r.latest = s; r.receivedMs = nowMs; r.gone = false;
      r.corrections = r.corrections.filter((c) => c.tick > s.tick); // the rider's own client applied these by now
      return s;
    },
    // A contact response this client predicts for the remote rider at `tick` (displacement dx and/or velocity change
    // dv, source cm and cm/s), held until a packet of a later tick arrives.
    correct(slot, tick, dx, dv) { const r = racers.get(slot); if (r?.latest) { r.corrections.push({ tick, dx, dv }); stats.corrections = (stats.corrections ?? 0) + 1; } },
    markGone(slot, gone = true) { const r = racers.get(slot); if (r) r.gone = gone; },
    // A racer who left the race (quit, left the lobby, reload): no longer drawn.
    hide(slot) { const r = racers.get(slot); if (r) { r.gone = true; r.hidden = true; } },
    // Live = has state and is not stale at this tick (pairs and the ranking use it).
    live(slot, tick) { const r = racers.get(slot); return !!r?.latest && !r.gone && tick - r.latest.tick <= STALE_TICKS; },
    // The ghost pair view at `tick` (140 words, positions predicted), or null.
    ghost(slot, tick) {
      const r = racers.get(slot); if (!r?.latest?.pair) return null;
      const words = r.latest.pair.slice(), f = new Float32Array(words.buffer), p = predicted(r, tick), d = [p[0] - r.latest.position[0], p[1] - r.latest.position[1], p[2] - r.latest.position[2]];
      if (r.corrections.length) { const v = correctedVelocity(r); for (let k = 0; k < 3; k++) f[87 + k] = Math.fround(v[k]); }
      const spheres = words[0] ? Math.min(words[1], 20) : 0;
      for (const at of MOVING) {
        if (at >= 7 && at < 87 && (at - 7) / 4 >= spheres) continue;
        if (at === 120 && !words[110]) continue;
        for (let k = 0; k < 3; k++) f[at + k] = Math.fround(f[at + k] + d[k]);
      }
      return words;
    },
    // The exact streamed [x, y, z, remaining] of packet tick `tick` (web/net/pair-net.js ranking), the last one of a
    // racer that stopped streaming (finished / gone), or null while it is still on its way.
    exactAt(slot, tick, stale = false) {
      const r = racers.get(slot); if (!r?.latest) return !r || r.gone ? [0, 0, 0, 1e9] : null; // never raced: last
      const h = r.history.get(tick); if (h) return h;
      if (tick > r.latest.tick && (stale || r.gone || r.latest.finished || r.latest.dnf)) return r.history.get(r.latest.tick) ?? null;
      return null;
    },
    // The race world inputs at a past tick every client shares (web/net/pair-net.js ranks everyone at the same
    // instant): position (source cm) and remaining distance interpolated between the packets around `tick`,
    // extrapolated from the newest when the link is slower than the delay. [x, y, z, remaining] or null.
    sampleAt(slot, tick) {
      const r = racers.get(slot); if (!r?.latest) return null;
      if (tick > r.latest.tick) return api.rankInput(slot, tick);
      const exact = r.snapshots.find((x) => x.tick === tick);
      if (exact) return [...exact.position, exact.remaining];
      const { a, b, f } = bracket(r, tick);
      return [0, 1, 2].map((k) => a.position[k] + (b.position[k] - a.position[k]) * f).concat(a.remaining + (b.remaining - a.remaining) * f);
    },
    // Ranking inputs at `tick`: [x, y, z (source cm), remaining] or null.
    // The remaining distance is advanced at the rate of the last two packets (same placement serial).
    rankInput(slot, tick) {
      const r = racers.get(slot), n = r?.latest; if (!n) return null;
      const p = predicted(r, tick), s = r.snapshots, prev = s.length > 1 ? s[s.length - 2] : null;
      const rate = prev && prev.serial === n.serial && n.tick > prev.tick && !n.finished ? (n.remaining - prev.remaining) / (n.tick - prev.tick) : 0;
      const lead = Math.max(0, Math.min(MAX_LEAD, tick - n.tick));
      return [p[0], p[1], p[2], Math.max(0, n.remaining + rate * lead)];
    },
    // Render: the opponents array for opponent-riders.js ({core, visible, reset}), in `order` (renderer entry order).
    // tick = this client's race tick + render alpha (fractional).
    frame(order, tick, nowMs = performance.now()) {
      const out = [];
      for (const slot of order) {
        const r = racers.get(slot);
        if (!r?.latest || r.hidden) { out.push({ core: null }); continue; }
        const br = bracket(r, tick - delayTicks), { a, b, f } = br, pa = paletteOf(r, a), pb = paletteOf(r, b), dst = r.core.palette;
        for (let i = 0; i < dst.length; i++) dst[i] = f ? pa[i] + (pb[i] - pa[i]) * f : pa[i];
        // Correction to the predicted present position, eased (snapped after a teleport).
        const now = predicted(r, tick), drawn = [0, 1, 2].map((k) => a.position[k] + (b.position[k] - a.position[k]) * f);
        const target = [now[0] - drawn[0], now[1] - drawn[1], now[2] - drawn[2]];
        const dt = r.lastFrameMs ? Math.min(0.25, (nowMs - r.lastFrameMs) / 1000) : 1; r.lastFrameMs = nowMs;
        const ease = r.offsetSerial !== r.latest.serial || a.serial !== r.latest.serial ? 1 : 1 - Math.exp(-dt / EASE_SECONDS);
        r.offsetSerial = r.latest.serial;
        for (let k = 0; k < 3; k++) r.offset[k] += (target[k] - r.offset[k]) * ease;
        for (let g = 0; g < dst.length; g += 16) for (let k = 0; k < 3; k++) dst[g + 12 + k] += r.offset[k];
        r.core.hasPose = true;
        r.drawn = { a, b, f, offset: r.offset.slice() };
        out.push({ core: r.core, visible: true, reset: true });
      }
      return out;
    },
    // The world pose (world_pose_bones layout) at `tick`: the packet's own pose on a packet tick (exact), else
    // positions lerped and quaternions normalized-lerped between the packets around it (the FX puppet's bones).
    poseAt(slot, tick) {
      const r = racers.get(slot), br = r && bracket(r, tick); if (!br) return null;
      const { a, b, f } = br; if (!f || a === b) return a.pose;
      const out = new Float32Array(a.pose.length), pa = a.pose, pb = b.pose;
      for (let o = 0; o < out.length; o += 7) {
        for (let k = 0; k < 3; k++) out[o + k] = pa[o + k] + (pb[o + k] - pa[o + k]) * f;
        const sign = pa[o + 3] * pb[o + 3] + pa[o + 4] * pb[o + 4] + pa[o + 5] * pb[o + 5] + pa[o + 6] * pb[o + 6] < 0 ? -1 : 1;
        let n = 0; for (let k = 3; k < 7; k++) { out[o + k] = pa[o + k] + (sign * pb[o + k] - pa[o + k]) * f; n += out[o + k] * out[o + k]; }
        n = Math.sqrt(n) || 1; for (let k = 3; k < 7; k++) out[o + k] /= n;
      }
      return out;
    },
    // The drawn rider's lighting inputs (bounds / rank point moved like the body), or null.
    lighting(slot) {
      const r = racers.get(slot), d = r?.drawn; if (!d?.a.lighting) return null;
      const la = d.a.lighting, lb = d.b.lighting ?? la, f = lb === la ? 0 : d.f, o = [0, 1, 2].map((k) => d.a.position[k] + (d.b.position[k] - d.a.position[k]) * f + d.offset[k]);
      const irradiance = new Float32Array(40); for (let i = 0; i < 40; i++) irradiance[i] = la.irradiance[i] + (lb.irradiance[i] - la.irradiance[i]) * f;
      const shift = [0, 1, 2].map((k) => o[k] - d.a.position[k]);
      return { irradiance, rim: la.rim, bounds: la.bounds.map((v, i) => v + shift[i % 3]), point: [0, 1, 2].map((k) => la.point[k] + shift[k]) };
    },
    clear() { racers.clear(); stats.packets = stats.bytes = stats.rejected = stats.palettes = stats.corrections = 0; },
  };
  return api;
}
