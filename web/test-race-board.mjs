// Online race boards (web/server/race-board.mjs, pv mpBoard; docs/multiplayer.md "Multiplayer leaderboard"): the module's rules on a
// fake clock (the event of a course, only a finished racer of a race the server ran, once per race, the value from the server's record,
// DNF / implausible / unknown / expired races refused, names by records.mjs cleanName, time and score order, one entry per name, the
// rate limits, persistence, no address or token stored), then mp-server end to end with node clients: two racers finish a real race,
// both save over HTTP with their tokens, the board lists both, a stranger's token and a second save are refused, the course records'
// board.json is untouched, a restarted server (the races in memory lost) refuses a save cleanly, and the board survives the restart.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { createRaceBoard, courseEvents, RACE_KEEP_MS } from './server/race-board.mjs';
import { eventTable } from './server/records.mjs';
import { createMpClient } from './net/mp-client.js';
import { orderTrickRows } from './net/trick-rows.js';

const web = path.dirname(new URL(import.meta.url).pathname);
// the disc's course table shape (career.json): Snow Jam race, R&B slope style, Happiness's Rival Time / Rival Points
const career = {
  courses: [{ code: 'ARA1' }, { code: 'ASS1' }, { code: 'ABC1' }],
  rules: { record_slots: [[0, 12, 14, 26], [1, 17, 14, 26], [4, 6, 5, 7]], records: {} }
};
const events = eventTable(career);
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx3-race-board-'));

