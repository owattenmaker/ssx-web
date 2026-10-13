// The native core's parity tests (docs/native.md "Tests"), run by ctest: a native result against the wasm build's for the same
// script. Exit 0 identical, 1 different or failed, 77 skipped (no game data, no wasm core, no PS2 capture).
//
//   node parity.mjs race [--ai] [--ticks N] [--give-up T] OPTIONS
//     the race driver (native/core/race_driver.cpp) against native/tools/ref-trace.mjs on the wasm core: every tick's hash
//   node parity.mjs gate NAME OPTIONS
//     a PS2 gate's export-call stream (web/compare-ps2-capture.mjs on the wasm core, logged by native/tools/spy-core.js)
//     replayed by native/core/replay_calls.cpp: every probe identical
//   OPTIONS: --wasm-core core.js --assets DIR --runs DIR --cache DIR --race-driver EXE --replay-calls EXE
//
// The wasm results are cached in --cache, keyed by the wasm core's hash and the script. They are game-derived data: the cache
// lives in the build folder, never in the repository.
import { execFileSync, spawnSync } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '../..');
const SKIP = 77;

// The gates this test can replay: the arguments web/test-ps2-captures.mjs run1 passes for that case (keep in step with it).
const GATES = {
  'event-race': { args: ['--zoe', '--event'] }
};

const argv = process.argv.slice(2);
const mode = argv[0];
function option(name, fallback) {
  const k = argv.indexOf(name);
  return k >= 0 && k + 1 < argv.length ? argv[k + 1] : fallback;
}
const wasmCore = path.resolve(option('--wasm-core', path.join(root, 'web/runtime/core.js')));
const assets = path.resolve(option('--assets', path.join(root, 'web/public/assets')));
const runs = path.resolve(option('--runs', path.join(root, 'local/ps2-capture/runs')));
const cache = path.resolve(option('--cache', path.join(root, 'build/native-core/parity-cache')));
const raceDriver = option('--race-driver', '');
const replayCalls = option('--replay-calls', '');

function skip(why) {
  console.log(`skip: ${why}`);
  process.exit(SKIP);
}
function fail(why) {
  console.log(`FAIL: ${why}`);
  process.exit(1);
}
// A -DSSX_NATIVE_UBSAN=ON build reports each float-to-int conversion out of range (NaN, inf, too large) on stderr:
// the values wasm, arm64 and x86-64 convert differently. Any report fails the test.
function checkSanitizer(stderr) {
  const sites = [...new Set((stderr || '').split('\n').filter((l) => l.includes('runtime error:')))];
  if (sites.length) fail(`float-to-int conversions out of range (they differ on x86-64):\n${sites.join('\n')}`);
}
function shortHash(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex').slice(0, 16);
}
function seconds(start) {
  return ((performance.now() - start) / 1000).toFixed(2);
}

const wasmFile = wasmCore.replace(/\.js$/, '.wasm');
if (!fs.existsSync(wasmCore) || !fs.existsSync(wasmFile)) skip(`no wasm core at ${wasmCore} (sh web/build-core.sh)`);
const coreHash = shortHash(wasmFile);
fs.mkdirSync(cache, { recursive: true });
console.log(`wasm reference: ${wasmCore} (core.wasm ${coreHash}); it must be built from the same sources as this native build`);

// One line per tick, "tick hash"
function compareTraces(native, wasm) {
  const a = native.trim().split('\n');
  const b = wasm.trim().split('\n');
  const n = Math.min(a.length, b.length);
  for (let k = 0; k < n; k++) {
    if (a[k] !== b[k]) fail(`tick ${k} differs: native "${a[k]}", wasm "${b[k]}"`);
  }
  if (a.length !== b.length) fail(`native ran ${a.length} ticks, wasm ${b.length}`);
  return n;
}

function race() {
  for (const f of ['ANIMATIONS/animation-packets.json', 'ARA1/terrain.json', 'RIDER_ZOE/rider.json']) {
    if (!fs.existsSync(path.join(assets, f))) skip(`no game data (${f}) in ${assets} (python3 tools/setup_from_iso.py)`);
  }
  const script = [];
  if (argv.includes('--ai')) script.push('--ai');
  for (const name of ['--ticks', '--give-up']) {
    const v = option(name, null);
    if (v !== null) script.push(name, v);
  }
  const key = crypto.createHash('sha256').update(`${coreHash} ${assets} ${script.join(' ')}`).digest('hex').slice(0, 16);
  const refFile = path.join(cache, `race-${key}.txt`);
  let wasmTime = 'cached';
  if (!fs.existsSync(refFile)) {
    const start = performance.now();
    const out = execFileSync(process.execPath, [path.join(root, 'native/tools/ref-trace.mjs'), ...script], {
      env: { ...process.env, CORE_JS: wasmCore, SSX_ASSETS: assets },
      encoding: 'utf8',
      maxBuffer: 1 << 28
    });
    wasmTime = `${seconds(start)} s`;
    fs.writeFileSync(`${refFile}.tmp`, out);
    fs.renameSync(`${refFile}.tmp`, refFile);
  }
  const start = performance.now();
  const r = spawnSync(raceDriver, [assets, ...script], { encoding: 'utf8', maxBuffer: 1 << 28 });
  const nativeTime = seconds(start);
  if (r.status !== 0) fail(`race driver exit ${r.status}: ${r.stderr}`);
  checkSanitizer(r.stderr);
  const ticks = compareTraces(r.stdout, fs.readFileSync(refFile, 'utf8'));
  console.log(`race ${script.join(' ') || '(solo)'}: ${ticks} ticks identical (native ${nativeTime} s, wasm ${wasmTime})`);
}

