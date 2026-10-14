// The online pre-race protocol (pv mpPreRace, docs/multiplayer.md "Pre-race"): web/server/mp-server.mjs with node clients.
// A host that asks for it gets loaded -> {t:'intro', at} -> every racer's {t:'ready'} -> {t:'go'}; a page from before the switch
// counts as ready once loaded; a racer not ready READY_TIMEOUT_MS after the intro no longer holds the GO; a racer who leaves holds
// nothing; a host without the switch keeps the old start (loaded -> go, no intro).
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createMpClient } from './net/mp-client.js';

const port = 19000 + Math.floor(Math.random() * 1000);
const READY_TIMEOUT_MS = 1500;
const server = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1'], {
  cwd: new URL('.', import.meta.url).pathname,
  stdio: ['ignore', 'pipe', 'inherit'],
  env: { ...process.env, MP_READY_TIMEOUT_MS: String(READY_TIMEOUT_MS), MP_PLAUSIBILITY: 'off' }
});
await new Promise((resolve) => server.stdout.once('data', resolve));
const url = `ws://127.0.0.1:${port}/mp`;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
// a wait registered before the action whose message it expects
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
// resolves false when the message comes within ms, true when it does not
const none = (client, type, ms) => wait(client, type, () => true, ms).then(() => false, () => true);
let n = 0;
const make = (name, preRace) => createMpClient({ url, name, rider: 'zoe', pkg: 'RIDER_ZOE', token: `tok-pre-${name}-${n++}`, preRace });
async function lobbyOf(host, guests) {
  for (const c of [host, ...guests]) await c.connect();
  host.create({ name: 'pre-race', course: 'ARA1' });
  const lobby = await wait(host, 'lobby', (l) => l);
  const full = wait(host, 'lobby', (l) => l.members.length === guests.length + 1);
  for (const g of guests) {
    const joined = wait(g, 'lobby', (l) => l && l.id === lobby.id);
    g.join(lobby.id);
    await joined;
  }
  await full;
  return lobby;
}
async function start(host, all) {
  const races = all.map((c) => wait(c, 'start'));
  host.start();
  return Promise.all(races);
}
const closeAll = (all) => all.forEach((c) => c.disconnect());
try {
  // ---- two pre-race pages: intro on one instant, GO only after both Continue ----
  {
    const a = make('A', true), b = make('B', true);
    await lobbyOf(a, [b]);
    const [ra, rb] = await start(a, [a, b]);
    assert.equal(ra.preRace, true, "the host's start asks for the pre-race");
    assert.equal(rb.preRace, true);
    const intros = [wait(a, 'intro'), wait(b, 'intro')];
    a.loaded();
    assert.ok(await none(a, 'intro', 300), 'no intro before every racer loaded');
    b.loaded();
    const [ia, ib] = await Promise.all(intros);
    assert.equal(ia, ib, 'one intro instant for every racer');
    assert.ok(ia > a.serverNow(), 'the intro instant lies ahead of the message');
    const goA = wait(a, 'go');
    a.raceReady();
    assert.ok(await none(b, 'go', 400), 'no GO while a racer is still on its card');
    b.raceReady();
    const go = await goA;
    assert.ok(go > ia, 'the GO follows the intro');
    console.log('pre-race: two pages, intro then GO after both Continue');
    closeAll([a, b]);
  }
  // ---- a page from before the switch counts as ready once loaded ----
  {
    const a = make('A2', true), old = make('Old', false);
    await lobbyOf(a, [old]);
    await start(a, [a, old]);
    const intro = wait(a, 'intro');
    const oldGo = wait(old, 'go');
    a.loaded();
    old.loaded();
    await intro;
    a.raceReady();
    await oldGo;
    console.log('pre-race: an old page is ready on its load');
    closeAll([a, old]);
  }
  // ---- a racer not ready no longer holds the GO after READY_TIMEOUT_MS ----
  {
    const a = make('A3', true), b = make('B3', true);
    await lobbyOf(a, [b]);
    await start(a, [a, b]);
    const intro = wait(a, 'intro');
    a.loaded();
    b.loaded();
    const at = await intro;
    const go = wait(b, 'go', () => true, READY_TIMEOUT_MS + 4000);
    a.raceReady();
    const goAt = await go;
    assert.ok(goAt - at >= READY_TIMEOUT_MS, 'the GO waited for the ready timeout');
    console.log('pre-race: ready timeout');
    closeAll([a, b]);
  }
  // ---- a racer who leaves in the pre-race holds nothing ----
  {
    const a = make('A4', true), b = make('B4', true);
    await lobbyOf(a, [b]);
    await start(a, [a, b]);
    const intro = wait(a, 'intro');
    a.loaded();
    b.loaded();
    await intro;
    const go = wait(a, 'go', () => true, 1000);
    a.raceReady();
    b.leave();
    await go;
    console.log('pre-race: a racer leaving on its card');
    closeAll([a, b]);
  }
  // ---- a host without the switch: the old start, no intro ----
  {
    const a = make('A5', false), b = make('B5', true);
    await lobbyOf(a, [b]);
    const [ra] = await start(a, [a, b]);
    assert.equal(ra.preRace, undefined, 'no pre-race unless the host asks');
    const noIntro = none(b, 'intro', 3500);
    const go = wait(b, 'go');
    a.loaded();
    b.loaded();
    await go;
    assert.ok(await noIntro, 'no intro message in the old start');
    console.log('pre-race: a host without the switch keeps the old start');
    closeAll([a, b]);
  }
} finally {
  server.kill();
}
await sleep(50);
console.log('pre-race protocol OK');
