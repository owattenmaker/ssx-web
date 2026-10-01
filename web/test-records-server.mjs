// Online course records server (web/server/records.mjs, docs/online-records.md): the boards seeded with the defaults, submit /
// rank / one entry per name, bad input, the size and rate limits, persistence across a restart, the replay download, the verifier
// pulling an entry, and the HTTP mount in mp-server.mjs (off without MP_RECORDS_DIR, behind the password gate). Synthetic tables
// only (no game data needed).
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { createRecords, cleanName, MAX_ENTRIES } from './server/records.mjs';
import { encodeReplayFile, decodeReplayFile, padStreamInfo } from './server/replay-file.mjs';
import { createRecording } from './replay.js';

const web = path.dirname(fileURLToPath(import.meta.url));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-records-'));
// synthetic disc tables: course 0 = race 'TST1' (slot 12), course 5 = slope style 'TSS1' (slot 17), course 14 = backcountry 'TBC1'
const courses = Array.from({ length: 17 }, (_, i) => ({ index: i, code: i === 0 ? 'TST1' : i === 5 ? 'TSS1' : i === 14 ? 'TBC1' : `X${i}`, name: `c${i}` }));
const recordSlots = Array.from({ length: 17 }, (_, i) => (i === 0 ? [0, 12, 14, 26] : i === 5 ? [1, 17, 14, 26] : i === 14 ? [4, 6, 5, 7] : [9, 26, 14, 26]));
const records = Array.from({ length: 26 }, (_, s) => Array.from({ length: 5 }, (_, k) => ({ value: s === 17 ? 50000 - 10000 * k : 170 + 10 * k, character: k, name: `DEF${s}X${k}` })));
const assets = path.join(tmp, 'assets'); fs.mkdirSync(path.join(assets, 'CAREER'), { recursive: true }); fs.mkdirSync(path.join(assets, 'TST1'));
fs.writeFileSync(path.join(assets, 'CAREER', 'career.json'), JSON.stringify({ rules: { record_slots: recordSlots, records }, courses }));
fs.writeFileSync(path.join(assets, 'TST1', 'npc-riders.json'), JSON.stringify({ riders: [{ progress_origin: 400000 }] }));   // floor 100 s = 6000 ticks

function upload({ event = '0:TST1', mode = 0, course = 'TST1', name = 'OWEN', ticks = 10000, score, finishTick, pad = null, extra = {} } = {}) {
  const rec = createRecording(), p = new Float32Array(24); const total = (finishTick ?? (ticks + 179)) + 1;
  for (let t = 0; t < total; t++) { p[0] = (t >> 6) & 1; p[4] = Math.fround(((t * 7) % 256) / 255); rec.push(p); }
  const meta = { event, mode, course, round: 3, name, character: 4, rider: { id: 'zoe' }, claim: score != null ? { score } : { ticks }, ticks: total, finishTick: total - 1, events: [], core: 'abcdef0123456789', build: 'test', ...extra };
  return zlib.deflateRawSync(encodeReplayFile({ meta, pad: pad ?? rec.exportBytes() }));
}

// ---- pure helpers ----
assert.equal(cleanName('  Owen  '), 'Owen'); assert.equal(cleanName('PLAYER 1'), 'PLAYER 1'); assert.equal(cleanName('A!@#$%^&'), 'A!@#$%^&');
for (const bad of ['', '   ', 'NINECHARS', 'a<b', 'x:y', 'a"b', 'é', 'fuck', 'SH1T'.replace('1', 'I'), 42, null]) assert.equal(cleanName(bad), null, String(bad));
{ const rec = createRecording(), p = new Float32Array(24); for (let t = 0; t < 300; t++) { p[1] = t >= 100 ? 1 : 0; p[6] = t < 200 ? 0.5 : Math.fround(1 / 3); rec.push(p); }
  const bytes = rec.exportBytes(), info = padStreamInfo(bytes); assert.equal(info.lastTick, 200); assert.equal(info.records, 3);
  const back = createRecording(); back.importBytes(bytes, 300, [{ tick: 5, kind: 'camera', value: 2 }]); const a = new Float32Array(24), b = new Float32Array(24), ca = {}, cb = {};
  for (let t = 0; t < 300; t++) assert.deepEqual(rec.pad(t, a, ca), back.pad(t, b, cb), `pad ${t}`);
  assert.equal(back.ticks, 300); assert.equal(back.eventsAt(5).length, 1);
  const f = decodeReplayFile(encodeReplayFile({ meta: { a: 1 }, pad: bytes })); assert.equal(f.meta.a, 1); assert.deepEqual([...f.pad], [...bytes]);
  assert.equal(padStreamInfo(new Uint8Array([5, 0, 0, 0, 0, 0, 0])), null, 'empty mask'); assert.equal(padStreamInfo(new Uint8Array([5, 1, 0])), null, 'truncated'); }

