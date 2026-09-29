// A character's countdown state (RIDER_*/settings.json original_event_start) was recorded on Snow Jam; on every other
// course the human must start at that course's gate (web/animation_bridge.cpp human_event_seed_for_course), not at
// Snow Jam's coordinates (the Superpipe spawned Sam / Psymon in mid-air there). Snow Jam keeps the recorded state.
import assert from 'node:assert/strict';
import { createTestRider, json } from './net/test-core.mjs';
import { humanSettings } from './character-roster.js';

// ABC1 / DBC2 / EBC3 are rolling starts: start_event runs the Continue's 11D390 placement (web/start_gameplay.inc), which must
// leave every character grounded at the course's grid row.
const courses = ['ARA1', 'BRA2', 'BHP1', 'ABC1', 'ASS1', 'ABA1', 'DBC2', 'EBC3'], riders = ['RIDER_PSYMON', 'RIDER_MAC', 'RIDER_ELISE', 'RIDER_MOBY'];
const initialOf = (c) => json(c === 'ARA1' ? 'ANIMATIONS/initial.json' : `${c}/initial.json`);
const start = async (course, pkg) => {
  const character = pkg ? json(`${pkg}/settings.json`) : null;
  const r = await createTestRider({ course, settings: humanSettings(initialOf(course), character) }); r.startEvent();
  const s = r.f32(r.core._rider_state(), 16); return { pos: [s[0], s[1], s[2]], grounded: s[8] };
};
let checked = 0;
for (const course of courses) {
  const zoe = await start(course, null);
  for (const pkg of riders) {
    const me = await start(course, pkg), d = Math.hypot(...me.pos.map((v, k) => v - zoe.pos[k]));
    assert.ok(d < 2, `${pkg} on ${course}: ${d.toFixed(1)} m from the course's grid spot (Zoe's)`); // the scale offset is < 0.5 m
    assert.equal(me.grounded, 1, `${pkg} on ${course} starts on the ground`);
    checked++;
  }
}
// Snow Jam: the recorded state is applied unchanged (the core's position = the state's, source cm -> browser metres).
const psy = json('RIDER_PSYMON/settings.json').settings.original_event_start.state.position, onSnowJam = await start('ARA1', 'RIDER_PSYMON');
assert.deepEqual(onSnowJam.pos.map((v) => Math.round(v * 100)), [Math.round(psy[0]), Math.round(psy[2]), Math.round(-psy[1])]);
console.log(`course spawn: ${checked} character starts on ${courses.length} courses at their own gate; Snow Jam keeps the recorded state`);
