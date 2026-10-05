// Score every ps2-captures gate one by one against a capture tree and a core (docs/ps2-float.md "Scoring").
// web/test-ps2-captures.mjs stops at the first failing gate; this runs it once per gate (ONLY=name) and keeps going,
// so a whole tree gets a pass / fail / skip line per gate and a tally per area (the name's first path part).
//
// usage: node tools/ps2-float/score_gates.mjs --runs DIR --core CORE.js [--par N] [--only a,b] [--out FILE.json]
import fs from 'node:fs';
import path from 'node:path';
import { execFile } from 'node:child_process';

const argument = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};
const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), '../..');
const runs = path.resolve(argument('--runs', path.join(root, 'local/ps2-capture/runs')));
const core = argument('--core', '');
const par = Number(argument('--par', '2'));
const out = argument('--out', '');
const onlyList = argument('--only', '');

const source = fs.readFileSync(path.join(root, 'web/test-ps2-captures.mjs'), 'utf8');
const ids = [...source.match(/RIDER_GATE_IDS = \[([\s\S]*?)\];/)[1].matchAll(/'([^']+)'/g)].map((m) => m[1]);
let names = [...new Set([...ids.flatMap((id) => [`riders/${id}-race`, `riders/${id}-hl`]), ...[...source.matchAll(/name: '([^']+)'/g)].map((m) => m[1])])];
if (onlyList) {
  const wanted = new Set(onlyList.split(','));
  names = names.filter((name) => wanted.has(name));
}
names = names.filter((name) => fs.existsSync(path.join(runs, `${name}.bin`)));

const score = (name) => new Promise((resolve) => {
  const env = { ...process.env, ONLY: name, PS2_RUNS: runs, PAR: '1', ...(core ? { CORE_JS: path.resolve(core) } : {}) };
  execFile(process.execPath, ['test-ps2-captures.mjs'], { cwd: path.join(root, 'web'), env, encoding: 'utf8', maxBuffer: 1 << 26 }, (error, stdout, stderr) => {
    const text = `${stdout}\n${stderr}`;
    const skipped = stdout.includes(`skip ${name}`) || stdout.includes(`pending ${name}`);
    const failure = (stdout.match(new RegExp(`FAIL ${name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}: ([^\n]+)`)) || text.match(/Error: ([^\n]+)/) || [])[1];
    const tick = failure ? Number((failure.match(/at (?:tick )?(\d+)/) || [])[1]) || null : null;
    // The comparer's own report (kept when the gate fails): the first tick the human's physics words differ.
    const kept = (text.match(/comparer reports kept in (\S+)/) || [])[1];
    let physicsThrough = null;
    for (const ext of ['regression.json', 'ai-regression.json']) {
      const file = kept && path.join(kept, `${name}.${ext}`);
      if (file && fs.existsSync(file)) {
        const report = JSON.parse(fs.readFileSync(file, 'utf8'));
        const first = report.summary?.firstInexact?.tick ?? report.firstInexact?.tick ?? report.rows?.find((row) => row.humanExact === false)?.tick;
        physicsThrough = first == null ? 'end' : first - 1;
      }
    }
    if (kept) fs.rmSync(kept, { recursive: true, force: true });
    resolve({ name, status: error ? 'fail' : skipped ? 'skip' : 'pass', tick, physicsThrough, message: failure || '' });
  });
});

const results = [];
let next = 0;
const worker = async () => {
  while (next < names.length) {
    const name = names[next++];
    const result = await score(name);
    results.push(result);
    console.log(`${result.status.padEnd(4)} ${name}${result.physicsThrough != null ? ` physics through ${result.physicsThrough}` : ''}${result.message ? `  ${result.message.slice(0, 160)}` : ''}`);
  }
};
await Promise.all(Array.from({ length: par }, worker));
const areas = {};
for (const result of results) {
  const area = result.name.includes('/') ? result.name.split('/')[0] : 'top';
  areas[area] ??= { pass: 0, fail: 0, skip: 0 };
  areas[area][result.status]++;
}
const total = { pass: 0, fail: 0, skip: 0 };
for (const counts of Object.values(areas)) for (const key of Object.keys(total)) total[key] += counts[key];
console.log(JSON.stringify({ runs, core: core || 'web/runtime', total, areas }, null, 1));
if (out) fs.writeFileSync(out, JSON.stringify({ runs, core, total, areas, results }, null, 1));
