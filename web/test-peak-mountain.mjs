// Connected Peak 1 mountain, browser rules that need no core (docs/peak-mountain.md): the region placement 11DE60 / 26B5E0
// (runtime kinds = exported kinds + 1, first-row fallback) against the PS2 Transport arrivals, the Session points (table
// 0x440770), the collectible lists of every location (table 0x43FA70 totals) and the merged set-piece / painter packages.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { regionRow, arrivalRow } from './free-ride.js';
import { SESSION_POINTS } from './career-ui.js';
import { PEAK1_COLLECT_TRACKS } from './stage-collect.js';

const root = new URL('./public/assets/PEAK1/', import.meta.url);
const json = (p) => JSON.parse(fs.readFileSync(new URL(p, root)));
const peak = json('peak.json');
const bankOf = (course) => { const code = peak.residency.find((r) => r.course === course).code; return json(`${code}/paths.json`).variants['0']; };

// PS2 local/ps2-capture/runs/peak1-arrive-<loc> (Transport -> Freeride arrivals): the placement record (source cm).
const ARRIVALS = { 5: [-103802.14, 70360.68], 8: [-83306.80, 66907.28], 1: [-230834.95, 136740.08], 11: [-268306.38, 127172.61] };
for (const [course, [x, y]] of Object.entries(ARRIVALS)) {
  const [index, kind] = arrivalRow(Number(course)), row = regionRow(bankOf(Number(course)), index, kind);
  assert.ok(Math.abs(row.position[0] - x) < 1 && Math.abs(row.position[1] - y) < 1, `arrival ${course}: row ${row.position} vs PS2 ${x}, ${y}`);
}
// Stations: 11DE60(rider, 0, 2) = the first row (A: kind 1 index 0 = the lodge "No" ride-in, PS2 fr/no-a spawn ~(-65730, 33400)).
{ const row = regionRow(bankOf(17), ...arrivalRow(17)); assert.ok(Math.abs(row.position[0] + 65782) < 1 && Math.abs(row.position[1] - 33404) < 1); }
// The peak runs' backcountry start: 11DE60(rider, 0, 1) = exported kind 0 index 0 = the ABC1 grid slot (1883.2, -45983.4, 96369.2).
{ const row = regionRow(bankOf(14), ...arrivalRow(14)); assert.deepEqual(row.position.map((v) => Math.round(v)), [1883, -45983, 96369]); }
// Session: menu item k stores k + 1 (208840 item +0x18 -> overlay +0xD8 -> P[0xA]+0x10), placed by 11DE60(rider, k + 1, 2): every
// course has exactly its count of kind-1 rows 1..count (Top of run = point 1, the entry; Bottom of run = the last).
for (const [course, n] of Object.entries(SESSION_POINTS).filter(([c]) => peak.residency.some((r) => r.course === Number(c)))) { const bank = bankOf(Number(course));   // this world's courses (Peak 2 / 3: their own tests)
  for (let k = 0; k < n; k++) { const row = regionRow(bank, k + 1, 2); assert.ok(row.kind === 1 && row.index === k + 1, `session ${course} item ${k}`); } }
assert.deepEqual(Object.fromEntries(Object.entries(SESSION_POINTS).filter(([c]) => peak.residency.some((r) => r.course === Number(c)))), { 14: 7, 17: 1, 0: 7, 5: 7, 8: 2, 18: 1, 1: 8, 11: 2 });
// Collectible lists (builtin 38) of every location's stage = the HUD totals of table 0x43FA70 of its course.
const TOTALS = { 0: 30, 1: 35, 5: 30, 8: 2, 11: 4, 14: 44, 17: 5, 18: 5 };
const stage = json('SETPIECES/stage-world.json');
for (const [key, rows] of Object.entries(stage.collections)) {
  const track = Number(key.split(':')[1]), course = PEAK1_COLLECT_TRACKS[track];
  assert.equal(rows.length, TOTALS[course], `collection ${key}`);
}
assert.equal(Object.keys(stage.collections).length, 8);
// Painters: every location has a Lighting section whose banks are in lighting-banks.json; the 22E180 defaults.
const banks = json('lighting-banks.json');
assert.equal(banks.defaults['0'], 'APBR1'); assert.equal(banks.defaults['18'], 'BPBR1'); assert.equal(banks.defaults['1'], 'BPBR1');
for (const l of peak.locations) {
  const lighting = json(`${l.code}/lighting.json`).painter; assert.ok(lighting, `${l.code} lighting`);
  for (const e of lighting.entries) for (const r of e.references.slice(0, 3)) assert.ok(banks.banks[r], `${l.code} ${r}`);
}
// Light glows: every source belongs to a Peak 1 location track.
const tracks = new Set(peak.locations.map((l) => l.track));
assert.ok(json('LIGHT_GLOW/light-glow.json').lights.every((g) => tracks.has(g.track)));
console.log('peak mountain: arrivals vs PS2 (4 courses), station / backcountry rows, session points, 8 collectible lists, painters, light glows OK');
