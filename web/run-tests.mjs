// npm test: every command of package.json "test:all" (split on &&; add new tests there), run in parallel lanes and
// continuing past failures. Longest first (timings of the previous run); the online tests that bind a local port run one
// after another in their own lane. Exit 1 if any command failed; each failure's log tail is printed at the end.
//   npm test                   all commands, up to 6 lanes, fewer when the machine is busy (see `free` below)
//   npm test -- stage peak     only commands matching any of the words
//   TEST_LANES=1 npm test      one at a time, in package.json order (what `npm run test:all` does, minus the stop at the first failure)
// Shared tree (web/test-lock.mjs): runs never overlap a core build; full runs go one at a time, and a full run requested
// while another is going waits and then reuses the next run that starts after the request (its edits are in it), so any
// number of agents asking at once costs at most two runs. Agents: run it in the background and wait for the result.
// Logs and timings: node_modules/.cache/ssx-tests/.
import fs from 'node:fs'; import os from 'node:os'; import { spawn } from 'node:child_process';
import { acquireFull, acquireTests } from './test-lock.mjs';
const web = new URL('.', import.meta.url).pathname, cache = web + 'node_modules/.cache/ssx-tests/', lastFullPath = cache + 'last-full.json';
const words = process.argv.slice(2), full = !words.length, requested = Date.now();
const lines = [], say = (s) => { console.log(s); lines.push(s); };
const clock = (ms) => new Date(ms).toLocaleTimeString();
fs.mkdirSync(cache + 'logs', { recursive: true });

if (full) {
  await acquireFull(`npm test (requested ${clock(requested)})`);
  // Someone else's full run started after this request, so it already tested this request's edits: reuse it.
  const last = (() => { try { return JSON.parse(fs.readFileSync(lastFullPath, 'utf8')); } catch { return null; } })();
  if (last && last.start >= requested && !last.invalid) {
    console.log(`Reusing the full run that started at ${clock(last.start)}, after this request (${clock(requested)}):\n`);
    console.log(last.lines.join('\n')); process.exit(last.code);
  }
}
// TEST_LOOP (web/test-loop.mjs): a core build doesn't wait for this run; the run is then reported INVALID and repeated.
await acquireTests(full ? (process.env.TEST_LOOP ? 'npm test (full) (loop)' : 'npm test (full)') : `npm test -- ${words.join(' ')}`);

