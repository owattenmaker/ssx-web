// Continuous testing: runs the full npm test (run-tests.mjs, niced) whenever the tree changed since the last run started,
// at most one run start per TEST_LOOP_GAP_MIN minutes (default 10), so agents never wait on a full run: they run the
// targeted tests of their change (`npm test -- word ...`) and move on; this loop catches the rest.
//   node test-loop.mjs            the loop (one per machine; a second one exits)
//   node test-loop.mjs status     the latest run and the current failure set
//   node test-loop.mjs watch      blocks until the next run with a NEW failure (or a loop error), prints it, exits
// Each finished run appends one line to node_modules/.cache/ssx-tests/loop/events.log:
//   <time> RUN 166/168 in 412s | NEW: a, b | STILL: c | FIXED: d | FLAKY: e
// A failure is rerun once on its own: passing then = FLAKY (reported, not NEW). The known baseline test-slopestyle-bigair
// failure counts as passing only with its usual signature (griff score+0x164 at tick 576, RNG at 821).
import fs from 'node:fs'; import path from 'node:path'; import { spawn } from 'node:child_process';
const web = new URL('.', import.meta.url).pathname, cache = web + 'node_modules/.cache/ssx-tests/', dir = cache + 'loop/';
const events = dir + 'events.log', latestPath = dir + 'latest.json', pidPath = dir + 'pid';
const GAP_MS = (Number(process.env.TEST_LOOP_GAP_MIN) || 10) * 60e3, POLL_MS = 30e3;
fs.mkdirSync(dir, { recursive: true });
const clock = () => new Date().toLocaleTimeString();
const alive = (pid) => { try { process.kill(pid, 0); return true; } catch { return false; } };
const readJson = (p) => { try { return JSON.parse(fs.readFileSync(p, 'utf8')); } catch { return null; } };
const append = (line) => { fs.appendFileSync(events, `${new Date().toISOString()} ${line}\n`); console.log(line); };

const mode = process.argv[2];
if (mode === 'status') {
  const l = readJson(latestPath); const pid = +(() => { try { return fs.readFileSync(pidPath, 'utf8'); } catch { return '0'; } })().trim() || 0;
  console.log(`loop: ${pid && alive(pid) ? `running (pid ${pid})` : 'not running'}`);
  console.log(l ? `latest: ${l.summary}\nfailing: ${l.failing.join(', ') || 'none'}` : 'no run yet');
  process.exit(0);
}
if (mode === 'watch') {
  let size = fs.existsSync(events) ? fs.statSync(events).size : 0;
  for (;;) {
    await new Promise((r) => setTimeout(r, 5000));
    const now = fs.existsSync(events) ? fs.statSync(events).size : 0; if (now <= size) continue;
    const fd = fs.openSync(events, 'r'), buf = Buffer.alloc(now - size); fs.readSync(fd, buf, 0, buf.length, size); fs.closeSync(fd); size = now;
    const hits = buf.toString().split('\n').filter((l) => / NEW: |LOOP-ERROR/.test(l));
    if (hits.length) { console.log(hits.join('\n')); process.exit(0); }
  }
}

// ---- the loop ----
const prior = +(() => { try { return fs.readFileSync(pidPath, 'utf8'); } catch { return '0'; } })().trim();
if (prior && prior !== process.pid && alive(prior)) { console.log(`test loop already running (pid ${prior})`); process.exit(0); }
fs.writeFileSync(pidPath, String(process.pid));

