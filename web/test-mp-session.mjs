// Time-attack sessions (web/server/sessions.mjs, docs/big-multiplayer.md TA-1): the phase machine on a fake clock (load quorum,
// window, attempts, overtime, verify, podium, vote, the next load), uploads and their tier-0 checks, time and score ranking, the
// verifier queue's priorities and results, players leaving / coming back, and the mount in mp-server.mjs end to end (off without
// MP_SESSIONS=1). Synthetic disc tables only (no game data needed).
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import net from 'node:net';
import { fileURLToPath } from 'node:url';
import { createSessions, MAX_ATTRIBUTE, RANDOM } from './server/sessions.mjs';
import { eventTable } from './server/records.mjs';
import { encodeReplayFile } from './server/replay-file.mjs';
import { encodePose, decodeBundle, POSE, LITE, DOT, FLAG_RIDING } from './server/ghost-packet.mjs';
import { createRecording } from './replay.js';

const web = path.dirname(fileURLToPath(import.meta.url));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-session-'));
// synthetic disc tables: course 0 = race 'TST1', course 5 = slope style 'TSS1', course 14 = backcountry 'TBC1' (Rival Time / Points)
const courses = Array.from({ length: 17 }, (_, i) => ({ index: i, code: i === 0 ? 'TST1' : i === 5 ? 'TSS1' : i === 14 ? 'TBC1' : `X${i}`, name: `c${i}` }));
const recordSlots = Array.from({ length: 17 }, (_, i) => (i === 0 ? [0, 12, 14, 26] : i === 5 ? [1, 17, 14, 26] : i === 14 ? [4, 6, 5, 7] : [9, 26, 14, 26]));
const records = Array.from({ length: 26 }, () => Array.from({ length: 5 }, (_, k) => ({ value: 170 + 10 * k, character: k, name: `D${k}` })));
const career = { rules: { record_slots: recordSlots, records }, courses };
const assets = path.join(tmp, 'assets');
fs.mkdirSync(path.join(assets, 'CAREER'), { recursive: true });
fs.writeFileSync(path.join(assets, 'CAREER', 'career.json'), JSON.stringify(career));
const events = eventTable(career);
assert.deepEqual([...events.keys()].sort(), ['0:TST1', '1:TSS1', '4:TBC1', '5:TBC1']);

// A replay file of a run on `event` (a synthetic pad stream).
function runFile({ event = '0:TST1', ticks = 6000, score = null, attributes = Array(7).fill(MAX_ATTRIBUTE), lineup = { values: [], enabled: false }, extra = {} } = {}) {
  const ev = events.get(event);
  const rec = createRecording();
  const p = new Float32Array(24);
  const total = (score != null ? ticks : ticks + 179) + 1;
  for (let t = 0; t < total; t++) {
    p[0] = (t >> 6) & 1;
    rec.push(p);
  }
  const meta = { event, mode: ev.mode, course: ev.code, round: 3, name: 'X', character: 4, rider: { id: 'zoe', attributes },
    claim: score != null ? { score } : { ticks }, ticks: total, finishTick: total - 1, events: [], lineup, core: 'abcdef0123456789', build: 'test', ...extra };
  return zlib.deflateRawSync(encodeReplayFile({ meta, pad: rec.exportBytes() }));
}
const runSeconds = (ticks) => ((ticks + 180) / 60) * 1000;

// ---- in process, fake clock ----
let clock = 1_000_000;
let rand = 0;
const inbox = new Map();
const clients = new Map();
const sent = (id) => inbox.get(id) ?? [];
const last = (id, t) => [...sent(id)].reverse().find((m) => m.t === t);
const client = (id, name = id) => {
  const c = { id, name, rider: 'zoe', pkg: 'RIDER_ZOE', token: `tok-${id}`, session: null };
  clients.set(id, c);
  return c;
};
const timing = { goDelayMs: 1000, loadMaxMs: 30000, overtimeMaxMs: 60000, uploadGraceMs: 10000, verifyMaxMs: 15000, podiumMs: 20000, voteMs: 15000, voteResultMs: 2000 };
const S = createSessions({ events, now: () => clock, send: (id, m) => { if (!inbox.has(id)) inbox.set(id, []); inbox.get(id).push(m); }, clients,
  random: () => rand, log: { log() {}, warn() {} }, options: timing });