// ---- in process ----
const dir = path.join(tmp, 'state', 'records');   // missing: created
let clock = Date.UTC(2026, 8, 30, 12);
const make = () => createRecords({ dir, assets, now: () => clock, log: { warn() {} } });
let R = make();
assert.ok(R.enabled); assert.ok(fs.existsSync(path.join(dir, 'replays')));
assert.deepEqual([...R.events.keys()].sort(), ['0:TST1', '1:TSS1', '4:TBC1', '5:TBC1']);
let sum = R.summary().events;
assert.deepEqual(sum['0:TST1'].top.map((r) => [r.name, r.value, r.default, r.replay]), [0, 1, 2, 3, 4].map((k) => [`DEF12X${k}`, (170 + 10 * k) * 60, true, false]));
assert.equal(sum['1:TSS1'].timed, false); assert.equal(sum['1:TSS1'].top[0].value, 50000);
const ok = (r) => { assert.equal(r.status, 200, JSON.stringify(r.body)); return r.body; };
const bad = (r, status, error) => { assert.equal(r.status, status, JSON.stringify(r.body)); assert.equal(r.body.error, error); };
// a run of 10175 ticks (2:49.58) ranks above DEF12X0 (170 s = 10200 ticks)
let b = ok(R.submit(upload({ ticks: 10175 }), 'a')); assert.equal(b.rank, 0); assert.equal(b.kept, true); assert.equal(b.top[0].name, 'OWEN'); assert.equal(b.top[5], undefined);
const firstId = b.id; assert.ok(R.replayPath(firstId));
// a tie with a default: the default (earlier) ranks first
b = ok(R.submit(upload({ name: 'TIE', ticks: 10800 }), 'b')); assert.equal(b.rank, 3); assert.equal(b.top[2].name, 'DEF12X1');
// the same name (any case) is kept only when better
b = ok(R.submit(upload({ name: 'owen', ticks: 10190 }), 'c')); assert.equal(b.kept, false); assert.equal(b.id, firstId);
b = ok(R.submit(upload({ name: 'Owen', ticks: 10100 }), 'c')); assert.equal(b.rank, 0); assert.notEqual(b.id, firstId); assert.equal(R.replayPath(firstId), null, 'old replay dropped');
assert.equal(R.board('0:TST1').rows.filter((r) => r.name.toLowerCase() === 'owen').length, 1);
// default names never count for the one-entry rule
b = ok(R.submit(upload({ name: 'DEF12X0', ticks: 13000 }), 'd')); assert.equal(R.board('0:TST1').rows.filter((r) => r.name === 'DEF12X0').length, 2);
// score events rank high first; equal score: the earlier submission first
b = ok(R.submit(upload({ event: '1:TSS1', mode: 1, course: 'TSS1', name: 'P1', score: 45000, finishTick: 4000 }), 'e')); assert.equal(b.rank, 1);
b = ok(R.submit(upload({ event: '1:TSS1', mode: 1, course: 'TSS1', name: 'P2', score: 45000, finishTick: 4000 }), 'f')); assert.equal(b.rank, 2);
// bad input
bad(R.submit(upload({ event: '2:TST1', mode: 2 }), 'g'), 400, 'event');
bad(R.submit(upload({ event: '0:TST1', course: 'TSS1' }), 'g'), 400, 'event');
bad(R.submit(upload({ name: 'BAD<NAME' }), 'h'), 400, 'name');
{ const b = ok(R.submit(upload({ name: 'FAST', ticks: 5000 }), 'i')); assert.equal(b.review, true); assert.equal(b.kept, false);   // under the route floor (100 s): flagged
  assert.ok(!R.board('0:TST1').rows.some((r) => r.name === 'FAST'), 'a flagged run is not listed'); }