// macOS: with the display asleep, headless Chrome never fires requestAnimationFrame, so the browser tests hang until
// their timeouts. Wake the display and keep it on while this run lives (a power assertion, no settings change).
if (process.platform === 'darwin') { try { spawn('caffeinate', ['-d', '-u', '-w', String(process.pid)], { stdio: 'ignore', detached: true }).unref(); } catch {} }
const all = JSON.parse(fs.readFileSync(web + 'package.json', 'utf8')).scripts['test:all'].split('&&').map((s) => s.trim()).filter(Boolean);
const cmds = full ? all : all.filter((c) => words.some((w) => c.includes(w)));
// Lanes and test-ps2-captures' own PAR scale with the cores that are free right now (load average): several agents
// running the suite at once on one machine starved each other at a load of ~100 on 18 cores.
const cores = os.availableParallelism?.() ?? os.cpus().length, free = Math.max(1, cores - os.loadavg()[0]);
const lanes = Math.max(1, Number(process.env.TEST_LANES) || Math.min(6, Math.max(1, Math.floor(free / 3))));
const env = { ...process.env, PAR: process.env.PAR || String(Math.min(12, Math.max(2, Math.floor(free / 2)))) };
const timesPath = cache + 'times.json', times = (() => { try { return JSON.parse(fs.readFileSync(timesPath, 'utf8')); } catch { return {}; } })();
const ports = /test-mp\.mjs|test-mp-gate\.mjs|test-precompress\.mjs|test-mp-plausibility\.mjs/; // bind 18000-19999 at random
const byTime = (a, b) => (times[b] ?? 5) - (times[a] ?? 5);
const serialQ = lanes > 1 ? cmds.filter((c) => ports.test(c)).sort(byTime) : [];
const mainQ = lanes > 1 ? cmds.filter((c) => !ports.test(c)).sort(byTime) : [...cmds];
const failed = [], t0 = Date.now(); let done = 0;
// The lock keeps core builds out, but a build outside it (an old build-core.sh, a manual em++) would still mix two
// cores (a test loading the new core.js with the old core.wasm crashes out of bounds): such a run is invalid.
const coreStamp = () => { try { return fs.statSync(web + 'runtime/core.wasm').mtimeMs; } catch { return 0; } }, core0 = coreStamp();
// A hung test (a browser that never answers) would stall the run for good (test-rider-prefetch once held the loop 5 h):
// each test gets TEST_TIMEOUT_MIN minutes (default 15; the slowest takes ~1.5), then its process group is stopped
// (SIGTERM so browser drivers close their browsers, SIGKILL 10 s later) and it fails as a timeout.
const TIMEOUT_MS = (Number(process.env.TEST_TIMEOUT_MIN) || 15) * 60e3, live = new Set();
const stopGroup = (p) => { try { process.kill(-p.pid, 'SIGTERM'); } catch {} setTimeout(() => { try { process.kill(-p.pid, 'SIGKILL'); } catch {} }, 10e3).unref(); };
for (const sig of ['SIGINT', 'SIGTERM', 'SIGHUP']) process.once(sig, () => { for (const p of live) stopGroup(p); setTimeout(() => process.exit(130), 200); });
const run = (c) => new Promise((resolve) => {
  const start = Date.now(), log = cache + 'logs/' + c.replace(/[^a-z0-9.-]+/gi, '_') + '.log', out = fs.openSync(log, 'w');
  const p = spawn('sh', ['-c', c], { cwd: web, env, stdio: ['ignore', out, out], detached: true }); live.add(p);
  let timedOut = false; const timer = setTimeout(() => { timedOut = true; stopGroup(p); }, TIMEOUT_MS);
  p.on('close', (code, signal) => {
    clearTimeout(timer); live.delete(p); if (timedOut) { fs.writeSync(out, `\nTIMEOUT: stopped after ${TIMEOUT_MS / 60e3} min\n`); code = null; signal = 'timeout'; }
    fs.closeSync(out); const s = (Date.now() - start) / 1000; times[c] = +s.toFixed(1); done++;
    const ok = code === 0; if (!ok) failed.push({ c, code, signal, log });
    say(`${ok ? 'ok  ' : 'FAIL'} ${String(done).padStart(3)}/${cmds.length} ${s.toFixed(1).padStart(6)}s  ${c}${ok ? '' : `  (exit ${code ?? signal})`}`);
    resolve();
  });
});
const lane = async (q) => { while (q.length) await run(q.shift()); };
say(`${cmds.length} test commands, ${lanes} lane${lanes > 1 ? 's' : ''}, PAR=${env.PAR} (load ${os.loadavg()[0].toFixed(0)} on ${cores} cores), started ${clock(t0)}`);
await Promise.all([lane(serialQ), ...Array.from({ length: lanes > 1 ? lanes - 1 : 1 }, () => lane(mainQ))]);
fs.writeFileSync(timesPath, JSON.stringify(times, null, 1));
for (const f of failed) say(`\n--- FAIL ${f.c} (exit ${f.code ?? f.signal}), ${f.log}\n` + fs.readFileSync(f.log, 'utf8').trim().split('\n').slice(-25).join('\n'));
say(`\n${cmds.length - failed.length}/${cmds.length} passed in ${((Date.now() - t0) / 1000).toFixed(0)}s${failed.length ? `; failed: ${failed.map((f) => f.c.replace(/^node /, '')).join(', ')}` : ''}`);
const invalid = coreStamp() !== core0, code = failed.length || invalid ? 1 : 0;
if (invalid) say(`\nINVALID RUN: web/runtime/core.wasm was rebuilt during the run (${clock(coreStamp())}); results mix two cores, run again.`);
if (full) fs.writeFileSync(lastFullPath, JSON.stringify({ start: t0, end: Date.now(), code, invalid, lines }));
process.exit(code);