const tick = (ms) => {
  for (let t = 0; t < ms; t += 250) {
    clock += 250;
    S.tick();
  }
};
const [a, b, c] = [client('a', 'Zoe'), client('b', 'BEN'), client('c', 'CAL')];
// the name shown is the player's own (the page's Player Name), not the connection's rider name
S.message(a, { t: 'ta-create', name: 'Big', limitMs: 300000, event: '0:TST1', playerName: 'ANNA' });
let joined = last('a', 'ta-joined');
assert.ok(joined, 'the creator joins');
const sid = joined.session.id;
assert.equal(joined.session.phase, 'load');
assert.equal(joined.session.event, '0:TST1');
assert.equal(joined.session.limitMs, 300000);
assert.equal(joined.session.hostId, 'a');
assert.equal(joined.session.podiumCourse, false, 'TST1 has no podium steps');
S.message(b, { t: 'ta-list' });
assert.equal(last('b', 'ta-sessions').sessions[0].id, sid);
S.message(b, { t: 'ta-join', session: sid.toLowerCase() });
S.message(c, { t: 'ta-join', session: sid });
assert.equal(last('c', 'ta-joined').roster.length, 3);
assert.ok(sent('a').some((m) => m.t === 'ta-roster' && m.add?.id === 'c'), 'roster deltas');
// load quorum: ceil(3 x 0.9) = 3 of 3
const win = () => S.sessions.get(sid).window;
for (const x of [a, b]) S.message(x, { t: 'ta-loaded', window: win().id });
tick(1000);
assert.equal(S.sessions.get(sid).phase, 'load', 'two of three loaded: still loading');
S.message(c, { t: 'ta-loaded', window: win().id });
tick(250);
let st = S.sessions.get(sid);
assert.equal(st.phase, 'window');
assert.equal(win().endAt - win().startAt, 300000);
// an attempt before the window opens (1 s slack) is ignored
S.message(a, { t: 'ta-attempt', window: win().id, n: 1 });
assert.equal(st.players.get('a').attempts.length, 1, 'within the 1 s slack');
clock = win().startAt;
S.message(b, { t: 'ta-attempt', window: win().id, n: 1 });
S.message(c, { t: 'ta-attempt', window: win().id, n: 1 });
const submit = (x, n, file, w = win().id) => S.submit({ compressed: file, token: x.token, sessionId: sid, windowId: w, attempt: n });
// a run cannot be longer than the time since its attempt started
let r = submit(a, 1, runFile({ ticks: 3000 }));
assert.equal(r.body.error, 'clock');
clock += runSeconds(3000);
r = submit(a, 1, runFile({ ticks: 3000 }));
assert.equal(r.status, 200, JSON.stringify(r.body));
assert.equal(r.body.kept, true);
assert.equal(r.body.rank, 0);
const runA = r.body.run;
// the same attempt twice
assert.equal(submit(a, 1, runFile({ ticks: 2990 })).body.error, 'attempt');
// checks
assert.equal(submit(b, 1, runFile({ ticks: 3100, attributes: Array(7).fill(5) })).body.error, 'attributes');
assert.equal(submit(b, 1, runFile({ ticks: 3100, lineup: { values: [1, 2, 3] } })).body.error, 'lineup');
assert.equal(submit(b, 1, runFile({ event: '1:TSS1', score: 5000, ticks: 3000 })).body.error, 'event');
assert.equal(submit(b, 2, runFile({ ticks: 3100 })).body.error, 'attempt', 'an attempt never announced');
assert.equal(S.submit({ compressed: runFile(), token: 'nobody', sessionId: sid, windowId: win().id, attempt: 1 }).status, 403);
assert.equal(submit(b, 1, Buffer.from('junk')).body.error, 'replay');
r = submit(b, 1, runFile({ ticks: 3100 }));
assert.equal(r.body.rank, 1);
const runB = r.body.run;
r = submit(c, 1, runFile({ ticks: 2900 }));
assert.equal(r.body.rank, 0, 'faster ranks first');
const runC = r.body.run;
// a slower run of a player is not kept
S.message(a, { t: 'ta-attempt', window: win().id, n: 2 });
clock += runSeconds(3500);
r = submit(a, 2, runFile({ ticks: 3500 }));
assert.equal(r.body.kept, false);
assert.equal(r.body.best, 3000);
// standings: pending runs listed, sent at most every 500 ms
tick(500);
let standings = last('b', 'ta-standings');
assert.deepEqual(standings.rows.map((x) => [x.name, x.value, x.status]), [['CAL', 2900, 'pending'], ['ANNA', 3000, 'pending'], ['BEN', 3100, 'pending']]);
assert.equal(standings.you.rank, 2);
assert.equal(standings.timed, true);
// the verifier queue: the podium candidates first (all three here), the best ranked first
let q = S.queue('abcdef0123456789');
assert.deepEqual(q.map((x) => [x.id, x.priority]), [[runC, 0], [runA, 0], [runB, 0]]);
assert.equal(S.verify(runA, { ok: true, core: 'abcdef0123456789' }).status, 'verified');
assert.equal(S.verify(runC, { ok: false, reason: 'time 3100, claimed 2900', core: 'abcdef0123456789' }).status, 'pulled');
assert.equal(S.verify('nope', { ok: true }), null);
tick(500);
standings = last('a', 'ta-standings');
assert.deepEqual(standings.rows.map((x) => [x.name, x.status]), [['ANNA', 'verified'], ['BEN', 'pending']], 'a pulled run leaves the standings');
// leave and come back in the same window: the runs stay
S.message(b, { t: 'ta-leave' });
assert.equal(last('b', 'ta-left').t, 'ta-left');
tick(500);
assert.equal(last('a', 'ta-standings').rows.length, 2, 'a player who left still counts');
S.message(b, { t: 'ta-join', session: sid });
assert.equal(S.sessions.get(sid).players.get('b').pending.length, 1, 'runs back on rejoin');
// the host's time limit applies from the next window
S.message(b, { t: 'ta-config', limitMs: 30000 });
assert.equal(S.sessions.get(sid).nextLimitMs, 300000, 'only the host');
S.message(a, { t: 'ta-config', limitMs: 90000 });
assert.equal(S.sessions.get(sid).nextLimitMs, 90000);
assert.equal(win().endAt - win().startAt, 300000);
// overtime: an attempt that started before the deadline may finish and upload
S.message(c, { t: 'ta-attempt', window: win().id, n: 2 });
clock = win().endAt - 1000;
tick(1000);
assert.equal(S.sessions.get(sid).phase, 'overtime');
S.message(b, { t: 'ta-attempt', window: win().id, n: 3 });
assert.equal(S.sessions.get(sid).players.get('b').attempts.length, 0, 'no new attempts after the deadline (b rejoined: its attempts were reset)');
tick(5000);
assert.equal(S.sessions.get(sid).phase, 'overtime', 'c is still riding');
S.message(c, { t: 'ta-end', window: win().id, n: 2, reason: 'finish', uploading: true });
tick(1000);
assert.equal(S.sessions.get(sid).phase, 'overtime', 'its upload is on its way');
r = submit(c, 2, runFile({ ticks: 2950 }));
assert.equal(r.status, 200, JSON.stringify(r.body));
const runC2 = r.body.run;
tick(500);
st = S.sessions.get(sid);
assert.equal(st.phase, 'verify');
// verify: c's 2950 (pending) is above the verified 3000: the podium waits for it
tick(5000);
assert.equal(st.phase, 'verify');
q = S.queue(null);
assert.equal(q[0].id, runC2);
assert.equal(q[0].priority, 0);
S.verify(runC2, { ok: true, core: 'abcdef0123456789' });
tick(250);
assert.equal(st.phase, 'verify', "BEN's pending 3100 could still take third");
tick(10000);
assert.equal(st.phase, 'podium', 'at most 15 s of verify');
let res = last('a', 'ta-results');
assert.deepEqual(res.podium.map((x) => [x.name, x.value]), [['CAL', 2950], ['ANNA', 3000]], 'only verified runs: BEN is still pending');
assert.deepEqual(res.standings.map((x) => [x.name, x.status]), [['CAL', 'verified'], ['ANNA', 'verified'], ['BEN', 'pending']]);
assert.equal(res.timed, true);
// vote: two events (not the one just ridden) and RANDOM
tick(20000);
assert.equal(st.phase, 'vote');
assert.equal(st.candidates.length, 3);
assert.equal(st.candidates[2], RANDOM);
assert.ok(!st.candidates.slice(0, 2).includes('0:TST1'));
S.message(a, { t: 'ta-vote', choice: 1 });
S.message(b, { t: 'ta-vote', choice: 1 });
S.message(c, { t: 'ta-vote', choice: 2 });
S.message(c, { t: 'ta-vote', choice: 9 });
tick(250);
assert.deepEqual(last('a', 'ta-votes').counts, [0, 2, 1]);
const next = st.candidates[1];
tick(15000);
assert.equal(st.phase, 'vote-result');
assert.equal(st.voteResult.event, next);
tick(2000);
assert.equal(st.phase, 'load');
assert.equal(st.event, next);
assert.equal(st.window.id, 2);
assert.equal(st.limitMs, 90000, "the host's new limit");
assert.equal(st.players.get('a').best, null, 'a new window starts empty');
assert.equal(st.players.get('a').name, 'ANNA', 'the player name');
// RANDOM wins: an event of the rotation but the two shown
{
  const R = createSessions({ events, now: () => clock, send: () => {}, clients, random: () => 0.99, log: { log() {}, warn() {} }, options: timing });
  const x = client('rx', 'RX');
  R.message(x, { t: 'ta-create', limitMs: 60000, event: '0:TST1' });
  const rs = [...R.sessions.values()][0];
  rs.candidates = ['1:TSS1', '4:TBC1', RANDOM];
  rs.phase = 'vote';
  rs.phaseEndsAt = clock;
  rs.votes = new Map([['rx', 2]]);
  R.tick();
  assert.equal(rs.voteResult.random, true);
  assert.ok(!['1:TSS1', '4:TBC1'].includes(rs.voteResult.event) && events.has(rs.voteResult.event), rs.voteResult.event);
}
assert.ok([...S.runs.values()].some((x) => x.window === 1), "the last window's replays stay");

