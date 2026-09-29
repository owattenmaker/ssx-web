// compare-ps2-capture.mjs TICK_HOOK (web/test-progress-meter.mjs): the progress meter port (web/progress-meter-hud.js)
// fed by the browser core against the PS2 entries 0x4C8BC8 of a capture with --watch 0x4C8BC8:0x90 (local/ps2-capture/
// runs/progress-meter/rnb-meter). The human's inputs come from the web core as main.js reads them (race_progress_info()[0]
// = +0x4D0, rider_world_state()[0..1] = +0x110), the computer rider's from the capture (watches of its +0x4D0 and +0x110).
// Records are written at the human provider exit, between the HUD update 0x20ED20 and the draw 0x20EDA0 of the previous
// frame: record r holds +0..+0xC after the update that used the rider state of record r and +0x10/+0x14 of the draw
// before it. So the port's update for record r is compared with record r (+0..+0xC) and its smoothing with record r+1.
import fs from 'node:fs';
import { ProgressMeter } from './progress-meter-hud.js';

export function create({ core, dv, RECORD, records, captureManifest }) {
  const L = captureManifest.layout; let off = L.watch_offset; const W = {};
  for (const w of L.watches || []) { W[Number(w.address)] = off; off += w.length; }
  const E = W[0x4C8BC8], others = captureManifest.others || [];
  if (E === undefined) throw new Error('progress-meter-compare: capture has no --watch 0x4C8BC8:0x90');
  const npcRem = others.map((a) => W[Number(a) + 0x4D0]), npcPos = others.map((a) => W[Number(a) + 0x110]);
  const code = captureManifest.location;
  const meter = new ProgressMeter(JSON.parse(fs.readFileSync(new URL(`./public/assets/${code}/progress-meter.json`, import.meta.url))));
  const n = 1 + others.length, entry = (r, i) => { const b = r * RECORD + E + 0x18 * i; return [dv.getInt32(b, true), dv.getInt32(b + 4, true), dv.getFloat32(b + 8, true), dv.getFloat32(b + 12, true), dv.getFloat32(b + 16, true), dv.getInt32(b + 20, true)]; };
  // seed from record 0 (the anchor's entries)
  for (let i = 0; i < n; i++) { const e = entry(0, i); Object.assign(meter.entries[i], { reset: e[0], seg: e[1], progress: e[2], lateral: e[3], smooth: e[4], flag: e[5] }); }
  meter.count = n; meter.humanSlot = 0; meter.pending = false;
  const f32 = (ptr, k) => new Float32Array(core.HEAPF32.buffer, ptr, k);
  let checked = 0, exactUpdate = 0, exactSmooth = 0, firstUpdate = null, firstSmooth = null, pendingSmooth = null, percents = {}, prevNpc = null, lastPlacements = null;
  return {
    // called after web tick i (the state record i + 1 holds)
    tick({ i }) {
      const r = i + 1; if (r >= records.length) return;
      const human = { remaining: f32(core._race_progress_info(), 1)[0], x: f32(core._rider_world_state(), 2)[0], y: f32(core._rider_world_state(), 2)[1], human: true,
        placements: f32(core._reset_info(), 3)[2] };
      const npcs = others.map((_, s) => ({ remaining: dv.getFloat32(r * RECORD + npcRem[s], true), x: dv.getFloat32(r * RECORD + npcPos[s], true), y: dv.getFloat32(r * RECORD + npcPos[s] + 4, true) }));
      // computer-rider placements (0x11D660 -> 0x2105B0): the capture has no counter; a teleport of more than 4 m marks one
      npcs.forEach((p, s) => { p.placed = !!prevNpc && Math.hypot(p.x - prevNpc[s].x, p.y - prevNpc[s].y) > 400; }); prevNpc = npcs;
      const humanPlaced = lastPlacements !== null && human.placements !== lastPlacements; lastPlacements = human.placements;
      if (pendingSmooth) { // the previous frame's draw, visible in this record; a placement in this frame cleared +0x14 before the sample
        const [k, got] = pendingSmooth, placed = [humanPlaced, ...npcs.map((p) => p.placed)];
        const want = Array.from({ length: n }, (_, s) => entry(r, s).slice(4));
        if (got.every((g, s) => g[0] === want[s][0] && (placed[s] || g[1] === want[s][1]))) exactSmooth++; else if (!firstSmooth) firstSmooth = { tick: records[k].tick, got, want };
      }
      meter.tick([human, ...npcs], 0, false);
      const got = meter.entries.slice(0, n).map((e) => [e.reset, e.seg, e.progress, e.lateral]);
      const want = Array.from({ length: n }, (_, s) => entry(r, s).slice(0, 4));
      checked++;
      if (JSON.stringify(got) === JSON.stringify(want)) exactUpdate++; else if (!firstUpdate) firstUpdate = { tick: records[r].tick, got, want };
      meter.smooth(); pendingSmooth = [r, meter.entries.slice(0, n).map((e) => [e.smooth, e.flag])];
      percents[records[r].tick] = meter.percent();
    },
    summary() { return { progressMeter: { checked, exactUpdate, exactSmooth, firstUpdate, firstSmooth, percents: Object.fromEntries(Object.entries(percents).filter(([t]) => [119, 597, 1538, 3038, 4539].includes(+t))) } }; },
  };
}
