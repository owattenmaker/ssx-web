// Full-course regression: Snow Jam event runs from several grid slots with varied played inputs (tucks, crouch/jump,
// spins, flips, grabs, carving) must reach the finish without any core error. The original never throws; a core
// exception here means a live path hit an unported/unsupported case (e.g. an instance routed as a dynamic entity
// the port does not model). Runs are independent processes: `node test-long-runs.mjs` runs all of them in parallel,
// `node test-long-runs.mjs <slot> <pattern> <offset>` runs one.
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { createTestRider, json, scriptedPad } from './net/test-core.mjs';
// scriptedPad carves and crouches a lot: its runs take ~18000..42000 ticks. The runs are chaotic and have no PS2 capture:
// since the instance walks follow the PS2 octree order (2026-09-26, docs/obstacle-collision.md) slot 5 pattern 1 offset 2
// crashes differently at 7843 and finishes at 42421 (was 36241), with no Select reset and no core error.
const MAX_TICKS = 60 * 60 * 13;
// Pattern 0: tuck 200 / crouch 60 / spin + grab 68 every 400 ticks (the multiplayer repro that once threw
// OriginalAirTrajectoryUnavailable on ARA1 mdl_ARA1_endmode_collide_1005 and OriginalCrashWorldUnavailable).
// Pattern 1: net/test-core.mjs scriptedPad (carving, jumps with spins/flips, grabs, tucks).
const patterns = [
  (t, off) => { const p = new Float32Array(24), ph = (t + off) % 400; p[22] = ph < 200 ? 1 : 0; if (ph >= 200 && ph < 260) p[10] = 1; if (ph >= 262 && ph < 330) { p[5] = 1; p[12] = 1; } return p; },
  (t, off) => scriptedPad(t, off),
];
async function run(slot, pattern, off) {
  const doc = json('ARA1/npc-riders.json');
  const r = await createTestRider({ gridSeed: slot ? JSON.stringify(doc.riders[slot - 1].ground.state) : '' }); r.startEvent();
  // A scripted rider can wedge itself in a pocket and cycle soft collisions / crashes at walking speed, as a player can on the
  // PS2 (slot 0 pattern 1 offset 3 near (-2834, -4047, -1330) since the PS2-verified control-3 landing, docs/peak3.md): with
  // no course progress for 60 s the test presses Select for 2 ticks (ResetPath -> 116120, the tech-select-* captures), as a
  // player would. The run must still finish without a core error.
  let best = Infinity, bestAt = 0, selects = 0, select = 0;
  for (let t = 0; t < MAX_TICKS; t++) {
    const pad = patterns[pattern](t, off); if (select > 0) { pad[0] = 1; select--; }
    try { r.tick(pad); } catch (e) { return { ok: false, why: `core error at tick ${t}: ${e.message}` }; }
    const progress = new Float32Array(r.core.HEAPF32.buffer, r.core._race_progress_info(), 8);
    if (progress[3] >= 0) return { ok: true, why: `finished at ${t}${selects ? ` (${selects} Select reset${selects > 1 ? 's' : ''} after a stall)` : ''}` };
    if (progress[1] < best - 100) { best = progress[1]; bestAt = t; } else if (t - bestAt > 3600) { select = 2; selects++; bestAt = t; }
  }
  return { ok: false, why: `no finish within ${MAX_TICKS} ticks` };
}
const argv = process.argv.slice(2).map(Number);
if (argv.length === 3) {
  const result = await run(...argv); console.log(JSON.stringify(result)); process.exit(result.ok ? 0 : 1);
}
const cases = [[2, 0, 0], [0, 0, 0], [1, 0, 137], [3, 0, 251], [4, 1, 1], [5, 1, 2], [0, 1, 3], [2, 1, 4]];
const self = fileURLToPath(import.meta.url);
const results = await Promise.all(cases.map((c) => new Promise((resolve) => {
  const child = spawn(process.execPath, [self, ...c.map(String)], { stdio: ['ignore', 'pipe', 'inherit'] }); let out = '';
  child.stdout.on('data', (d) => { out += d; });
  child.on('close', (code) => { let r; try { r = JSON.parse(out.trim().split('\n').pop()); } catch { r = { ok: false, why: `exit ${code}: ${out.trim()}` }; } resolve({ c, ...r, ok: r.ok && code === 0 }); });
})));
let failed = 0;
for (const r of results) { console.log(`slot ${r.c[0]} pattern ${r.c[1]} offset ${r.c[2]}: ${r.ok ? 'ok' : 'FAIL'} (${r.why})`); if (!r.ok) failed++; }
if (failed) { console.error(`${failed} long run(s) failed`); process.exit(1); }
console.log(`Long runs: ${results.length} full Snow Jam runs finished without core errors.`);