// ---- a score event: the highest wins; a session's podium wait ends after verifyMaxMs ----
{
  const [d, e] = [client('d', 'DAN'), client('e', 'EVE')];
  S.message(d, { t: 'ta-create', limitMs: 60000, event: '1:TSS1' });
  const id = last('d', 'ta-joined').session.id;
  S.message(e, { t: 'ta-join', session: id });
  const s2 = S.sessions.get(id);
  S.message(d, { t: 'ta-loaded', window: 1 });
  S.message(e, { t: 'ta-loaded', window: 1 });
  tick(250);
  clock = s2.window.startAt;
  S.message(d, { t: 'ta-attempt', window: 1, n: 1 });
  S.message(e, { t: 'ta-attempt', window: 1, n: 1 });
  clock += runSeconds(3000);
  const up = (x, score) => S.submit({ compressed: runFile({ event: '1:TSS1', score, ticks: 3000 }), token: x.token, sessionId: id, windowId: 1, attempt: 1 });
  const rd = up(d, 40000).body;
  const re = up(e, 52000).body;
  assert.equal(re.rank, 0, 'the higher score ranks first');
  S.verify(rd.run, { ok: true, core: 'abcdef0123456789' });
  clock = s2.window.endAt;
  tick(500);
  assert.equal(s2.phase, 'verify');
  tick(14000);
  assert.equal(s2.phase, 'verify');
  tick(1500);
  assert.equal(s2.phase, 'podium', 'at most 15 s of verify');
  res = last('d', 'ta-results');
  assert.deepEqual(res.podium.map((x) => [x.name, x.value]), [['DAN', 40000]], "EVE's unverified run is skipped for the podium");
  assert.equal(res.timed, false);
  // a run that does not reproduce on another core is stale, not pulled
  S.message(d, { t: 'ta-leave' });
  S.message(e, { t: 'ta-leave' });
  assert.ok(!S.sessions.has(id), 'an empty session closes');
}

