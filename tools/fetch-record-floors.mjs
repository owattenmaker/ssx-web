#!/usr/bin/env node
// Online records' time floors (docs/online-records.md "Anti-cheat floors"): the real-world individual-level records of SSX 3 from
// speedrun.com's public API, written to web/server/record-floors.json (read by web/server/records.mjs). Run by hand when the
// records move:   node tools/fetch-record-floors.mjs [--out web/server/record-floors.json]
// Per timed event (the five races and the three backcountry Rival Time courses; speedrun.com has no level for the score events):
// the fastest verified run of every racing category (Racing (Clean), (NMG), (NMG+), (No Restrictions); not 'Alternate'), on any
// platform. The floor is 0.9 x the fastest of them all: the port is bit-exact, so a glitch or shortcut the PS2 / GameCube runners use
// is open to a port player too, and the floor must not catch a run as fast as a real one.
import fs from 'node:fs';
const API = 'https://www.speedrun.com/api/v1', GAME = 'k6qwy96g';   // 'ssx3' redirects here
const FACTOR = 0.9;
// speedrun.com level -> the port's event key (career.js MODE: 0 race, 4 Rival Time; course codes from CAREER/career.json)
const LEVELS = { kwj7npnw: '0:ARA1', owo7zyo9: '0:BRA2', ewp7gekw: '0:CRA3', y9m5ly5d: '0:DRA4', '5920x3od': '0:ERA5', xd1jgxed: '4:ABC1', '5wkky82w': '4:DBC2', '29vmoj39': '4:EBC3' };
const RACING = ['Racing (Clean)', 'Racing (NMG)', 'Racing (NMG+)', 'Racing (No Restrictions)'];
const out = process.argv.includes('--out') ? process.argv[process.argv.indexOf('--out') + 1] : new URL('../web/server/record-floors.json', import.meta.url).pathname;
const get = async (path) => { const r = await fetch(API + path, { headers: { 'user-agent': 'ssx3-port record floors' } }); if (!r.ok) throw new Error(`${path}: ${r.status}`); return (await r.json()).data; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const platforms = Object.fromEntries((await get(`/games/${GAME}?embed=platforms`)).platforms.data.map((p) => [p.id, p.name]));
const levels = await get(`/games/${GAME}/levels`);
const events = {};
for (const level of levels) {
  const key = LEVELS[level.id]; if (!key) { console.warn('unmapped level', level.id, level.name); continue; }
  await sleep(700);   // the API asks for at most 100 requests a minute
  const boards = await get(`/levels/${level.id}/records?top=1&embed=players,category`);
  const best = {};
  for (const b of boards) {
    const category = b.category.data.name; if (!RACING.includes(category)) continue;
    const top = b.runs.map((x) => x.run).filter((run) => run.status?.status === 'verified').sort((a, c) => a.times.primary_t - c.times.primary_t)[0];
    if (!top) continue;
    const player = top.players.map((p) => b.players.data.find((x) => x.id === p.id)?.names?.international ?? p.name ?? p.id).join(', ');
    best[category] = { seconds: top.times.primary_t, platform: platforms[top.system.platform] ?? top.system.platform, emulated: !!top.system.emulated, player, date: top.date, url: top.weblink };
  }
  const fastest = Object.entries(best).sort((a, b) => a[1].seconds - b[1].seconds)[0];
  if (!fastest) { console.warn('no racing record for', level.name); continue; }
  events[key] = { level: level.name, levelId: level.id, source: `${API}/levels/${level.id}/records`, records: best,
    floorFrom: fastest[0], wr: fastest[1].seconds, floorTicks: Math.floor(FACTOR * fastest[1].seconds * 60) };
}
const doc = { source: 'speedrun.com API v1 (individual levels)', game: GAME, fetched: new Date().toISOString().slice(0, 10), factor: FACTOR,
  rule: 'a timed run under floorTicks race ticks (0.9 x the fastest verified racing record, any category, any platform) is stored flagged, not listed', events };
fs.writeFileSync(out, JSON.stringify(doc, null, 1) + '\n');
for (const [k, e] of Object.entries(events)) console.log(`${k} ${e.level}: WR ${e.wr} s (${e.floorFrom}, ${e.records[e.floorFrom].platform}${e.records[e.floorFrom].emulated ? ' emu' : ''}, ${e.records[e.floorFrom].player}) -> floor ${e.floorTicks} ticks (${(e.floorTicks / 60).toFixed(1)} s)`);