bad(R.submit(upload({ ticks: 9000, finishTick: 9999 }), 'i'), 400, 'claim');        // the race clock does not match the recorded ticks
bad(R.submit(upload({ extra: { giveUp: true } }), 'j'), 400, 'claim');
bad(R.submit(upload({ extra: { finishTick: 5 } }), 'j'), 400, 'replay');
bad(R.submit(upload({ extra: { character: 12 } }), 'k'), 400, 'rider');
bad(R.submit(upload({ extra: { core: 'x' } }), 'k'), 400, 'core');
bad(R.submit(upload({ pad: new Uint8Array([1, 2, 3]) }), 'l'), 400, 'replay');
bad(R.submit(Buffer.from('not deflate at all'), 'l'), 400, 'replay');
bad(R.submit(zlib.deflateRawSync(Buffer.alloc(5 * 1048576)), 'l'), 400, 'replay');   // inflates past the limit
bad(R.submit(Buffer.alloc(600 * 1024), 'm'), 413, 'size');
// rate limit: 6 a minute per address, then 30 an hour
for (let i = 0; i < 6; i++) R.submit(upload({ name: 'R' + i, ticks: 12000 + i }), 'rate');
bad(R.submit(upload({ name: 'R7', ticks: 12100 }), 'rate'), 429, 'rate');
ok(R.submit(upload({ name: 'R8', ticks: 12101 }), 'other'));
clock += 61e3; ok(R.submit(upload({ name: 'R9', ticks: 12102 }), 'rate'));
// at most MAX_ENTRIES real runs; the slowest drop with their replays
for (let i = 0; i < MAX_ENTRIES + 5; i++) { clock += 700e3; R.submit(upload({ name: 'M' + i, ticks: 14000 + i }), 'many' + i); }
const full = R.board('0:TST1', 0, 200); assert.equal(full.total, MAX_ENTRIES + 5); assert.equal(full.rows.length, 100);
assert.equal(fs.readdirSync(path.join(dir, 'replays')).length, MAX_ENTRIES + 3, 'replays of the kept runs (+2 slope style, +1 flagged)');
// persistence across a restart
const before = JSON.stringify(R.summary()); R = make(); assert.equal(JSON.stringify(R.summary()), before);
// the verifier pulls a failed entry
const top = R.summary().events['0:TST1'].top[0]; assert.equal(top.name, 'Owen'); assert.equal(top.verified, null);
assert.ok(R.verify(top.id, false)); assert.notEqual(R.summary().events['0:TST1'].top[0].name, 'Owen');
// ---- the anti-cheat floor, the admin CLI, the verifier's rules (fresh state; record-floors.json style floors) ----
{
  const d2 = path.join(tmp, 'floors'), floorsDoc = { events: { '0:TST1': { floorTicks: 7000 } } };
  let F = createRecords({ dir: d2, assets, now: () => clock, log: { warn() {} }, floors: floorsDoc });
  assert.deepEqual(F.floors.get('0:TST1'), { ticks: 7000, source: 'speedrun.com' }, 'the record floor wins over the route floor');
  assert.equal(createRecords({ dir: path.join(tmp, 'f2'), assets, log: { warn() {} }, floors: null }).floors.get('0:TST1').source, 'route');
  const under = ok(F.submit(upload({ name: 'CHEAT', ticks: 6999 }), 'u1'));
  assert.equal(under.review, true); assert.equal(under.rank, -1);
  const at = ok(F.submit(upload({ name: 'EXACT', ticks: 7000 }), 'u2')); assert.equal(at.rank, 0, 'at the floor: listed');
  const above = ok(F.submit(upload({ name: 'ABOVE', ticks: 7300 }), 'u3')); assert.equal(above.rank, 1, 'above the floor: listed');
  const names = () => F.board('0:TST1').rows.map((r) => r.name);
  assert.ok(!names().includes('CHEAT') && !F.summary().events['0:TST1'].top.some((r) => r.name === 'CHEAT'), 'flagged: hidden from board and summary');
  assert.equal(F.stored.flagged.length, 1); assert.equal(F.stored.flagged[0].flagged.reason, 'under-floor');
  assert.ok(F.replayPath(under.id), 'the flagged run keeps its replay (the admin / verifier watch it)');
  const two = ok(F.submit(upload({ name: 'CHEAT2', ticks: 6000 }), 'u4'));
  // the admin CLI on the same directory; the running server sees its edits
  const admin = (...a) => spawnSync(process.execPath, ['server/records-admin.mjs', '--dir', d2, ...a], { cwd: web, encoding: 'utf8' });
  let r = admin('flagged'); assert.equal(r.status, 0); assert.match(r.stdout, /CHEAT .*under-floor \(floor 7000 from speedrun\.com\)/);
  assert.match(admin('show', under.id).stdout, /"name": "CHEAT"/);
  assert.equal(admin('approve', 'ffffffffffffffff').status, 1, 'a bad id');
  assert.equal(admin().status, 2, 'usage');
  r = admin('approve', under.id); assert.equal(r.status, 0); assert.match(r.stdout, /listed/);
  assert.equal(names()[0], 'CHEAT', 'approved: listed (the server read the changed board.json)');
  r = admin('delete', two.id); assert.equal(r.status, 0); assert.equal(F.stored.flagged.length, 0); assert.equal(F.replayPath(two.id), null);
  assert.match(admin('list', '0:TST1').stdout, /1\. .*CHEAT/);
  assert.equal(admin('delete', under.id).status, 0); assert.ok(!names().includes('CHEAT'));
  // the verifier: an own-core mismatch pulls a listed run; another core's run is marked stale, once per core; ok re-stamps it;
  // a flagged run that verifies is listed; one that fails stays flagged
  const own = ok(F.submit(upload({ name: 'OWN', ticks: 7100 }), 'v1')), old = ok(F.submit(upload({ name: 'OLD', ticks: 7200 }), 'v2'));
  const fl = ok(F.submit(upload({ name: 'FL', ticks: 6500 }), 'v3')), fl2 = ok(F.submit(upload({ name: 'FL2', ticks: 6600 }), 'v4'));
  const C = 'abcdef0123456789', NEW = '1111222233334444';
  let q = F.queue(C).map((x) => x.id); assert.deepEqual(q.slice(0, 2), [fl.id, fl2.id], 'flagged runs first');
  assert.ok(q.includes(own.id) && q.includes(old.id));
  assert.equal(F.verify(own.id, { ok: false, reason: 'finish 7300', core: C }).action, 'pulled'); assert.ok(!names().includes('OWN'));
  assert.equal(F.verify(old.id, { ok: false, reason: 'drift', core: NEW }).action, 'stale');
  assert.ok(names().includes('OLD') && F.board('0:TST1').rows.find((x) => x.name === 'OLD').stale, 'an older core\'s run stays listed, stale');
  assert.ok(!F.queue(NEW).some((x) => x.id === old.id), 'one try per core');
  assert.equal(F.verify(fl.id, { ok: true, reason: 'ok', core: C, value: 6500 }).action, 'listed'); assert.equal(names()[0], 'FL');
  assert.equal(F.verify(fl2.id, { ok: false, reason: 'finish 9000', core: C }).action, 'kept-flagged'); assert.ok(!names().includes('FL2'));
  assert.match(F.stored.flagged.find((x) => x.id === fl2.id).flagged.reason, /verify-failed/);
  assert.equal(F.verify(at.id, { ok: true, core: C }).action, 'verified');
  assert.ok(F.queue(NEW).some((x) => x.id === at.id), 'a new core re-verifies the verified runs (D7 (3))');
  assert.ok(!F.queue(C).some((x) => x.id === at.id));
  for (let k = 0; k < 3; k++) F.verify(above.id, { ok: null, reason: 'no chrome', core: C });
  assert.ok(!F.queue(C).some((x) => x.id === above.id), 'three failed attempts: not again on this core');
}
// off: no directory given / the tables missing
assert.equal(createRecords({ dir: null, assets }).enabled, false);
assert.equal(createRecords({ dir: path.join(tmp, 'x'), assets: path.join(tmp, 'none'), log: { warn() {} } }).enabled, false);