// ---- the module ----
{
  assert.deepEqual([...courseEvents(events)], [['ARA1', '0:ARA1'], ['ASS1', '1:ASS1'], ['ABC1', '4:ABC1']], 'a course names its first record-slot event');
  let t = Date.parse('2026-10-15T12:00:00Z');
  const dir = path.join(tmp, 'unit');
  const rb = createRaceBoard({ dir, events, now: () => t });
  assert.ok(rb.enabled);
  assert.equal(createRaceBoard({ dir: null, events }).enabled, false, 'off without a state directory');
  assert.equal(createRaceBoard({ dir: path.join(tmp, 'none'), events: new Map() }).enabled, false, 'off without the course table');
  rb.raceDone({
    id: 'R1',
    course: 'ARA1',
    racers: [
      { id: 'a', ticks: 9000, dnf: false, rider: 'zoe' },
      { id: 'b', ticks: 9500, dnf: false, rider: 'mac', verified: true },
      { id: 'c', ticks: 0, dnf: true, rider: 'elise' },
      { id: 'd', ticks: 8000, dnf: false, rider: 'kaori', verified: false }
    ]
  });
  const save = (id, race, name) => rb.save(id, { race, name });
  assert.equal(save('a', 'R1', 'fuckface').body.error, 'name', 'the blocklist');
  assert.equal(save('a', 'R1', 'TOOLONGNAME').body.error, 'name', 'eight characters at most');
  assert.equal(save('a', 'R1', 'O-WEN').body.error, 'name', 'the keyboard has no punctuation keys for a name');
  assert.equal(save('a', 'R1', '   ').body.error, 'name', 'an empty name');
  let r = save('a', 'R1', '  Owen ');
  assert.equal(r.status, 200);
  assert.deepEqual([r.body.kept, r.body.rank, r.body.name, r.body.rows[0].value, r.body.rows[0].rider, r.body.rows[0].at], [true, 0, 'Owen', 9000, 'zoe', '2026-10-15']);
  assert.equal(save('a', 'R1', 'Owen').body.error, 'saved', 'once per racer per race');
  assert.equal(save('c', 'R1', 'Carol').body.error, 'dnf');
  assert.equal(save('d', 'R1', 'Dee').body.error, 'invalid', 'a finish the plausibility check found implausible');
  assert.equal(save('x', 'R1', 'Xavier').body.error, 'racer', 'not a racer of that race');
  assert.equal(save('b', 'R9', 'Bob').body.error, 'race', 'a race the server did not run');
  assert.equal(save(null, 'R1', 'Bob').body.error, 'token');
  r = save('b', 'R1', 'Bob');
  assert.deepEqual([r.body.kept, r.body.rank, r.body.total], [true, 1, 2]);
  // the minute's six saves of 'a' are used up: a minute later
  t += 61000;
  // a second race: Owen slower (not kept, his best stays), Bob faster (replaces his own row)
  rb.raceDone({ id: 'R2', course: 'ARA1', racers: [{ id: 'a', ticks: 9900, dnf: false, rider: 'zoe' }, { id: 'b', ticks: 8800, dnf: false, rider: 'mac' }] });
  r = save('a', 'R2', 'OWEN');
  assert.deepEqual([r.body.kept, r.body.rank, r.body.you.value, r.body.total], [false, 0, 9000, 2], 'one entry per name (case-insensitive), the best kept');
  r = save('b', 'R2', 'Bob');
  assert.deepEqual([r.body.kept, r.body.rank, r.body.total], [true, 0, 2]);
  assert.deepEqual(rb.board('0:ARA1', 'owen').rows.map((x) => [x.name, x.value, x.race]), [['Bob', 8800, 'R2'], ['Owen', 9000, 'R1']]);
  assert.equal(rb.board('0:ARA1', 'owen').you.rank, 1, 'your own rank');
  assert.equal(rb.board('0:XXXX'), null);
  // a tie goes to the earlier entry
  rb.raceDone({ id: 'R3', course: 'ARA1', racers: [{ id: 'e', ticks: 9000, dnf: false, rider: 'moby' }] });
  assert.equal(save('e', 'R3', 'Eve').body.rank, 2, 'a tie ranks below the earlier entry');
  // a trick event ranks by score; its value is the score the finish message carried (no score: refused)
  rb.raceDone({ id: 'S1', course: 'ASS1', racers: [{ id: 'a', ticks: 7000, score: 120000, rider: 'zoe' }, { id: 'b', ticks: 6000, score: 150000, rider: 'mac' }, { id: 'c', ticks: 6000, rider: 'elise' }] });
  save('a', 'S1', 'Owen');
  r = save('b', 'S1', 'Bob');
  assert.deepEqual([r.body.timed, r.body.rows.map((x) => [x.name, x.value])], [false, [['Bob', 150000], ['Owen', 120000]]], 'score events by score, highest first');
  assert.equal(save('c', 'S1', 'Carol').body.error, 'value', 'a trick finish without a score');
  // Rival Time on a backcountry course: timed
  rb.raceDone({ id: 'B1', course: 'ABC1', racers: [{ id: 'a', ticks: 5000, rider: 'zoe' }] });
  assert.equal(save('a', 'B1', 'Owen').body.event, '4:ABC1');
  // an unknown course keeps nothing
  rb.raceDone({ id: 'M1', course: 'PEAK1', racers: [{ id: 'a', ticks: 5000, rider: 'zoe' }] });
  assert.equal(save('a', 'M1', 'Owen').body.error, 'race');
  // expiry: a race older than RACE_KEEP_MS is gone
  rb.raceDone({ id: 'R4', course: 'ARA1', racers: [{ id: 'f', ticks: 9100, rider: 'nate' }] });
  t += RACE_KEEP_MS + 1000;
  assert.equal(save('f', 'R4', 'Fay').body.error, 'race', 'an expired race');
  // rate limits per connection: 6 a minute
  const codes = [];
  for (let i = 0; i < 8; i++) codes.push(save('g', 'R5', 'Gus').body.error);
  assert.deepEqual(codes, ['race', 'race', 'race', 'race', 'race', 'race', 'rate', 'rate'], 'six saves a minute per connection');
  t += 61000;
  assert.equal(save('g', 'R5', 'Gus').body.error, 'race', 'the minute bucket refills');
  // persistence and privacy
  const stored = JSON.parse(fs.readFileSync(path.join(dir, 'race-board.json'), 'utf8'));
  const keys = new Set(Object.values(stored.events).flat().flatMap((e) => Object.keys(e)));
  assert.deepEqual([...keys].sort(), ['at', 'id', 'name', 'race', 'rider', 'seq', 'value'], 'an entry stores no address, token or client id');
  const again = createRaceBoard({ dir, events, now: () => t });
  assert.deepEqual(again.board('0:ARA1').rows.map((x) => x.name), ['Bob', 'Owen', 'Eve'], 'the board survives a restart');
  assert.equal(again.save('b', { race: 'R2', name: 'Bob' }).body.error, 'race', 'the races in memory do not');
  console.log('race board: events, racer checks, value from the server, names, order, one per name, expiry, rate limits, persistence OK');
}

