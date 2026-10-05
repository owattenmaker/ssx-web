// web/load-files.json (pv loadMeter: the load screens' download totals, web/load-files.mjs): the templating, and the recorded sums
// against public/assets (stale after an asset export: node load-files.mjs build).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { classify, courseTemplate, sums } from './load-files.mjs';

assert.equal(courseTemplate('/assets/BRA2/world.json', 'BRA2'), '/assets/{course}/world.json');
assert.equal(courseTemplate('/assets/AUDIO/banks/BRA2_slot8.bnk', 'BRA2'), '/assets/AUDIO/banks/{course}_slot8.bnk');
const c = classify(
  ['/assets/BRA2/world.json', '/assets/RIDER_MOBY/rider.json', '/assets/RIDER_ZOE/rider.json', '/assets/FX/fx.json', '/assets/main-abc12345.js'],
  'BRA2',
  'RIDER_ZOE'
);
assert.deepEqual(c, { files: ['/assets/FX/fx.json'], templates: ['/assets/{course}/world.json'], rider: ['rider.json'] });

const list = JSON.parse(fs.readFileSync(new URL('./load-files.json', import.meta.url), 'utf8'));
for (const k of ['course', 'event', 'rider', 'world']) assert.ok(list.sums[k], k);
if (!fs.existsSync(new URL('./public/assets/ARA1', import.meta.url))) {
  console.log('load files: templating OK; sums skipped (no game data)');
  process.exit(0);
}
const now = sums(list);
const bad = [];
for (const kind of Object.keys(now))
  for (const [k, v] of Object.entries(now[kind])) if (list.sums[kind]?.[k] !== v) bad.push(`${kind}.${k} ${list.sums[kind]?.[k]} -> ${v}`);
assert.deepEqual(bad, [], 'load-files.json is stale: node load-files.mjs build');
assert.ok(list.sums.course.ARA1 > 10e6, 'the Snow Jam course counts its files');
console.log('load files: templating and the recorded sums against public/assets OK');