// ---- resume ----
S.dropped(a);
assert.equal(last('b', 'ta-roster').update.online, false);
S.resumed(a);
assert.equal(last('b', 'ta-roster').update.online, true);
assert.ok(last('a', 'ta-joined'));

// ---- ghosts: the tiers per recipient (nearest 2 poses every bundle, the next 2 lite at 4 Hz + a pose every 2 s, the rest dots at 1 Hz) ----
{
  const bins = new Map();
  const G = createSessions({ events, now: () => clock, send: () => {}, sendBinary: (id, bytes) => { if (!bins.has(id)) bins.set(id, []); bins.get(id).push(bytes); },
    clients, random: () => 0, log: { log() {}, warn() {} }, options: { ...timing, nearCount: 2, midCount: 2 } });
  const ps = Array.from({ length: 7 }, (_, i) => client(`g${i}`, `G${i}`));
  G.message(ps[0], { t: 'ta-create', limitMs: 120000, event: '0:TST1' });
  const gid = [...G.sessions.keys()][0];
  for (const x of ps.slice(1)) G.message(x, { t: 'ta-join', session: gid });
  const rig = Array.from({ length: 3 }, (_, i) => ({ parent_index: i - 1, source_translation_cm: [0, 0, 10, 1], name: `b${i}` }));
  const poseAt = (x, n) => {
    const pose = new Float32Array(21);
    for (let b = 0; b < 3; b++) {
      pose[b * 7] = x;
      pose[b * 7 + 2] = 10 * b;
      pose[b * 7 + 6] = 1;
    }
    return encodePose({ flags: FLAG_RIDING, serial: n, time: clock, pose, scale: [1, 1, 1], free: [], rigBones: rig });
  };
  // player i rides at x = 100 i (player 0 at 0): from player 0 the nearest are 1, 2 (near), 3, 4 (mid), 5, 6 (far)
  let n = 0;
  const feed = () => ps.forEach((x, i) => assert.ok(G.binary(x, poseAt(100 * i, ++n))));
  bins.clear();
  feed();
  G.ghostTick();
  const kinds = (id) => decodeBundle(bins.get(id).at(-1)).entries.map((e) => [e.player, e.kind]);
  const idx = (i) => G.sessions.get(gid).players.get(`g${i}`).index;
  assert.deepEqual(kinds('g0'), [[idx(1), POSE], [idx(2), POSE], [idx(3), POSE], [idx(4), POSE], [idx(5), DOT], [idx(6), DOT]], 'first bundle: poses for near and mid, dots for far');
  const first = decodeBundle(bins.get('g0').at(-1)).entries[0].value;
  assert.deepEqual([first.root[0], first.bones, first.flags], [100, 3, FLAG_RIDING]);
  clock += 100;
  feed();
  G.ghostTick();
  assert.deepEqual(kinds('g0'), [[idx(1), POSE], [idx(2), POSE]], '100 ms later: the near poses only (mid lite every 250 ms, its pose every 2 s)');
  clock += 200;
  feed();
  G.ghostTick();
  assert.deepEqual(kinds('g0'), [[idx(1), POSE], [idx(2), POSE], [idx(3), LITE], [idx(4), LITE]]);
  clock += 1000;
  feed();
  G.ghostTick();
  assert.deepEqual(kinds('g0'), [[idx(1), POSE], [idx(2), POSE], [idx(3), LITE], [idx(4), LITE], [idx(5), DOT], [idx(6), DOT]]);
  // nothing new: nothing sent; a stale ghost is not sent
  const before = bins.get('g0').length;
  G.ghostTick();
  assert.equal(bins.get('g0').length, before, 'no bundle without news');
  clock += 4000;
  G.ghostTick();
  assert.equal(bins.get('g0').length, before, 'stale ghosts are not sent');
  // a malformed pose is refused
  assert.equal(G.binary(ps[0], new Uint8Array([5, 1, 0, 0])), false);
  assert.equal(G.ghostStats.dropped, 1);
}

