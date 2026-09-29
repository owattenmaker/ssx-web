// Online multiplayer end to end: web/server/mp-server.mjs + web/net/mp-client.js with node WebSocket clients.
// Lobbies, invite join, resume after reload, version check, synchronized start, binary relay (after GO only),
// late joiner waits for the next race, leave / disconnect mid-race (DNF, host migration, presence, automatic
// reconnect back into the race), results by time with DNF, time-up after the first finisher, rejoin after
// results, rate limits, and the static file server (the built game).
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { createMpClient } from './net/mp-client.js';

const port = 18000 + Math.floor(Math.random() * 1000);
const staticRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx3-mp-static-'));
fs.writeFileSync(path.join(staticRoot, 'index.html'), '<!doctype html><title>SSX 3</title>');
fs.mkdirSync(path.join(staticRoot, 'assets')); fs.writeFileSync(path.join(staticRoot, 'assets', 'data.bin'), Buffer.from([...Array(100).keys()]));
const server = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1', '--static', staticRoot],
  { cwd: new URL('.', import.meta.url).pathname, stdio: ['ignore', 'pipe', 'inherit'], env: { ...process.env, MP_FINISH_GRACE_MS: '1500', MP_RESUME_MS: '4000', MP_PLAUSIBILITY: 'off' /* finish plausibility: test-mp-plausibility.mjs */ } });