// ---- a trick event's Results order (web/net/trick-rows.js: 0x239EC8 -> 238B70, 0x245E30 the higher score first) ----
{
  const rows = [
    { slot: 0, score: 1200, dnf: false },
    { slot: 1, score: null, dnf: false, onCourse: true },
    { slot: 2, score: 0, dnf: true },
    { slot: 3, score: 5400, dnf: false },
    { slot: 4, score: 1200, dnf: false },
    { slot: 5, score: null, dnf: false }
  ];
  const order = orderTrickRows(rows);
  assert.deepEqual(order.map((r) => r.slot), [3, 0, 4, 1, 5, 2], 'by score, equal scores in slot order, then on course, then DNF');
  assert.deepEqual(order.map((r) => r.place), [0, 1, 2, 3, 4, 5]);
  assert.ok(order[4].onCourse, 'a racer without a score yet is on course');
  console.log('race board: trick Results order OK');
}

// ---- the page's side (web/mp-board-ui.js) against stub servers: a refused or failed save ends on the game's message line, no hang ----
{
  const { MpBoard } = await import('./mp-board-ui.js');
  const realFetch = globalThis.fetch;
  const mem = new Map();
  globalThis.localStorage = { getItem: (k) => (mem.has(k) ? mem.get(k) : null), setItem: (k, v) => mem.set(k, String(v)), removeItem: (k) => mem.delete(k) };
  const ui = { index: 0, sync() {}, feScreens: { playerName: 'PLAYER 1', keyboard: null, openKeyboard(kind, o) { this.keyboard = { kind, ...o }; } }, careerUI: null };
  const session = { race: { id: 'R1', course: 'ARA1', slot: 0 }, finalResults: [{ slot: 0, ticks: 9000, dnf: false }], client: { token: 'tok' } };
  const reply = (status, body) => async () => ({ ok: status === 200, status, json: async () => body });
  const board = { event: '0:ARA1', timed: true, total: 0, rows: [], you: null };
  const settle = () => new Promise((r) => setTimeout(r, 20));
  const runWith = async (saveReply) => {
    globalThis.fetch = async (url, init) => (init?.method === 'POST' ? saveReply(url, init) : reply(200, board)());
    const b = new MpBoard(ui, session);
    b.start();
    return b;
  };
  // no saved name: the keyboard asks; a name the pad keyboard cannot type is refused before any request, the profile untouched
  let b = await runWith(reply(200, {}));
  assert.equal(ui.feScreens.keyboard?.kind, 'record');
  ui.feScreens.keyboard.done('O-W');
  assert.deepEqual([b.view.note, mem.get('ssx3.playerName')], ['The record could not be saved.', undefined]);
  // the server restarted (a deploy): the race is gone -> the message line, Save Records greyed, the board still shown
  b = await runWith(reply(404, { error: 'race' }));
  ui.feScreens.keyboard.done('OWEN');
  await settle();
  assert.deepEqual([b.view.status, b.view.note, b.view.canSave, b.disabled(1)], ['ready', 'The record could not be saved.', false, true]);
  assert.equal(mem.get('ssx3.playerName'), 'OWEN', 'the name is on the profile once entered');
  // a later race: the saved name, no keyboard; the server unreachable -> 'Online records unavailable.'
  ui.feScreens.keyboard = null;
  b = await runWith(async () => {
    throw new Error('down');
  });
  await settle();
  assert.deepEqual([ui.feScreens.keyboard, b.view.note], [null, 'Online records unavailable.']);
  // the blocklist (server 'name'): the profile goes back to what it was
  mem.delete('ssx3.playerName');
  b = await runWith(reply(400, { error: 'name' }));
  ui.feScreens.keyboard.done('BADNAME');
  await settle();
  assert.deepEqual([mem.get('ssx3.playerName'), b.view.note, b.view.canSave], [undefined, 'The record could not be saved.', true]);
  // saved: the row and the message
  mem.set('ssx3.playerName', 'OWEN');
  const row = { id: 'x1', rank: 0, name: 'OWEN', rider: 'zoe', value: 9000, race: 'R1', at: '2026-10-15' };
  b = await runWith(reply(200, { ok: true, kept: true, id: 'x1', rank: 0, event: '0:ARA1', timed: true, total: 1, rows: [row], you: row }));
  await settle();
  assert.deepEqual([b.view.saved, b.view.note, b.shownRows()[0].player], [true, 'Your record is number 1 online.', true]);
  // your own row below the top 4 when you are outside the top 5
  b.view.rows = Array.from({ length: 10 }, (_, i) => ({ ...row, id: `r${i}`, rank: i, name: `R${i}` }));
  b.view.you = { ...row, id: 'me', rank: 11 };
  assert.deepEqual(b.shownRows().map((r) => [r.rank, r.player]), [[1, false], [2, false], [3, false], [4, false], [12, true]]);
  globalThis.fetch = realFetch;
  console.log('race board: the page side (keyboard on first use, profile name, refused / lost / offline saves on the message line, rows) OK');
}

