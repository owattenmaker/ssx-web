// Original in-race progress meter (HUD flag 0x40; docs/slopestyle-bigair.md "Progress meter"). Races and R&B slope
// style: a vertical window of the course 10000 cm long centred on the human, scrolling with it, drawn at the left.
//
// Data: the location's SSB kind-21 record (tools/export_progress_meter.py -> /assets/<X>/progress-meter.json): total
// length, segments {nx, ny, px, py, d} (lateral axis, origin, distance) and markers {type, d} (0 start, 1 checkpoint,
// 2 finish). Loading it (0x2105D0 -> 0x2108F8) resets the six rider entries 0x4C8BC8 (stride 0x18):
//   +0 reset flag, +4 segment, +8 progress, +0xC lateral offset, +0x10 smoothed lateral offset from the human, +0x14 flag.
// Per frame (the HUD owner update 0x1EC1A4 -> 0x20ED20, after the riders): 0x210618 for each rider of the game list
//   progress = total - rider+0x4D0 (0x113128), stepped by at most 138.8889 unless the entry was reset (rider placement
//   0x11D660 -> 0x2105B0 sets +0 for the rider's slot +0x86C), clamped to 0 / total, segment searched from the previous;
//   0x210820: lateral = (y - py) * ny + (x - px) * nx at rider+0x110 (vt+0x28), a reset clears +0x14; then +0 = 0.
// Draw 0x20EDA0 (from 0x1F03A0 when fewer than 2 humans; gp-0x994 = owner+0x3CC bit 6, a loaded path):
//   * the smoothed lateral of every rider (+0x10: other - human clamped to +-2000, stepped by at most 46.2963);
//   * the percent "%d%%" of cvt.w.s(progress / total x 100 + 0.5), fefont x0.6667 centred at (60, 330.6657), shadow (2,2);
//   * an 'offup' arrow (20x12) at the top (flipped at the bottom) for each rider outside the window, x offset by
//     -smooth x 0.005; the 'radarline' bar (11 wide) shortened where the window passes the start or the end; the red
//     fill (5.5 wide, 0x80,0,0) from the bar bottom over progress/total of the visible bar; the rider arrows (human
//     'radar1P ply' 43x23, computer 'radar1P encpu' 30x18, other humans 'radar1P enply' 24x15) at their progress within
//     the window grown by 20 px, clipped to the bar with their V; the start/checkpoint/finish lines 40x8 (checkpoints
//     unless 0x5308D0 bit 9 and gp-0x848+0xC0+0x88).
// The descriptor is owner+0x50 (0x20EC18 of the layout rect (20,110,80,240)); the sprite records 0x4C8C58 (names in the
// ELF static initializer 0x21C240, sizes there, UVs from the HUD atlas objects read in the R&B savestate). Arithmetic is
// EE float (web/ee-scalar-float.js); draw order is the GS priority field (state +8 bits 5..9), text above sprites.
import { add, sub, mul, div } from './ee-scalar-float.js';

const f = Math.fround;
export const STEP = f(138.88890075683594);       // gp-0x54AC progress step per frame
const LAT_CLAMP = 2000, LAT_STEP = f(46.29629898071289), LAT_X = f(0.005000000353902578);
const WINDOW_BACK = 5000, WINDOW = 10000, GROW_PX = 20;
const BAR_HALF = f(5.499532222747803), FILL_HALF = f(2.7497661113739014), TEXT_SCALE = f(0.666700005531311);
// Descriptor owner+0x50 (0x20EC18; words from the R&B / Metro-City / Snow Jam savestates).
export const DESC = { textX: 60, textY: f(330.66571044921875), top: 110, bottom: f(318.66571044921875), barX: 60, barTop: 122,
  barBottom: f(318.66571044921875), scale: f(0.01966656930744648) };