await new Promise((resolve) => server.stdout.once('data', resolve));
const url = `ws://127.0.0.1:${port}/mp`, http = `http://127.0.0.1:${port}`;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
// Register a wait before the action that triggers its message: a message that arrives with no listener is gone.
const wait = (client, type, pred = () => true, ms = 5000) => new Promise((resolve, reject) => { const t = setTimeout(() => reject(new Error(`timeout waiting for ${type}`)), ms); const off = client.on(type, (v) => { if (pred(v)) { clearTimeout(t); off(); resolve(v); } }); });
const status = async () => (await fetch(`${http}/mp/status`)).json();
const make = (name, token, extra = {}) => createMpClient({ url, name, rider: 'zoe', pkg: 'RIDER_ZOE', token, ...extra });
try {
  // ---- version check ----
  const old = make('Old', 'tok-old', { version: 1 });
  const refused = wait(old, 'status', (s) => s === 'version'); await old.connect(); await refused;
  assert.equal(old.state.status, 'version'); assert.match(old.state.error, /reload/);

  // ---- lobbies, join, resume after reload ----
  const a = make('Alice', 'tok-a', { pair: { weight_attribute: 70, collision_stat: 0.1, attack_stat: 0.2 }, outfit: 'w1:3,17,42' }), b = make('Bob', 'tok-b', { rider: 'brodi', pkg: 'RIDER_BRODI', base: 'psymon', outfit: '<script>' });
  await a.connect(); await b.connect();
  const listedP = wait(b, 'lobbies', (ls) => ls.some((l) => l.name === 'Snow Jam run'));
  a.create({ name: 'Snow Jam run', course: 'ARA1' });
  const lobbyA = await wait(a, 'lobby', (l) => l);
  assert.equal(lobbyA.members.length, 1); assert.equal(lobbyA.hostId, a.state.id); assert.ok(a.isHost);
  assert.equal((await listedP).find((l) => l.id === lobbyA.id).host, 'Alice');
  b.join(lobbyA.id);
  const lobbyB = await wait(b, 'lobby', (l) => l && l.members.length === 2);
  assert.deepEqual(lobbyB.members.map((m) => m.name), ['Alice', 'Bob']); assert.ok(!b.isHost);
  assert.deepEqual(lobbyB.members[0].pair, { weight_attribute: 70, collision_stat: 0.1, attack_stat: 0.2 }); assert.equal(lobbyB.members[0].outfit, 'w1:3,17,42');
  assert.deepEqual([lobbyB.members[1].rider, lobbyB.members[1].pkg, lobbyB.members[1].base, lobbyB.members[1].outfit], ['brodi', 'RIDER_BRODI', 'psymon', null], 'a skin with its base rider; a malformed outfit key is dropped');
  const changed = wait(a, 'lobby', (l) => l.members[1].outfit === 'sam:RIDER_SAM_B'); b.setProfile({ rider: 'sam', pkg: 'RIDER_SAM', base: null, outfit: 'sam:RIDER_SAM_B' });
  assert.equal((await changed).members[1].base, null, 'a profile update carries base and outfit');
  b.ready(true); await wait(a, 'lobby', (l) => l.members[1].ready);
  b.disconnect(); await sleep(200);
  const b2 = make('Bob', 'tok-b'); await b2.connect();
  const resumed = await wait(b2, 'lobby', (l) => l && l.members.length === 2);
  assert.ok(b2.state.resumed); assert.equal(resumed.id, lobbyA.id);

  // ---- start -> loaded -> synchronized go; relay only after GO ----
  a.start();
  const [raceA, raceB] = await Promise.all([wait(a, 'start'), wait(b2, 'start')]);
  assert.equal(raceA.id, raceB.id); assert.equal(raceA.course, 'ARA1'); assert.equal(raceA.players.length, 2);
  assert.deepEqual([a.state.slot, b2.state.slot], [0, 1]);
  a.sendState(new Uint8Array([2, 1, 2, 3])); await sleep(100);
  assert.equal((await status()).binary.relayed, 0, 'no relay before GO');
  a.loaded(); b2.loaded();
  const [goA, goB] = await Promise.all([wait(a, 'go'), wait(b2, 'go')]);
  assert.equal(goA, goB); assert.ok(goA > a.serverNow());
  assert.ok(Math.abs(a.localTimeOf(goA) - b2.localTimeOf(goB)) < 50, 'both clients map GO to the same local moment');
  await sleep(goA - a.serverNow() + 20);
  const got = wait(b2, 'state'); a.sendState(new Uint8Array([2, 9, 8, 7])); assert.deepEqual([...await got], [0, 2, 9, 8, 7]);
  const attack = wait(a, 'state'); b2.sendState(new Uint8Array([3, 0, 1, 0]), { essential: true }); assert.deepEqual([...await attack], [1, 3, 0, 1, 0]);
  a.sendState(new Uint8Array([7, 1])); await sleep(80); assert.ok((await status()).binary.dropped >= 1, 'unknown packet kinds are not relayed');

  // ---- late joiner waits; a dropped socket reconnects by itself into the race ----
  const c = make('Carol', 'tok-c'); await c.connect(); c.join(lobbyA.id);
  const waitingLobby = await wait(c, 'lobby', (l) => l && l.members.length === 3);
  assert.ok(waitingLobby.members.find((m) => m.name === 'Carol').waiting && waitingLobby.race, 'late joiner waits for the next race');
  const noRelay = wait(c, 'state', () => true, 300).then(() => false, () => true); a.sendState(new Uint8Array([2, 5])); assert.ok(await noRelay, 'a waiting member gets no rider packets');
  const away = wait(a, 'presence', (m) => m.slot === 1 && !m.online), back = wait(a, 'presence', (m) => m.slot === 1 && m.online);
  const welcome = wait(b2, 'welcome');
  b2.dropForTest(); await away;
  const w = await welcome; await back;
  assert.ok(w.resumed && w.race && w.race.slot === 1 && !w.race.finished, 'reconnected into the running race');
  const again = wait(a, 'state'); b2.sendState(new Uint8Array([2, 4])); assert.deepEqual([...await again], [1, 2, 4]);

  // ---- finish, time-up DNF after the first finisher, results ----
  // The server sends 'results' and then the race-free 'lobby' to every member at once (mp-server.mjs maybeResults), so both
  // listeners must exist before the finish: registered after `await resultsP`, Carol's lobby is lost whenever her
  // socket is read before Alice's (a busy test process wakes up with all three sockets readable).
  const resultsP = wait(a, 'results', () => true, 5000), afterP = wait(c, 'lobby', (l) => l && !l.race); // Bob never finishes: DNF after the grace (1.5 s here)
  a.finish(7000);
  const fin = await wait(b2, 'finished', (m) => m.slot === 0); assert.equal(fin.ticks, 7000);
  const results = await resultsP;
  assert.deepEqual(results.map((r) => [r.name, r.dnf, r.reason]), [['Alice', false, ''], ['Bob', true, 'time']]);
  const after = await afterP;
  assert.ok(after.members.every((m) => !m.waiting), 'after the results everyone is in the lobby');

  // ---- next race with the late joiner; leaving mid-race is DNF, the host migrates ----
  a.start();
  const [, , raceC] = await Promise.all([wait(a, 'start'), wait(b2, 'start'), wait(c, 'start')]);
  assert.equal(raceC.players.length, 3); assert.equal(c.state.slot, 2);
  a.loaded(); b2.loaded(); c.loaded(); await wait(c, 'go');
  const leftP = wait(b2, 'finished', (m) => m.slot === 0 && m.dnf && m.reason === 'left'), hostP = wait(b2, 'lobby', (l) => l && l.hostId === b2.state.id);
  a.leave(); await leftP; await hostP; assert.ok(b2.isHost, 'the host role moved to Bob mid-race');
  b2.finish(6500); c.finish(6400);
  const r2 = await wait(b2, 'results');
  assert.deepEqual(r2.map((r) => [r.name, r.dnf]), [['Carol', false], ['Bob', false], ['Alice', true]]);

  // ---- rate limits ----
  let chats = 0; const off = c.on('chat', () => chats++);
  for (let i = 0; i < 30; i++) b2.chat('spam ' + i);
  await sleep(300); off(); assert.ok(chats >= 1 && chats <= 6, `chat is rate limited (${chats} of 30 delivered)`);

  // ---- static files ----
  const index = await fetch(`${http}/`); assert.equal(index.status, 200); assert.match(index.headers.get('content-type'), /text\/html/);
  assert.equal(index.headers.get('cross-origin-embedder-policy'), null);
  const etag = index.headers.get('etag'); assert.equal((await fetch(`${http}/index.html`, { headers: { 'if-none-match': etag } })).status, 304);
  const part = await fetch(`${http}/assets/data.bin`, { headers: { range: 'bytes=10-19' } });
  assert.equal(part.status, 206); assert.deepEqual([...new Uint8Array(await part.arrayBuffer())], [10, 11, 12, 13, 14, 15, 16, 17, 18, 19]);
  assert.equal((await fetch(`${http}/..%2f..%2fetc%2fpasswd`)).status, 404);
  assert.equal((await fetch(`${http}/missing.js`)).status, 404);

  for (const x of [a, b2, c, old]) x.disconnect();
  console.log('multiplayer: lobbies, resume, version check, synchronized start, relay, late join, reconnect, DNF/time-up, host migration, results, rate limits, static files OK');
} finally { server.kill(); fs.rmSync(staticRoot, { recursive: true, force: true }); }