// ---- HTTP (mp-server.mjs) ----
async function serve(env) {
  const port = 19000 + Math.floor(Math.random() * 900);
  const p = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: web, env: { ...process.env, MP_RECORDS_ASSETS: assets, ...env }, stdio: ['ignore', 'pipe', 'pipe'] });
  await new Promise((res, rej) => { const t = setTimeout(() => rej(new Error('server start')), 8000); p.stdout.on('data', (d) => { if (String(d).includes('multiplayer server')) { clearTimeout(t); res(); } }); });
  return { url: `http://127.0.0.1:${port}`, stop: () => p.kill() };
}
{
  const s = await serve({ MP_RECORDS_DIR: path.join(tmp, 'http') });
  try {
    let r = await fetch(s.url + '/mp/records'); assert.equal(r.status, 200); assert.equal((await r.json()).events['0:TST1'].top.length, 5);
    r = await fetch(s.url + '/mp/records/submit', { method: 'POST', body: upload({ name: 'HTTP', ticks: 10150 }) }); const body = await r.json(); assert.equal(r.status, 200); assert.equal(body.rank, 0);
    r = await fetch(s.url + '/mp/records/replay?id=' + body.id); assert.equal(r.status, 200); const f = decodeReplayFile(zlib.inflateRawSync(Buffer.from(await r.arrayBuffer()))); assert.equal(f.meta.name, 'HTTP');
    r = await fetch(s.url + '/mp/records/board?event=0:TST1&offset=5&limit=2'); const page = await r.json(); assert.equal(page.rows.length, 1); assert.equal(page.rows[0].rank, 5);
    r = await fetch(s.url + '/mp/records/board?event=nope'); assert.equal(r.status, 404);
    r = await fetch(s.url + '/mp/records/replay?id=../../etc'); assert.equal(r.status, 404);
    r = await fetch(s.url + '/mp/records/submit', { method: 'POST', body: Buffer.alloc(700 * 1024) }); assert.equal(r.status, 413);
  } finally { s.stop(); }
}
{   // the verifier's endpoints: loopback, the token, never with CF-Connecting-IP (through the tunnel); else 404
  const tok = 'v'.repeat(8) + Math.random().toString(16).slice(2) + 'token-x'; fs.writeFileSync(path.join(tmp, 'vtoken'), tok + '\n');
  const s = await serve({ MP_RECORDS_DIR: path.join(tmp, 'vhttp'), MP_RECORDS_VERIFIER_TOKEN_FILE: path.join(tmp, 'vtoken') });
  try {
    const sub = await (await fetch(s.url + '/mp/records/submit', { method: 'POST', body: upload({ name: 'VER', ticks: 10150 }) })).json();
    assert.equal((await fetch(s.url + '/mp/records/verifier/queue?core=abc')).status, 404, 'no token');
    assert.equal((await fetch(s.url + '/mp/records/verifier/queue?core=abc', { headers: { 'x-ssx-verifier': 'wrong' } })).status, 404, 'wrong token');
    assert.equal((await fetch(s.url + '/mp/records/verifier/queue', { headers: { 'x-ssx-verifier': tok, 'cf-connecting-ip': '1.2.3.4' } })).status, 404, 'through the tunnel');
    const q = await (await fetch(s.url + '/mp/records/verifier/queue?core=abcdef0123456789', { headers: { 'x-ssx-verifier': tok } })).json();
    assert.equal(q.items[0]?.id, sub.id);
    const res = await fetch(s.url + '/mp/records/verifier/result', { method: 'POST', headers: { 'x-ssx-verifier': tok }, body: JSON.stringify({ id: sub.id, ok: true, core: 'abcdef0123456789', reason: 'ok' }) });
    assert.equal((await res.json()).action, 'verified');
    const top = (await (await fetch(s.url + '/mp/records')).json()).events['0:TST1'].top[0]; assert.equal(top.verified, true);
  } finally { s.stop(); }
}
{ const s = await serve({}); try { const r = await fetch(s.url + '/mp/records'); assert.equal(r.status, 503); } finally { s.stop(); } }
{
  fs.writeFileSync(path.join(tmp, 'pw'), 'secret-pass\n', { mode: 0o600 });
  const s = await serve({ MP_RECORDS_DIR: path.join(tmp, 'gated'), MP_GATE_PASSWORD_FILE: path.join(tmp, 'pw'), MP_GATE_SECRET_FILE: path.join(tmp, 'gs'), MP_GATE_INSECURE: '1' });
  try { for (const u of ['/mp/records', '/mp/records/board?event=0:TST1', '/mp/records/replay?id=0123456789abcdef']) assert.equal((await fetch(s.url + u)).status, 401, u);
    assert.equal((await fetch(s.url + '/mp/records/submit', { method: 'POST', body: upload() })).status, 401); } finally { s.stop(); }
}
// The deploy ships web/server/ on its own (deploy/deploy-staged.sh): its modules import only node: and web/server/, and the server
// starts from a copy of that folder alone, with the records on (and their tables missing) or off. (2026-09-30: a records import
// from ../net took the live server down.)
for (const f of fs.readdirSync(path.join(web, 'server')).filter((f) => /\.(mjs|js)$/.test(f))) {
  const src = fs.readFileSync(path.join(web, 'server', f), 'utf8');
  for (const m of src.matchAll(/(?:^|\n)\s*import\s[^'"]*['"]([^'"]+)['"]|import\(\s*['"]([^'"]+)['"]/g)) {
    const spec = m[1] ?? m[2]; if (f === 'vite.online.config.js' || f === 'online.mjs') continue;   // build / dev helpers, not deployed to run
    assert.ok(spec.startsWith('node:') || (spec.startsWith('./') && !spec.includes('/..')), `web/server/${f} imports ${spec}`);
  }
}
{
  const alone = path.join(tmp, 'alone', 'web'); fs.cpSync(path.join(web, 'server'), path.join(alone, 'server'), { recursive: true });
  for (const env of [{}, { MP_RECORDS_DIR: path.join(tmp, 'alone', 'state', 'records'), MP_RECORDS_ASSETS: path.join(tmp, 'nothing') }, { MP_RECORDS_DIR: path.join(tmp, 'alone', 'state', 'records2') }]) {
    const port = 19000 + Math.floor(Math.random() * 900);
    const p = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], { cwd: alone, env: { PATH: process.env.PATH, MP_RECORDS_ASSETS: assets, ...env }, stdio: ['ignore', 'pipe', 'pipe'] });
    let out = ''; p.stdout.on('data', (d) => { out += d; }); p.stderr.on('data', (d) => { out += d; });
    await new Promise((res, rej) => { const t = setTimeout(() => rej(new Error('web/server alone did not start: ' + out)), 8000); p.stdout.on('data', () => { if (out.includes('multiplayer server on')) { clearTimeout(t); res(); } }); p.on('exit', (c) => { clearTimeout(t); rej(new Error(`exit ${c}: ${out}`)); }); });
    const r = await fetch(`http://127.0.0.1:${port}/mp/records`); assert.equal(r.status, env.MP_RECORDS_DIR && !env.MP_RECORDS_ASSETS?.endsWith('nothing') ? 200 : 503, JSON.stringify(env));
    p.kill();
  }
}
fs.rmSync(tmp, { recursive: true, force: true });
console.log('test-records-server: ok');