// 0x4C8C58 sprite records {w, h, page, uv [v0, u0, u1, v1]} (texture 0x657 = OV_1-3 'hud ', 0x656 = OV_1-4 'hude').
export const SPRITES = {
  radarline: { w: 8, h: 8, page: 'OV_1-4', uv: [0.517578125, 0.583984375, 0.677734375, 0.537109375] },
  ply: { w: 43, h: 23, page: 'OV_1-3', uv: [0.505859375, 0.677734375, 0.818359375, 0.583984375] },
  encpu: { w: 30, h: 18, page: 'OV_1-3', uv: [0.935546875, 0.595703125, 0.689453125, 0.994140625] },
  enply: { w: 24, h: 15, page: 'OV_1-3', uv: [0.857421875, 0.595703125, 0.689453125, 0.912109375] },
  offup: { w: 20, h: 12, page: 'OV_1-3', uv: [0.009765625, 0.755859375, 0.845703125, 0.060546875] },
  chkstart: { w: 40, h: 8, page: 'OV_1-4', uv: [0.630859375, 0.861328125, 0.998046875, 0.650390625] },
  chkpt: { w: 40, h: 8, page: 'OV_1-4', uv: [0.607421875, 0.861328125, 0.998046875, 0.626953125] },
};
const MARKER_SPRITE = ['chkstart', 'chkpt', 'chkstart'];

export async function loadProgressMeter(course, fetchImpl = globalThis.fetch) {
  if (!course?.root) return null;
  try { const r = await fetchImpl(course.root + 'progress-meter.json'); return r.ok ? new ProgressMeter(await r.json()) : null; } catch { return null; }
}

// Per-tick inputs from the cores (slot order: the human core is slot 0, web/ai-racers.js npcs carry their slot):
// +0x4D0 = race_progress_info()[0], +0x110/+0x114 = rider_world_state()[0..1] (original cm), placements reset_info()[2].
export function progressMeterRiders(human, npcs = []) {
  const read = (core, isHuman) => {
    const p = new Float32Array(core.HEAPF32.buffer, core._race_progress_info(), 1), w = new Float32Array(core.HEAPF32.buffer, core._rider_world_state(), 2);
    return { remaining: p[0], x: w[0], y: w[1], human: isHuman, placements: new Float32Array(core.HEAPF32.buffer, core._reset_info(), 3)[2] };
  };
  const riders = [read(human, true)];
  for (const n of npcs) if (n?.core) riders[n.slot] = read(n.core, false);
  for (let i = 0; i < riders.length; i++) if (!riders[i]) riders[i] = { remaining: 0, x: 0, y: 0, human: false };
  return riders;
}