// ---- mp-server end to end ----
const assets = path.join(tmp, 'assets');
fs.mkdirSync(path.join(assets, 'CAREER'), { recursive: true });
fs.writeFileSync(path.join(assets, 'CAREER', 'career.json'), JSON.stringify(career));
const records = path.join(tmp, 'records');
const port = 26000 + Math.floor(Math.random() * 1000);
const start = () => {
  const p = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], {
    cwd: web,
    stdio: ['ignore', 'pipe', 'inherit'],
    env: { ...process.env, MP_RECORDS_DIR: records, MP_RECORDS_ASSETS: assets, MP_PLAUSIBILITY: 'off', MP_FINISH_GRACE_MS: '1500' }
  });
  return new Promise((resolve, reject) => {
    const t = setTimeout(() => reject(new Error('mp-server did not start')), 8000);
    p.stdout.on('data', (d) => {
      if (String(d).includes('multiplayer server on')) {
        clearTimeout(t);
        resolve(p);
      }
    });
  });
};
const http = `http://127.0.0.1:${port}`;
const url = `ws://127.0.0.1:${port}/mp`;
const wait = (client, type, pred = () => true, ms = 5000) =>
  new Promise((resolve, reject) => {
    const t = setTimeout(() => reject(new Error(`timeout waiting for ${type}`)), ms);
    const off = client.on(type, (v) => {
      if (pred(v)) {
        clearTimeout(t);
        off();
        resolve(v);
      }
    });
  });
const saveHttp = (token, body) =>
  fetch(`${http}/mp/race-board/save`, { method: 'POST', headers: { 'content-type': 'application/json', 'x-ssx-session': token }, body: JSON.stringify(body) }).then(async (r) => ({ status: r.status, body: await r.json() }));
