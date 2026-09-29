// PS2 rider shadow box fit (374D00) and terrain UV rows (web/rider-shadow.js) against the shadow objects of PS2
// savestates (local/reference/rider-shadow/fixture-*.json: every rider's bones, up column rider+0x1C0 and the fitted
// light-space bounds / UV matrix read from RAM). The quaternion -> up conversion of pose_physical is checked by the
// core replay in docs (human rider, setpieces/full 418 / 969).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {fitShadow, receiverRows} from './rider-shadow.js';

const dir = '../local/reference/rider-shadow/';
if (!fs.existsSync(dir)) { console.log('rider shadow: fixtures missing, skipped'); process.exit(0); }
let riders = 0, worst = 0, worstUv = 0;
for (const file of fs.readdirSync(dir).filter((f) => f.endsWith('.json'))) {
  const fixture = JSON.parse(fs.readFileSync(dir + file, 'utf8'));
  for (const r of fixture.riders) {
    if (!r.out || !r.visibleB18) continue; // a rider that is not visible keeps a stale shadow object
    const bones = [5, 10, 15, 18, 21, 0].map((b) => r.bones[String(b)]), fit = fitShadow(bones, r.up_1C0);
    const [minx, maxx, maxy, miny] = r.out.bounds_minx_maxx_maxy_miny;
    for (const [a, b] of [[fit.minx, minx], [fit.maxx, maxx], [fit.maxy, maxy], [fit.miny, miny]]) { worst = Math.max(worst, Math.abs(a - b)); assert.ok(Math.abs(a - b) < .05, `${file} ${r.rider}: bounds ${a} vs ${b}`); }
    for (let k = 0; k < 3; k++) { assert.ok(Math.abs(fit.A[k] - r.out.A[k]) < 1e-6 && Math.abs(fit.B[k] - r.out.B[k]) < 1e-6, `${file} ${r.rider}: axes`); }
    // Terrain UV rows over browser metres (origin 0) against the RAM UV matrix (stored transposed) at the six bones.
    const rows = receiverRows(fit, [0, 0, 0]), uv = r.out.uv;
    for (const p of bones) {
      const b = [p[0] / 100, p[2] / 100, -p[1] / 100, 1], dot = (row) => row[0] * b[0] + row[1] * b[1] + row[2] * b[2] + row[3];
      const u = p[0] * uv[0][0] + p[1] * uv[1][0] + p[2] * uv[2][0] + uv[3][0], v = p[0] * uv[0][1] + p[1] * uv[1][1] + p[2] * uv[2][1] + uv[3][1];
      worstUv = Math.max(worstUv, Math.abs(dot(rows[0]) - u), Math.abs(dot(rows[1]) - v));
      assert.ok(Math.abs(dot(rows[0]) - u) < 5e-4 && Math.abs(dot(rows[1]) - v) < 5e-4, `${file} ${r.rider}: uv ${dot(rows[0])},${dot(rows[1])} vs ${u},${v}`);
      assert.ok(Math.abs(dot(rows[2]) - (fit.C[2] - p[2])) < 1e-3, 'depth below the feet');
    }
    riders++;
  }
}
assert.ok(riders >= 6, `only ${riders} rider shadows checked`);
console.log(`rider shadow fit: ${riders} PS2 shadow objects, bounds within ${worst.toFixed(3)} cm, terrain UV within ${worstUv.toExponential(1)}`);
