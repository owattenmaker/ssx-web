// The CTM Transport map (web/ctm-map.js, pv transportMap): the red routes against the executable's tables (0x206690 and its
// path lists), the peak run routes, the base-station states against the Map LUI, and what the Map LUI override shows on each
// Transport screen. The executable (local/disc/SLUS_207.72), the career tables and the Map LUI (fe-menus.json) are
// git-ignored: each part runs when its file is there.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { CtmMap, transportRedPaths, PEAK_PATHS, PEAK_RUN_PATHS, STATION_INDICATOR, OV_PAGE } from './ctm-map.js';
import { PATHS, PATH_RED, PATH_ORANGE, INDICATOR_STATE } from './fe-event-select.js';
import { pdaTemperature } from './ctm-pda.js';

let checks = 0; const ok = (c, m) => { assert.ok(c, m); checks++; };
const read = (p) => { const f = new URL(p, import.meta.url).pathname; return fs.existsSync(f) ? fs.readFileSync(f) : null; };
const LABEL = new Map(Object.entries(PATHS).map(([code, label]) => [label, code]));

// 1. The executable's tables.
const elf = read('../local/disc/SLUS_207.72');
const careerJson = read('./public/assets/CAREER/career.json');
if (elf) {
  const phoff = elf.readUInt32LE(0x1C), phn = elf.readUInt16LE(0x2C), phs = elf.readUInt16LE(0x2A), segs = [];
  for (let i = 0; i < phn; i++) { const o = phoff + i * phs; if (elf.readUInt32LE(o) === 1) segs.push({ off: elf.readUInt32LE(o + 4), va: elf.readUInt32LE(o + 8), size: elf.readUInt32LE(o + 16) }); }
  const at = (va) => { const s = segs.find((g) => va >= g.va && va < g.va + g.size); return s.off + va - s.va; };
  const u32 = (va) => elf.readUInt32LE(at(va)), str = (va) => { const o = at(va); return elf.subarray(o, elf.indexOf(0, o)).toString('latin1'); };
  const codes = (va, n) => Array.from({ length: n }, (_, k) => LABEL.get(str(u32(va + 4 * k))));
  // the per-peak route tables of 0x206690 (copied to its stack): 0x4713A8 / 0x471420 (6 each), 0x471488 (5)
  assert.deepEqual([codes(0x4713A8, 6), codes(0x471420, 6), codes(0x471488, 5)].map((l) => [...l].sort()), PEAK_PATHS.map((l) => [...l].sort())); checks++;
  // peak runs: 'peak1racepath' -> 0x4714A0 (3), 'peak1fspath' .. 'peak3fspath' -> the pairs at 0x4A26A8
  assert.deepEqual(codes(0x4714A0, 3), PEAK_RUN_PATHS['peak1-race']); checks++;
  const pairs = ['peak1-jam', 'peak2-race', 'peak2-jam', 'peak3-race', 'peak3-jam'];
  pairs.forEach((k, i) => { assert.deepEqual(codes(0x4A26A8 + 8 * i, 2), PEAK_RUN_PATHS[k], k); checks++; });
  // the goal groups ('peak<n>racegroup' / 'fsgroup'): loops over the goal list records (stride 0x6C) whose first field names
  // the route: lui v0 / addiu s1, v0, lo / addiu s2, s1, length in 0x206E80..0x207600, in the order p1 race, p1 fs, p2 race, ...
  const lists = []; let lui = null;
  for (let a = 0x206E80; a < 0x207600; a += 4) {
    const w = u32(a), op = w >>> 26, rs = (w >>> 21) & 31, rt = (w >>> 16) & 31, imm = w & 0xFFFF, simm = imm & 0x8000 ? imm - 0x10000 : imm;
    if (op === 0x0F) lui = { rt, v: imm << 16 };
    if (op === 0x09 && rt === 17 && rs === 2 && lui?.rt === 2) {
      const w2 = u32(a + 4); if (w2 >>> 26 !== 9 || ((w2 >>> 16) & 31) !== 18 || ((w2 >>> 21) & 31) !== 17) continue;
      const base = (lui.v + simm) >>> 0, n = (w2 & 0xFFFF) / 0x6C;
      lists.push(Array.from({ length: n }, (_, k) => LABEL.get(str(base + 0x6C * k))));
    }
  }
  ok(lists.length === 6, `six goal-group lists (${lists.length})`);
  if (careerJson) {
    const career = JSON.parse(careerJson), courses = career.courses;
    let n = 0;
    for (const peak of [1, 2, 3]) for (const goal of ['race', 'freestyle']) {
      const g = career.rules.goal_events.find((x) => x.peak === peak && x.goal === goal);
      const goalLists = { [goal]: g.events.map((e) => ({ code: e.mode >= 6 ? null : courses[e.course]?.code })) };
      const red = transportRedPaths('ctm-goals', { peak, goal, goalLists });
      assert.deepEqual([...red].sort(), [...lists[n++]].sort(), `peak ${peak} ${goal}`); checks++;
    }
  } else console.log('career.json missing: goal-group check skipped');
  // the MCOMM badge (0x20A854 in the PDA template's init 0x20A778): -10 - (G+0x1C / 3600) % 15, '%d°%C' with 'C'
  const imm = (va) => u32(va) & 0xFFFF;
  ok(imm(0x20A854) === 0xE10 && imm(0x20A860) === 0xF && imm(0x20A868) === 0xFFF6 && imm(0x20A874) === 0x43, 'temperature constants');
} else console.log('SLUS_207.72 not present: executable checks skipped');
assert.deepEqual([0, 3599, 3600, 4 * 3600, 14 * 3600, 15 * 3600].map(pdaTemperature), [-10, -10, -11, -14, -24, -10]); checks++;

