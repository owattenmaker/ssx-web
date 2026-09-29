// Test / core-build lock for this working tree. Several agents share it, and their test runs and core rebuilds collided:
// load spikes that stalled everyone, and tests that loaded one core build's core.js with the next one's core.wasm.
//   - core build (web/build-core.sh):       exclusive; waits for running tests, blocks new ones while it waits
//   - any test run (npm test, filtered or not): shared with other test runs, never during a core build
//   - full test run (npm test, no filter):  additionally one at a time; a waiter whose request predates the start of the
//                                           run that just finished reuses its results (web/run-tests.mjs)
// State lives in node_modules/.cache/ssx-tests/locks/ (mkdir-atomic; entries of dead processes are cleared).
//   node test-lock.mjs build -- CMD ARGS...   run CMD holding the build lock (build-core.sh does this itself)
//   node test-lock.mjs status                 who holds or waits for what
//   node test-lock.mjs wait LOG [SECONDS]     block up to SECONDS (300) for a background `npm test > LOG` to finish
import fs from 'node:fs'; import path from 'node:path'; import { spawn } from 'node:child_process';
const dir = new URL('node_modules/.cache/ssx-tests/locks/', import.meta.url).pathname;
const buildLock = dir + 'build', fullLock = dir + 'full', readers = dir + 'readers/';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const alive = (pid) => { if (!(pid > 0)) return false; try { process.kill(pid, 0); return true; } catch (e) { return e.code === 'EPERM'; } };
const owner = (lock) => { try { return JSON.parse(fs.readFileSync(lock + '/owner.json', 'utf8')); } catch { return null; } };
const age = (p) => { try { return Date.now() - fs.statSync(p).mtimeMs; } catch { return 0; } };
const minutes = (ms) => `${Math.round(ms / 60000)} min`;

// An exclusive lock directory; cleared when its owner process is gone (or it never got an owner within 10 s).
async function takeDir(lock, label, onWait) {
  fs.mkdirSync(dir + 'readers', { recursive: true });
  for (let n = 0; ; n++) {
    try { fs.mkdirSync(lock); fs.writeFileSync(lock + '/owner.json', JSON.stringify({ pid: process.pid, label, since: Date.now() })); break; } catch (e) { if (e.code !== 'EEXIST') throw e; }
    const o = owner(lock);
    if (o ? !alive(o.pid) : age(lock) > 10000) { fs.rmSync(lock, { recursive: true, force: true }); continue; }
    if (n % 15 === 0) onWait?.(o);
    await sleep(2000);
  }
  const release = () => { if (owner(lock)?.pid === process.pid) fs.rmSync(lock, { recursive: true, force: true }); };
  process.on('exit', release); return release;
}
const liveReaders = (skipLoop = false) => { let n = 0; for (const f of fs.readdirSync(readers)) { const pid = +f; if (!alive(pid)) { fs.rmSync(readers + f, { force: true }); continue; } if (skipLoop) { try { if (fs.readFileSync(readers + f, 'utf8').includes('(loop)')) continue; } catch {} } n++; } return n; };

/** Exclusive: waits for the tests that are running, and blocks new ones while it waits. The continuous loop's runs
 * (web/test-loop.mjs, label '(loop)') are not waited for: the core changes under them, so they end INVALID and are repeated. */
export async function acquireBuild(label = 'core build') {
  const release = await takeDir(buildLock, label, (o) => console.error(`[test-lock] ${label}: waiting for ${o?.label ?? 'another core build'} (pid ${o?.pid})`));
  for (let n = 0; liveReaders(true); n++) { if (n % 15 === 0) console.error(`[test-lock] ${label}: waiting for ${liveReaders(true)} running test run(s) to finish`); await sleep(2000); }
  return release;
}