// ---- end to end through mp-server.mjs ----
const freePort = () => new Promise((resolve) => {
  const srv = net.createServer();
  srv.listen(0, '127.0.0.1', () => {
    const { port } = srv.address();
    srv.close(() => resolve(port));
  });
});
async function startServer(env) {
  const port = await freePort();
  const proc = spawn(process.execPath, [path.join(web, 'server', 'mp-server.mjs'), '--port', String(port), '--host', '127.0.0.1'],
    { env: { ...process.env, MP_RECORDS_ASSETS: assets, ...env }, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = '';
  proc.stdout.on('data', (d) => { out += d; });
  proc.stderr.on('data', (d) => { out += d; });
  for (let i = 0; i < 100 && !/multiplayer server on/.test(out); i++) await new Promise((r) => setTimeout(r, 50));
  assert.match(out, /multiplayer server on/, out);
  return { port, proc, log: () => out };
}
function wsClient(port, name) {
  return new Promise((resolve) => {
    const ws = new WebSocket(`ws://127.0.0.1:${port}/mp`);
    const box = { ws, messages: [], waiters: [], token: `token-${name}-${Math.random()}` };
    box.wait = (t, pred = () => true) => new Promise((res) => {
      const hit = box.messages.find((m) => m.t === t && pred(m));
      if (hit) return res(hit);
      box.waiters.push({ t, pred, res });
    });
    box.send = (m) => ws.send(JSON.stringify(m));
    ws.onmessage = (e) => {
      if (typeof e.data !== 'string') return;
      const m = JSON.parse(e.data);
      box.messages.push(m);
      for (const w of [...box.waiters]) if (w.t === m.t && w.pred(m)) {
        box.waiters.splice(box.waiters.indexOf(w), 1);
        w.res(m);
      }
    };
    ws.onopen = () => {
      box.send({ t: 'hello', name, rider: 'zoe', version: 2, token: box.token });
      // the client's clock-sync pings (mp-client.js: every 2 s); a socket silent for 10 s is closed by the server
      box.pinger = setInterval(() => box.send({ t: 'ping', c: Date.now() }), 2000);
      ws.addEventListener('close', () => clearInterval(box.pinger));
      resolve(box);
    };
  });
}
{
  // off: no MP_SESSIONS
  const off = await startServer({ MP_SESSIONS: '' });
  const x = await wsClient(off.port, 'OFF');
  await x.wait('welcome');
  x.send({ t: 'ta-list' });
  assert.equal((await x.wait('error')).code, 'sessions');
  const resp = await fetch(`http://127.0.0.1:${off.port}/mp/session/run?session=X`, { method: 'POST', body: 'x' });
  assert.equal(resp.status, 503);
  x.ws.close();
  off.proc.kill();
}
{
  const tokenFile = path.join(tmp, 'verifier-token');
  fs.writeFileSync(tokenFile, 'v'.repeat(32));
  const opts = { goDelayMs: 200, overtimeMaxMs: 2000, verifyMaxMs: 3000, podiumMs: 500, voteMs: 500, voteResultMs: 200, minLimitMs: 100 };
  const on = await startServer({ MP_SESSIONS: '1', MP_RECORDS_VERIFIER_TOKEN_FILE: tokenFile, MP_SESSION_OPTIONS: JSON.stringify(opts) });
  const base = `http://127.0.0.1:${on.port}`;
  const [h, g] = [await wsClient(on.port, 'HOST'), await wsClient(on.port, 'GUEST')];
  await h.wait('welcome');
  await g.wait('welcome');
  // a lobby member who creates a session leaves the lobby
  h.send({ t: 'create', name: 'old lobby', course: 'TST1' });
  await h.wait('lobby');
  h.send({ t: 'ta-create', name: 'E2E', limitMs: 5000, event: '0:TST1' });
  const j = await h.wait('ta-joined');
  const id = j.session.id;
  g.send({ t: 'ta-join', session: id });
  await g.wait('ta-joined');
  h.send({ t: 'ta-loaded', window: 1 });
  g.send({ t: 'ta-loaded', window: 1 });
  const open = await h.wait('ta-state', (m) => m.session.phase === 'window');
  await new Promise((r) => setTimeout(r, Math.max(0, open.session.window.startAt - Date.now())));
  h.send({ t: 'ta-attempt', window: 1, n: 1 });
  await new Promise((r) => setTimeout(r, 100));
  const pad = runFile({ ticks: 20 });
  const upload = await fetch(`${base}/mp/session/run?session=${id}&window=1&attempt=1`, { method: 'POST', headers: { 'x-ssx-session': h.token }, body: pad });
  const body = await upload.json();
  assert.equal(body.error, 'clock', JSON.stringify(body));
  await new Promise((r) => setTimeout(r, runSeconds(20) - 200));
  const upload2 = await fetch(`${base}/mp/session/run?session=${id}&window=1&attempt=1`, { method: 'POST', headers: { 'x-ssx-session': h.token }, body: pad });
  const body2 = await upload2.json();
  assert.equal(body2.kept, true, JSON.stringify(body2));
  // the replay as stored, for the verifier page
  const replay = Buffer.from(await (await fetch(`${base}/mp/session/replay?id=${body2.run}`)).arrayBuffer());
  assert.deepEqual(replay, pad);
  // the verifier endpoints: the token on loopback only
  assert.equal((await fetch(`${base}/mp/session/verifier/queue`)).status, 404);
  const vh = { 'x-ssx-verifier': 'v'.repeat(32) };
  const queue = await (await fetch(`${base}/mp/session/verifier/queue?core=abcdef0123456789`, { headers: vh })).json();
  assert.equal(queue.items[0].id, body2.run);
  const result = await fetch(`${base}/mp/session/verifier/result`, { method: 'POST', headers: vh,
    body: JSON.stringify({ id: body2.run, ok: true, reason: 'reproduced', core: 'abcdef0123456789' }) });
  assert.equal((await result.json()).status, 'verified');
  const status = await (await fetch(`${base}/mp/status`)).json();
  assert.equal(status.sessions[0].id, id);
  assert.equal(status.lobbies.length, 0, 'the old lobby closed when its only member left');
  const results = await h.wait('ta-results');
  assert.deepEqual(results.podium.map((x) => x.name), ['HOST']);
  const vote = await g.wait('ta-state', (m) => m.session.phase === 'vote');
  g.send({ t: 'ta-vote', choice: 0 });
  const nextLoad = await g.wait('ta-state', (m) => m.session.phase === 'load' && m.session.window.id === 2);
  assert.equal(nextLoad.session.event, vote.session.candidates[0]);
  // resume: the same token on a new socket gets the session again
  g.ws.close();
  await new Promise((r) => setTimeout(r, 200));
  const g3 = await new Promise((resolve) => {
    const ws = new WebSocket(`ws://127.0.0.1:${on.port}/mp`);
    const box = { ws, messages: [] };
    ws.onmessage = (e) => {
      const m = JSON.parse(e.data);
      box.messages.push(m);
      if (m.t === 'ta-joined') resolve(m);
    };
    ws.onopen = () => ws.send(JSON.stringify({ t: 'hello', name: 'GUEST', rider: 'zoe', version: 2, token: g.token }));
  });
  assert.equal(g3.session.id, id, 'resumed into the session');
  h.ws.close();
  on.proc.kill();
}
console.log('time-attack sessions: phases, uploads, ranking, verifier queue, leave / rejoin / resume, mp-server mount OK');
process.exit(0);