// Tree fingerprint: newest mtime and file count of the sources, tests, tools' outputs the tests read, and the served assets.
const SKIP = new Set(['node_modules', '.git', '.vite']);
function fingerprint() {
  let newest = 0, count = 0;
  const walk = (d) => { let ents; try { ents = fs.readdirSync(d, { withFileTypes: true }); } catch { return; }
    for (const e of ents) { if (SKIP.has(e.name) || e.name.startsWith('dist')) continue; const p = path.join(d, e.name);
      if (e.isDirectory()) walk(p); else { try { const m = fs.statSync(p).mtimeMs; count++; if (m > newest) newest = m; } catch {} } } };
  walk(web); return `${Math.round(newest)}:${count}`;
}
const run = (args) => new Promise((resolve) => {
  const p = spawn('nice', ['-n', '10', process.execPath, 'run-tests.mjs', ...args], { cwd: web, stdio: ['ignore', 'pipe', 'pipe'], env: { ...process.env, TEST_LOOP: '1' } });
  let out = ''; p.stdout.on('data', (d) => { out += d; }); p.stderr.on('data', (d) => { out += d; });
  p.on('close', (code) => resolve({ code, out }));
});
const failuresOf = (out) => [...out.matchAll(/^FAIL\s+\d+\/\d+\s+[\d.]+s\s+(node [^\n(]+?)\s+\(exit/gm)].map((m) => m[1].trim());
const knownBaseline = () => {
  const log = (() => { try { return fs.readFileSync(cache + 'logs/node_test-slopestyle-bigair.mjs.log', 'utf8'); } catch { return ''; } })();
  return /ass1-griff-87e9ff58/.test(log) && /"first":\{"tick":576,"key":"score\+0x164"/.test(log) && /"firstRngMismatch":\{"tick":821\}/.test(log);
};

let lastStart = 0, lastPrint = '';
for (;;) {
  try {
    const fp = fingerprint();
    if (fp === lastPrint || Date.now() - lastStart < GAP_MS) { await new Promise((r) => setTimeout(r, POLL_MS)); continue; }
    lastStart = Date.now(); lastPrint = fp;
    const t0 = Date.now(), log = dir + `run-${new Date(t0).toISOString().replace(/[:.]/g, '-')}.log`;
    const { out } = await run([]); fs.writeFileSync(log, out);
    if (/INVALID RUN/.test(out)) { append(`RUN invalid (core rebuilt mid-run), again next round | log ${log}`); lastPrint = ''; lastStart = 0; continue; }
    const summary = (out.match(/^(\d+)\/(\d+) passed in (\d+)s/m) || []);
    let failed = failuresOf(out);
    if (!summary.length) { append(`LOOP-ERROR run produced no summary | log ${log}`); continue; }
    const baselineOk = failed.includes('node test-slopestyle-bigair.mjs') && knownBaseline();
    if (baselineOk) failed = failed.filter((c) => c !== 'node test-slopestyle-bigair.mjs');
    // Rerun each failure once on its own: a pass then is a flake (load / timing), still reported.
    const flaky = [], real = [];
    for (const c of failed) { const word = c.replace(/^node /, ''); const r = await run([word]); (r.code === 0 ? flaky : real).push(c);
      if (r.code !== 0 && c === 'node test-slopestyle-bigair.mjs' && knownBaseline()) real.pop(); }
    const prev = readJson(latestPath)?.failing ?? [];
    const name = (c) => c.replace(/^node /, '');
    const NEW = real.filter((c) => !prev.includes(c)), STILL = real.filter((c) => prev.includes(c)), FIXED = prev.filter((c) => !real.includes(c));
    const line = `RUN ${summary[1]}/${summary[2]} in ${summary[3]}s`
      + (NEW.length ? ` | NEW: ${NEW.map(name).join(', ')}` : '') + (STILL.length ? ` | STILL: ${STILL.map(name).join(', ')}` : '')
      + (FIXED.length ? ` | FIXED: ${FIXED.map(name).join(', ')}` : '') + (flaky.length ? ` | FLAKY: ${flaky.map(name).join(', ')}` : '')
      + (baselineOk ? ' | baseline slopestyle as known' : '') + ` | log ${log}`;
    fs.writeFileSync(latestPath, JSON.stringify({ start: t0, end: Date.now(), summary: line, failing: real, flaky, log }, null, 1));
    append(line);
    // Keep the last 30 run logs.
    const runs = fs.readdirSync(dir).filter((f) => f.startsWith('run-')).sort(); for (const f of runs.slice(0, -30)) fs.rmSync(dir + f, { force: true });
  } catch (e) { append(`LOOP-ERROR ${e?.stack || e}`); await new Promise((r) => setTimeout(r, POLL_MS)); }
}