/** Shared: any number of test runs together, none during a core build. */
export async function acquireTests(label = 'tests') {
  fs.mkdirSync(readers, { recursive: true });
  const mine = readers + process.pid;
  for (let n = 0; ; n++) {
    const o = owner(buildLock);
    if (fs.existsSync(buildLock) && (o ? !alive(o.pid) : age(buildLock) > 10000)) fs.rmSync(buildLock, { recursive: true, force: true }); // left by a dead build
    if (!fs.existsSync(buildLock)) {
      fs.writeFileSync(mine, label);
      if (!fs.existsSync(buildLock)) break; // a build started in between: step back and wait for it
      fs.rmSync(mine, { force: true });
    }
    if (n % 15 === 0) console.error(`[test-lock] ${label}: waiting for the core build (pid ${owner(buildLock)?.pid}) to finish`);
    await sleep(2000);
  }
  const release = () => fs.rmSync(mine, { force: true });
  process.on('exit', release); return release;
}

/** One full run at a time. */
export const acquireFull = (label) => takeDir(fullLock, label, (o) => console.error(`[test-lock] waiting for the full test run by pid ${o?.pid} (running ${minutes(Date.now() - (o?.since ?? Date.now()))}); its results are reused if it started after this request`));

if (import.meta.url === `file://${process.argv[1]}`) {
  const [cmd, ...rest] = process.argv.slice(2);
  if (cmd === 'status') {
    fs.mkdirSync(readers, { recursive: true });
    const show = (name, lock) => { const o = owner(lock); console.log(`${name}: ${o ? `pid ${o.pid} ${o.label}, ${minutes(Date.now() - o.since)}${alive(o.pid) ? '' : ' (dead)'}` : 'free'}`); };
    show('core build', buildLock); show('full test run', fullLock);
    console.log(`test runs: ${liveReaders()} [${fs.readdirSync(readers).map((f) => `${f} ${fs.readFileSync(readers + f, 'utf8')}`).join(', ')}]`);
  } else if (cmd === 'wait' && rest[0]) {
    // Agents: an agent that ends its turn to wait for a background `npm test` is stopped by the 10-minute stall watchdog
    // (a queued full run can take longer). Instead: `npm test > LOG 2>&1` in the background, then call this repeatedly;
    // each call blocks at most SECONDS (default 300) and exits 0 once LOG has the final summary, 3 while it runs.
    const [log, seconds = '300'] = rest, until = Date.now() + Number(seconds) * 1000, doneRe = /passed in \d+s|INVALID RUN/;
    const read = () => { try { return fs.readFileSync(log, 'utf8'); } catch { return ''; } };
    while (!doneRe.test(read()) && Date.now() < until) await sleep(5000);
    const text = read().trim().split('\n'), finished = doneRe.test(text.join('\n'));
    console.log(finished ? text.filter((l) => /^FAIL|passed in|INVALID|Reusing/.test(l)).join('\n') || text.slice(-3).join('\n')
      : `still running (${text.filter((l) => /^(ok|FAIL) /.test(l)).length} commands done so far${/waiting for/.test(text.join('\n')) ? '; queued behind another run' : ''}); call wait again`);
    process.exit(finished ? 0 : 3);
  } else if (cmd === 'build' && rest[0] === '--' && rest.length > 1) {
    const release = await acquireBuild('core build');
    // build-core.sh re-runs itself here as "$0", which may be relative ("build-core.sh"): run a script file through sh by
    // its absolute path (spawn('build-core.sh') looks it up on PATH and fails with ENOENT).
    const script = fs.existsSync(rest[1]) ? path.resolve(rest[1]) : null;
    const p = script ? spawn('sh', [script, ...rest.slice(2)], { stdio: 'inherit', env: { ...process.env, SSX_BUILD_LOCK_HELD: '1' } })
      : spawn(rest[1], rest.slice(2), { stdio: 'inherit', env: { ...process.env, SSX_BUILD_LOCK_HELD: '1' } });
    for (const s of ['SIGINT', 'SIGTERM']) process.on(s, () => p.kill(s));
    p.on('close', (code, signal) => { release(); process.exit(code ?? (signal ? 1 : 0)); });
  } else { console.error('usage: node test-lock.mjs status | wait LOG [SECONDS] | build -- CMD ARGS...'); process.exit(2); }
}
