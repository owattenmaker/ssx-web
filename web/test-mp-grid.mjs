// Online grid spots (web/net/grid-seed.js): a racer in countdown slot k starts on slot k's PS2 spot for its own body
// scale and stance, from recorded values only (lineups.json; grid-scales.json for the cheat-skin scales no computer
// rider has, tools/export_grid_scales.py).
//   1. The five computer riders of the Zoe anchor lineup (Snow Jam, Metro-City) rebuilt from the parts: their
//      recorded countdown states exactly.
//   2. Every selectable rider and cheat skin (on every base rider), in every slot, on both courses: a recorded spot
//      (the PS2 part of that slot and scale), the base rider's stance.
//   3. Cores: every rider with its own settings started on its seed in every slot of both courses sits exactly on
//      that spot; after an online race the rider's own seed is back.
import assert from 'node:assert/strict';
import { json, createTestRider } from './net/test-core.mjs';
import { onlineGridState } from './net/grid-seed.js';
import { skinName, f32key } from './lineup.js';
import { composeCheat, humanSettings } from './character-roster.js';
import fs from 'node:fs';

const COURSES = ['ARA1', 'BRA2'], data = {};
for (const course of COURSES) data[course] = { lineups: json(course + '/lineups.json'), gridScales: json(course + '/grid-scales.json'), npcDoc: json(course + '/npc-riders.json') };
const norm = (x) => JSON.parse(JSON.stringify(x)); // as the core receives it (JSON text: -0 reads as 0, as for the computer riders' seeds)

// 1. anchors
for (const course of COURSES) {
  const { lineups, gridScales, npcDoc } = data[course];
  lineups.anchor.values.forEach((v, k) => {
    const id = skinName(lineups, v), humanBase = v < 10 ? v : lineups.anchor.human_base, scale = lineups.skin[id]['ground.profile.body_scale'];
    const got = onlineGridState({ lineups, gridScales, rider: { id, kind: v < 10 ? 'rider' : 'cheat' }, humanBase, scale, slot: k + 1 });
    assert.deepEqual(norm(got.state), norm(npcDoc.riders[k].ground.state), `${course} slot ${k + 1} (${id}): the recorded countdown state`);
  });
}
console.log('anchor computer riders: countdown states rebuilt exactly (Snow Jam, Metro-City)');

// Every selectable rider's own settings (riders.json; a cheat skin composed over a base rider).
const roster = json('riders.json'), riders = roster.riders ?? roster;
const settingsDoc = (pkg) => { const f = new URL(`./public/assets/${pkg}/settings.json`, import.meta.url); return fs.existsSync(f) ? JSON.parse(fs.readFileSync(f)) : null; };
const bases = riders.filter((r) => r.kind === 'rider');
function character(entry, base = null) {
  if (entry.kind !== 'cheat') return settingsDoc(entry.package);
  const b = base && !['zoe', 'sam'].includes(base.id) ? settingsDoc(base.package) : null;
  return composeCheat(b, settingsDoc(entry.package));
}
const scaleOf = (doc, initial) => (doc?.settings?.original_animation?.scale ?? initial.original_animation.scale)[0];

// 2. every rider / skin x base / slot / course: a recorded PS2 spot
let combos = 0;
for (const course of COURSES) {
  const { lineups, gridScales } = data[course], initial = json('ANIMATIONS/initial.json');
  for (const entry of riders) for (const base of entry.kind === 'cheat' ? bases : [null]) {
    const doc = character(entry, base), scale = Math.fround(scaleOf(doc, initial)), humanBase = entry.kind === 'cheat' ? base.character : entry.character;
    for (let slot = 0; slot <= 5; slot++) {
      const got = onlineGridState({ lineups, gridScales, rider: { ...entry, base: base?.id }, humanBase, scale, slot });
      assert.ok(got?.exact, `${course} ${entry.id}${base ? ' on ' + base.id : ''} slot ${slot}: a recorded spot (scale ${f32key(scale)})`);
      if (slot > 0) {
        const part = lineups.grid[slot][f32key(scale)] ?? gridScales.grid[slot][f32key(scale)];
        assert.deepEqual(got.state.position, part['ground.state.position']);
      }
      assert.equal(got.state.reverse_stance, lineups.human_base[String(humanBase)].reverse_stance, 'the base rider\'s stance');
      combos++;
    }
  }
}
console.log(`${combos} rider x base x slot x course combinations: every one on a recorded PS2 spot`);

// 3. cores started on the seeds: exactly on the spot
let starts = 0;
for (const course of COURSES) {
  const { lineups, gridScales } = data[course], initial = json(course === 'ARA1' ? 'ANIMATIONS/initial.json' : course + '/initial.json');
  for (const entry of riders) {
    const doc = character(entry), settings = humanSettings(initial, doc), scale = Math.fround(settings.original_animation.scale[0]);
    const r = await createTestRider({ course, rider: entry.package, settings });
    const at = () => Array.from(new Float32Array(r.core.HEAPF32.buffer, r.core._rider_world_state(), 3));
    const seed = (text) => { const b = Buffer.from(text + '\0'), p = r.core._malloc(b.length); r.core.HEAPU8.set(b, p); r.core._human_grid_seed(p); r.core._free(p); };
    for (let slot = 0; slot <= 5; slot++) {
      const { state } = onlineGridState({ lineups, gridScales, rider: entry, humanBase: entry.kind === 'cheat' ? 4 : entry.character, scale, slot }); // a skin on Zoe (main.js's default base)
      seed(JSON.stringify(state)); r.startEvent();
      assert.deepEqual(at(), state.position.map(Math.fround), `${course} ${entry.id} slot ${slot}: starts exactly on its spot`);
      starts++;
    }
    seed('');
    if (course === 'ARA1' && doc?.settings?.original_event_start) { // the rider's own seed is back after the online race
      r.startEvent(); const own = doc.settings.original_event_start.state.position;
      assert.deepEqual(at(), own.map(Math.fround), `${entry.id}: off-line again on its own spot`);
    }
  }
}
console.log(`online grid spots: ${starts} core starts exactly on their PS2 spots, own seed restored after the race OK`);
