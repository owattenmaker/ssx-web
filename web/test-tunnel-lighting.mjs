// Stage builtin 74 (0x303F80, DBC2's tunnel volumes) and the rider environment selector (2ED490: x 0.9, + 0.1 while rider+0x3FC;
// above 0.1 the rider irradiance takes the alternate bank; docs/avalanche.md "Port status of these builtins"): the Ruthless run
// local/ps2-capture/runs/tunnel/dbc2-tunnel-ai (the dbc2 tuck line held to 11000; it enters a tunnel volume at 9830 and the
// selector decays to 0 by 10730), the selector bit-exact on every record. Skips without the capture.
//   node test-tunnel-lighting.mjs [CORE_JS=path/to/core.js]
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';

const bin = new URL('../local/ps2-capture/runs/tunnel/dbc2-tunnel-ai.bin', import.meta.url).pathname;
if (!fs.existsSync(bin)) { console.log('tunnel lighting: skipped (capture not present)'); process.exit(0); }
const report = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'tunnel-lighting-')), 'report.json');
execFileSync(process.execPath, ['compare-ai-capture.mjs', bin, '--world-draws', '--zoe', '--report', report], {
  cwd: new URL('.', import.meta.url).pathname, env: { ...process.env, TICK_HOOK: 'environment-selector-hook.mjs' }, stdio: ['ignore', 'ignore', 'ignore'], maxBuffer: 1 << 28 });
const s = JSON.parse(fs.readFileSync(report, 'utf8')).summary;
assert.equal(s.firstHumanInexact, null, `human exact (${JSON.stringify(s.firstHumanInexact)})`);
assert.ok(s.selectorNonzero > 800, `the run goes through a tunnel (${s.selectorNonzero} ticks with a selector)`);
assert.equal(s.selectorExact, s.selectorChecked, `selector bit-exact (first ${JSON.stringify(s.selectorFirst)})`);
console.log(`tunnel lighting: selector bit-exact on ${s.selectorChecked} ticks (${s.selectorNonzero} in or after a tunnel), human exact`);