function gate() {
  const name = argv[1];
  const gateCase = GATES[name];
  if (!gateCase) fail(`unknown gate ${name}`);
  const bin = path.join(runs, `${name}.bin`);
  if (!fs.existsSync(bin) || !fs.existsSync(path.join(runs, `${name}.capture.json`))) skip(`no PS2 capture ${bin}`);
  const stream = path.join(cache, `gate-${name}-${coreHash}.jsonl`);
  let wasmTime = 'cached';
  if (!fs.existsSync(stream)) {
    const start = performance.now();
    const report = path.join(cache, `gate-${name}-${coreHash}.regression.json`);
    const r = spawnSync(process.execPath, ['compare-ps2-capture.mjs', bin, '--pad', '--sync-rng', '--report', report, ...gateCase.args], {
      cwd: path.join(root, 'web'),
      env: {
        ...process.env,
        CORE_JS: path.join(root, 'native/tools/spy-core.js'),
        REAL_CORE: wasmCore,
        SPY_LOG: `${stream}.tmp`,
        BONE_SCAN: '1',
        BONE_SCAN_MAX: '29'
      },
      encoding: 'utf8',
      maxBuffer: 1 << 28
    });
    if (r.status !== 0) fail(`the wasm gate run failed (exit ${r.status}): ${(r.stderr || '').slice(-2000)}`);
    fs.renameSync(`${stream}.tmp`, stream);
    wasmTime = `${seconds(start)} s`;
  }
  const start = performance.now();
  const r = spawnSync(replayCalls, [stream], { encoding: 'utf8', maxBuffer: 1 << 28 });
  const nativeTime = seconds(start);
  if (r.status !== 0) fail(`replay exit ${r.status}: ${r.stdout}${r.stderr}`);
  checkSanitizer(r.stderr);
  console.log(`gate ${name}: ${r.stdout.trim()} (native ${nativeTime} s, wasm log ${wasmTime})`);
}

// The native replay verifier against the browser verifier (docs/native.md "N2"): every <name>.ssxr in --fixtures with its
// <name>.verdict.json (native/tools/browser-verify.mjs: {ok, reason, value}) and, when there, <name>.trace.json (the page's ?simtrace
// hash of every replayed tick). The verdict, the value and every tick's hash must be the browser's.
function verify() {
  const dir = path.resolve(option('--fixtures', path.join(root, 'local/native-port/fixtures')));
  const verifier = option('--verify-replay', '');
  const names = fs.existsSync(dir) ? fs.readdirSync(dir).filter((f) => f.endsWith('.ssxr')).sort() : [];
  if (!names.length) skip(`no replay fixtures in ${dir} (docs/native.md "N2")`);
  let failures = 0;
  for (const f of names) {
    const base = path.join(dir, f.replace(/\.ssxr$/, ''));
    if (!fs.existsSync(`${base}.verdict.json`)) {
      console.log(`${f}: no verdict.json, skipped`);
      continue;
    }
    const want = JSON.parse(fs.readFileSync(`${base}.verdict.json`, 'utf8'));
    const traceOut = path.join(cache, `verify-${path.basename(base)}.trace.json`);
    const start = performance.now();
    const r = spawnSync(verifier, [assets, path.join(dir, f), '--trace', traceOut], { encoding: 'utf8', maxBuffer: 1 << 28 });
    const took = seconds(start);
    checkSanitizer(r.stderr);
    let got = null;
    try {
      got = JSON.parse(r.stdout.trim().split('\n').pop());
    } catch {
      console.log(`${f}: FAIL no result (exit ${r.status}): ${r.stderr}`);
      failures++;
      continue;
    }
    const problems = [];
    for (const k of ['ok', 'reason', 'value']) if (JSON.stringify(got[k] ?? null) !== JSON.stringify(want[k] ?? null)) problems.push(`${k} ${JSON.stringify(got[k])} != ${JSON.stringify(want[k])}`);
    let ticks = '';
    if (fs.existsSync(`${base}.trace.json`)) {
      const a = JSON.parse(fs.readFileSync(traceOut, 'utf8'));
      const b = JSON.parse(fs.readFileSync(`${base}.trace.json`, 'utf8'));
      const n = Math.min(a.length, b.length);
      let first = -1;
      for (let k = 0; k < n && first < 0; k++) if (a[k] !== b[k]) first = k;
      if (first >= 0) problems.push(`tick ${first} differs`);
      else if (a.length !== b.length) problems.push(`native ${a.length} ticks, page ${b.length}`);
      ticks = `, ${n} ticks identical`;
    }
    if (problems.length) {
      failures++;
      console.log(`${f}: FAIL ${problems.join('; ')}`);
    } else console.log(`${f}: ${got.reason} (value ${got.value}${ticks}; native ${took} s, browser ${want.simMs ?? '?'} ms)`);
  }
  if (failures) fail(`${failures} replay(s) differ from the browser verifier`);
}

if (mode === 'race') race();
else if (mode === 'gate') gate();
else if (mode === 'verify') verify();
else fail(`unknown mode ${mode}`);