// 2. The red routes by screen.
assert.deepEqual([...transportRedPaths('ctm-goals', { peak: 2, goal: 'freeride' })].sort(), [...PEAK_PATHS[1]].sort()); checks++;
assert.deepEqual([...transportRedPaths('ctm-events', { peak: 1, focused: { code: 'ARA1' } })], ['ARA1']); checks++;
assert.deepEqual([...transportRedPaths('ctm-events', { peak: 1, focused: { key: 'peak1-race' } })], ['ABC1', 'ARA1', 'BRA2']); checks++;
ok(transportRedPaths('ctm-events', { focused: { code: 'A', station: true } }).size === 0, 'a station: no red route');
ok(transportRedPaths('ctm-peaks', {}).size === 0, 'Select Peak: no red route');

// 3. The Map LUI: station states and the override on each screen.
const feMenus = read('./public/assets/UI/fe-menus.json');
if (feMenus) {
  const screen = JSON.parse(feMenus).screens.Map, by = new Map(screen.elements.map((e) => [e.name, e]));
  const labelOf = new Map(screen.elements.filter((e) => e.label).map((e) => [e.label, e.name]));
  const stateProps = (frame) => new Map(screen.events.filter((ev) => ev.frame === frame && ev.props).map((ev) => [ev.element, ev.props]));
  const markers = ['0006fb81', '0006fb82', '0006fb83', '0006fb84', '0006fb85'];
  const shown = {};
  for (const [course, frame] of Object.entries(STATION_INDICATOR)) {
    const p = stateProps(frame), on = markers.filter((m) => p.get(m)?.[13] === 255);
    ok(p.get(labelOf.get('indicator'))?.[13] === 0, `state ${frame}: the start indicator hidden`);
    ok(on.length === 1, `state ${frame}: one station icon`);
    shown[course] = { name: on[0], y: p.get(on[0])[1], group: by.get(on[0]).parent };
  }
  // top to bottom as the All Peak Race splits: E, C, D, A, B (Green above Blue on Peak 1, Yellow above Red on Peak 2)
  ok(shown[17].group === shown[18].group && shown[17].y < shown[18].y, 'Green (A) above Blue (B)');
  ok(shown[19].group === shown[20].group && shown[19].y < shown[20].y, 'Yellow (C) above Red (D)');
  ok(by.get(shown[17].group).label === 'peak0indicators' && by.get(shown[19].group).label === 'peak1indicators' && by.get(shown[21].group).label === 'peak2indicators', 'station icons on their peak');
  for (const f of Object.values(INDICATOR_STATE)) ok(stateProps(f).get(labelOf.get('indicator'))?.[13] === 255, `course state ${f} shows the indicator`);
  // the override, without the canvas: the same chain / label maps ensure() builds
  const m = new CtmMap({});
  m.label = labelOf; m.lui = { byName: by, screen }; m.chain = new Map();
  for (const e of screen.elements) { const up = []; let p = e.parent || e.menu; while (p && by.has(p)) { up.push(p); const q = by.get(p); p = q.parent || q.menu; } m.chain.set(e.name, up); }
  const el = (label) => by.get(labelOf.get(label));
  const vis = (e, o) => { const r = m.override(e, { part: 'map', ...o }); return !(r && r.hidden); };
  const colour = (e, o) => { const r = m.override(e, { part: 'map', ...o }); return r?.props ? [r.props[27], r.props[28], r.props[29]] : null; };
  const ev = { s: 'ctm-events', peak: 1, index: 0, red: new Set(['ARA1']), indicator: 230, tab: 'Race', showTab: true, tabText: 'Show INFO' };
  assert.deepEqual(colour(el('ara1path'), ev), PATH_RED); assert.deepEqual(colour(el('abc1path'), ev), PATH_ORANGE); checks += 2;
  ok(!vis(el('dbc2path'), ev) && !vis(el('ebc3path'), ev), 'other peaks\' routes hidden');
  ok(vis(el('indicator'), ev) && !vis(el('indicator'), { ...ev, indicator: null }), 'the indicator only with a course state');
  ok(vis(el('MapPic'), ev) && m.override(el('MapPic'), { part: 'map', ...ev }).sprite.page === 'MAPGFX_map_peakA', 'the peak picture');
  ok(m.override(el('PEAKno'), { part: 'map', ...ev }).text === 'Race' && m.override(el('maptab text'), { part: 'map', ...ev }).text === 'Show INFO', 'the tabs');
  const info = { ...ev, info: true, tabText: 'Show MAP' };
  ok(!vis(el('MapPic'), info) && !vis(el('ara1path'), info) && m.override(el('maptab text'), { part: 'map', ...info }).text === 'Show MAP', 'INFO: no map, Show MAP');
  ok(!vis(el('maptab text'), { ...ev, showTab: false }), 'All Mountain / Earnings: no maptab');
  const peaks = { s: 'ctm-peaks', peak: 1, index: 1, focusPeak: 2, herePeak: 1, locked: [true, true, false], showTab: true, tabText: 'Show INFO' };
  const outline = (n) => by.get(el(`peak${n}outline`).parent);
  ok(!(m.override(outline(2), { part: 'map', ...peaks })?.hidden) && m.override(outline(1), { part: 'map', ...peaks })?.hidden && m.override(outline(3), { part: 'map', ...peaks })?.hidden, 'Select Peak: the focused peak\'s outline');
  const here = (n) => screen.elements.find((e) => m.chain.get(e.name)?.includes(labelOf.get(`P${n}you are here`)) && e.kind === 'text');
  ok(vis(here(1), peaks) && !vis(here(2), peaks) && !vis(here(3), peaks), '"You are here" on the peak being ridden');
  ok(!vis(el('ara1path'), peaks) && !vis(el('PEAKno'), peaks), 'Select Peak: no routes, no peak tab');
  ok(vis(by.get('007359e3'), peaks) && vis(by.get('007359e2'), peaks) && !vis(by.get('007359e1'), peaks) && !vis(by.get('007359e0'), peaks), 'locks on Peak 3 / Peak 2');
  const title = m.override(screen.elements.find((e) => e.label === 'SubTitle'), { part: 'title', sub: 'Select Peak' });
  ok(title.text === 'Select Peak' && m.override(el('MapPic'), { part: 'title' }).hidden, 'the title part: pdatitle only');
  ok(Object.keys(OV_PAGE).every((p) => screen.elements.some((e) => e.sprite?.page === p)), 'the FE pages the OV pages replace are the Map\'s');
} else console.log('fe-menus.json missing: Map LUI checks skipped (python3 tools/export_fe_menus.py)');
console.log(`ctm-map: ${checks} checks passed`);