export class ProgressMeter {
  constructor(data) {
    this.total = f(data.total); this.segments = data.segments.map((s) => s.map(f)); this.markers = data.markers.map(([t, d]) => [t, f(d)]);
    this.load();
  }
  // 0x2108F8 (path load): every entry reset, nothing drawn before the first update (gp-0x990).
  load() { this.entries = Array.from({ length: 6 }, () => ({ reset: 1, seg: 0, progress: 0, lateral: 0, smooth: 0, flag: 0 })); this.pending = true; this.placements = []; }
  // 0x2105B0 from the rider placement 0x11D660 (event start, restart, reset): the slot's next update snaps.
  place(slot) { if (this.entries[slot]) this.entries[slot].reset = 1; }
  // 0x210618 + 0x210820 for one slot; r = {remaining (+0x4D0), x, y (+0x110/+0x114)}.
  updateEntry(slot, r) {
    const e = this.entries[slot], segs = this.segments, n = segs.length;
    let p = sub(this.total, f(r.remaining));
    if (!e.reset) { const d = sub(p, e.progress); if (STEP < d) p = add(e.progress, STEP); else if (d < -STEP) p = sub(e.progress, STEP); }
    if (p <= 0) { e.seg = 0; e.progress = 0; }
    else if (segs[n - 1][4] <= p) { e.seg = n - 1; e.progress = this.total; }
    else {
      let a = e.seg;
      if (segs[a][4] <= p) do a++; while (segs[a][4] <= p);
      if (p < segs[a][4]) do a--; while (p < segs[a][4]);
      e.seg = a; e.progress = p;
    }
    const [nx, ny, px, py] = segs[e.seg];
    e.lateral = add(mul(sub(f(r.y), py), ny), mul(sub(f(r.x), px), nx));
    if (e.reset) e.flag = 0;
  }
  // One game frame: riders[slot] = {remaining, x, y, human, placed}; human = the HUD's rider slot. The 20EDA0 smoothing
  // runs when the meter is drawn (drawn = false leaves it, as a hidden HUD skips 0x20EDA0).
  // `placements` (a rider's reset-placement counter, reset_info()[2]) marks a placement when it changes.
  tick(riders, humanSlot = 0, drawn = true) {
    for (let i = 0; i < riders.length; i++) {
      const r = riders[i], n = r?.placements;
      if (r?.placed || (n !== undefined && this.placements[i] !== undefined && n !== this.placements[i])) this.place(i);
      this.placements[i] = n;
    }
    for (let i = 0; i < riders.length; i++) { this.updateEntry(i, riders[i]); this.entries[i].reset = 0; }
    this.count = riders.length; this.humanSlot = humanSlot; this.humans = riders.map((r) => !!r?.human); this.pending = false;
    if (drawn) this.smooth();
  }
  smooth() {
    const self = this.entries[this.humanSlot].lateral;
    for (let i = 0; i < 6; i++) {
      if (i >= this.count) continue;
      const e = this.entries[i]; let d = sub(e.lateral, self);
      if (d < -LAT_CLAMP) d = -LAT_CLAMP; else if (LAT_CLAMP < d) d = LAT_CLAMP;
      if (e.flag) { const g = sub(d, e.smooth); if (LAT_STEP < g) d = add(e.smooth, LAT_STEP); else if (g < -LAT_STEP) d = sub(e.smooth, LAT_STEP); }
      e.smooth = d; e.flag = 1;
    }
  }
  percent() { const q = div(this.entries[this.humanSlot ?? 0].progress, this.total); return Math.trunc(add(mul(q, 100), 0.5)); }
  // The 0x20EDA0 submissions: {kind: 'sprite'|'rect'|'text', order, ...} in submission order (PS2 640x480 HUD space).
  frame({ showCheckpoints = true } = {}) {
    if (this.pending || this.count == null) return [];
    const D = DESC, out = [], hs = this.humanSlot, E = this.entries, count = this.count;
    const self = E[hs].progress, lo = sub(self, WINDOW_BACK), hi = add(lo, WINDOW), q = div(E[hs].progress, this.total);
    out.push({ kind: 'text', text: `${Math.trunc(add(mul(q, 100), 0.5))}%`, x: D.textX, y: D.textY, scale: TEXT_SCALE, order: 10 });
    const quad = (name, x0, x1, y0, y1, v0, v1, order) => { const s = SPRITES[name]; out.push({ kind: 'sprite', page: s.page, x0, x1, y0, y1, u0: s.uv[1], u1: s.uv[2], v0, v1, order, name }); };
    // riders outside the window
    const off = SPRITES.offup, offHalf = mul(off.w, 0.5);
    for (let i = 0; i < count; i++) {
      const p = E[i].progress; if (lo <= p && p <= hi) continue;
      const x = add(D.barX, mul(-E[i].smooth, LAT_X)), ahead = hi < p, y0 = ahead ? D.top : D.bottom;
      quad('offup', sub(x, offHalf), add(x, offHalf), y0, add(y0, off.h), ahead ? off.uv[0] : off.uv[3], ahead ? off.uv[3] : off.uv[0], 10);
    }
    // bar and fill
    let yBottom = D.barBottom, yTop = D.barTop;
    if (this.total < hi) yTop = add(yTop, mul(sub(hi, this.total), D.scale));
    if (lo < 0) yBottom = add(yBottom, mul(lo, D.scale));
    const bar = SPRITES.radarline; quad('radarline', sub(D.barX, BAR_HALF), add(D.barX, BAR_HALF), yTop, yBottom, bar.uv[3], bar.uv[0], 9);
    out.push({ kind: 'rect', x0: sub(D.barX, FILL_HALF), x1: add(D.barX, FILL_HALF), y0: add(yBottom, mul(sub(yTop, yBottom), q)), y1: yBottom, rgb: [128, 0, 0], order: 10 });
    // rider arrows (window grown by 20 px)
    const grow = div(GROW_PX, D.scale), hi2 = add(hi, grow), lo2 = sub(lo, grow);
    const clip = (s, y, vTop, vBottom, twoH) => { // 0x20F72C..0x20F7B4 / 0x20F96C..0x20F9E4
      const half = mul(s.h, 0.5); let top = sub(y, half), bottom = add(y, half);
      if (D.barBottom <= top || bottom <= D.barTop) return null;
      if (top < D.barTop) { vTop = add(vBottom, div(mul(sub(vTop, vBottom), sub(bottom, D.barTop)), twoH ? add(half, half) : s.h)); top = D.barTop; }
      else if (D.barBottom < bottom) { vBottom = add(vTop, div(mul(sub(vBottom, vTop), sub(D.barBottom, top)), twoH ? add(half, half) : s.h)); bottom = D.barBottom; }
      return [top, bottom, vTop, vBottom];
    };
    for (let i = 0; i < count; i++) {
      const p = E[i].progress; if (p < lo2 || hi2 < p) continue;
      const y = sub(D.barBottom, mul(sub(p, lo), D.scale)); let x = D.barX, name, order;
      if (i === hs) { name = 'ply'; order = 12; } else { x = sub(x, mul(E[i].smooth, LAT_X)); [name, order] = this.humans[i] ? ['enply', 13] : ['encpu', 14]; }
      const s = SPRITES[name], c = clip(s, y, s.uv[0], s.uv[3], true); if (!c) continue;
      const hw = mul(s.w, 0.5); quad(name, sub(x, hw), add(x, hw), c[0], c[1], c[2], c[3], order);
    }
    // start / checkpoint / finish lines
    for (const [type, d] of this.markers) {
      if (d < lo2 || hi2 < d) continue;
      if (type === 1 && !showCheckpoints) continue;
      const name = MARKER_SPRITE[type]; if (!name) continue;
      const s = SPRITES[name], y = sub(D.barBottom, mul(sub(d, lo), D.scale)), c = clip(s, y, s.uv[0], s.uv[3], false); if (!c) continue;
      const hw = mul(s.w, 0.5); quad(name, sub(D.barX, hw), add(D.barX, hw), c[0], c[1], c[2], c[3], 11);
    }
    return out;
  }
  // Canvas draw (ui canvas 640x448 = PS2 640x480 HUD space scaled); `ui` provides images and the fefont glyphs/measure.
  draw(ui, ctx, options) {
    const list = this.frame(options); if (!list.length) return;
    ctx.save(); ctx.scale(1, 448 / 480); ctx.globalAlpha = 1;
    const rank = (d) => d.order * 2 + (d.kind === 'text' ? 1 : 0);
    for (const d of list.map((d, k) => [d, k]).sort((a, b) => rank(a[0]) - rank(b[0]) || a[1] - b[1]).map((x) => x[0])) {
      if (d.kind === 'rect') { ctx.fillStyle = `rgb(${d.rgb.join(',')})`; ctx.fillRect(d.x0, d.y0, d.x1 - d.x0, d.y1 - d.y0); }
      else if (d.kind === 'sprite') {
        const im = ui.images[d.page]; if (!im) continue; const W = im.width, H = im.height, flip = d.v1 < d.v0;
        const sy = Math.min(d.v0, d.v1) * H, sh = Math.abs(d.v1 - d.v0) * H; if (sh <= 0 || d.y1 <= d.y0) continue;
        ctx.save(); ctx.translate(d.x0, flip ? d.y1 : d.y0); ctx.scale(1, flip ? -1 : 1);
        ctx.drawImage(im, d.u0 * W, sy, (d.u1 - d.u0) * W, sh, 0, 0, d.x1 - d.x0, d.y1 - d.y0); ctx.restore();
      } else if (d.kind === 'text' && ui.trickHud && ui.trickHudRenderer) { // 0x392908: centred on x (0x391FB0 width incl. shadow), 0x391CB0
        const w = ui.trickHud.measure('FEFONT', d.text, d.scale, d.scale).w, x = sub(d.x, mul(w, 0.5));
        const t = { font: 'FEFONT', text: d.text, x, y: d.y, scale: [d.scale, d.scale] };
        ui.trickHudRenderer.text(ctx, t, 2, 2, [1, 0, 0, 0]); ui.trickHudRenderer.text(ctx, t, 0, 0, [1, 1, 1, 1]);
      }
    }
    ctx.restore();
  }
}