let server = await start();
const clients = [];
try {
  const a = createMpClient({ url, name: 'Zoe', rider: 'zoe', pkg: 'RIDER_ZOE', token: 'tok-board-a-123456' });
  const b = createMpClient({ url, name: 'Mac', rider: 'mac', pkg: 'RIDER_MAC', token: 'tok-board-b-123456' });
  const s = createMpClient({ url, name: 'Spy', rider: 'moby', pkg: 'RIDER_MOBY', token: 'tok-board-s-123456' });
  clients.push(a, b, s);
  for (const c of clients) await c.connect();
  a.create({ name: 'board', course: 'ARA1' });
  const lobby = await wait(a, 'lobby', (l) => l);
  b.join(lobby.id);
  await wait(a, 'lobby', (l) => l.members.length === 2);
  a.start();
  const [race] = await Promise.all([wait(a, 'start'), wait(b, 'start')]);
  a.loaded();
  b.loaded();
  await wait(a, 'go');
  const resultsP = wait(a, 'results');
  a.finish(9000);
  b.finish(9600);
  const results = await resultsP;
  assert.deepEqual(results.map((x) => [x.name, x.ticks]), [['Zoe', 9000], ['Mac', 9600]]);
  assert.equal((await saveHttp('tok-board-s-123456', { race: race.id, name: 'SPY' })).body.error, 'racer', "a stranger cannot save another race's time");
  assert.equal((await saveHttp('nobody', { race: race.id, name: 'X' })).body.error, 'token', 'an unknown token');
  const sa = await saveHttp('tok-board-a-123456', { race: race.id, name: 'OWEN' });
  assert.deepEqual([sa.status, sa.body.kept, sa.body.rank], [200, true, 0]);
  const sb = await saveHttp('tok-board-b-123456', { race: race.id, name: 'MAC' });
  assert.deepEqual([sb.body.rank, sb.body.rows.map((x) => [x.name, x.value, x.rider])], [1, [['OWEN', 9000, 'zoe'], ['MAC', 9600, 'mac']]]);
  assert.equal((await saveHttp('tok-board-a-123456', { race: race.id, name: 'OWEN' })).body.error, 'saved');
  const got = await (await fetch(`${http}/mp/race-board?course=ARA1&name=mac`)).json();
  assert.equal(got.event, '0:ARA1');
  assert.deepEqual([got.total, got.you.rank, got.you.name], [2, 1, 'MAC']);
  assert.equal((await fetch(`${http}/mp/race-board?event=9:NONE`)).status, 404);
  // a separate board: the course records' file holds none of it
  const recordsFile = path.join(records, 'board.json');
  const recordsText = fs.existsSync(recordsFile) ? fs.readFileSync(recordsFile, 'utf8') : '';
  assert.ok(!recordsText.includes('OWEN'), 'the verified course records are untouched');
  assert.ok(fs.existsSync(path.join(records, 'race-board.json')));
  // a trick event's score from the finish message
  a.course('ASS1');
  await wait(a, 'lobby', (l) => l.course === 'ASS1');
  a.start();
  const [race2] = await Promise.all([wait(a, 'start'), wait(b, 'start')]);
  a.loaded();
  b.loaded();
  await wait(a, 'go');
  const results2 = wait(a, 'results');
  const scoreSeen = wait(b, 'finished', (m) => m.slot === 0);
  a.finish(7000, false, '', 64000);
  b.finish(6900, false, '', 51000);
  assert.equal((await scoreSeen).score, 64000, 'the server echoes the score to the other racers');
  const r2 = await results2;
  assert.deepEqual(r2.map((x) => x.score).sort(), [51000, 64000], 'the final results carry the scores');
  const fa = await saveHttp('tok-board-a-123456', { race: race2.id, name: 'OWEN' });
  const fb = await saveHttp('tok-board-b-123456', { race: race2.id, name: 'MAC' });
  assert.deepEqual([fa.body.event, fa.body.timed, fb.body.rows.map((x) => [x.name, x.value])], ['1:ASS1', false, [['OWEN', 64000], ['MAC', 51000]]]);
  // a server restart (a deploy): the race is gone, the save is refused cleanly, the board is kept
  a.start();
  const [race3] = await Promise.all([wait(a, 'start'), wait(b, 'start')]);
  a.loaded();
  b.loaded();
  await wait(a, 'go');
  const results3 = wait(a, 'results');
  a.finish(6000, false, '', 70000);
  b.finish(6100, false, '', 1);
  await results3;
  for (const c of clients) c.disconnect();
  server.kill();
  await new Promise((r) => server.once('exit', r));
  server = await start();
  const c2 = createMpClient({ url, name: 'Zoe', rider: 'zoe', pkg: 'RIDER_ZOE', token: 'tok-board-a-123456' });
  clients.push(c2);
  await c2.connect();
  const lost = await saveHttp('tok-board-a-123456', { race: race3.id, name: 'OWEN' });
  assert.deepEqual([lost.status, lost.body.error], [404, 'race'], 'after a restart the race is unknown: refused, no hang');
  const kept = await (await fetch(`${http}/mp/race-board?event=1:ASS1`)).json();
  assert.deepEqual(kept.rows.map((x) => x.name), ['OWEN', 'MAC'], 'the stored board survives the restart');
  console.log('race board: mp-server end to end (real race, token saves, stranger refused, separate board, trick score, restart) OK');
} finally {
  for (const c of clients) c.disconnect();
  server.kill();
  fs.rmSync(tmp, { recursive: true, force: true });
}
