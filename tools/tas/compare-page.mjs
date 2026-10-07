// The node harness (tools/tas/race.mjs) against the page (tools/tas/page-run.mjs): the same pad from the same start state, the
// human's rider state, the shared game RNG and every computer rider's position compared tick by tick with the page's dump.json.
//   node tools/tas/compare-page.mjs PAGE_RUN_DIR PAD.tas
import fs from 'node:fs';
import path from 'node:path';
import { createTasRace } from './race.mjs';
import { parse, toChannels } from './pad-format.mjs';

const [dir, padPath] = process.argv.slice(2);
const start = JSON.parse(fs.readFileSync(path.join(dir, 'start.json'), 'utf8'));
const dump = JSON.parse(fs.readFileSync(path.join(dir, 'dump.json'), 'utf8'));
const { frames } = parse(fs.readFileSync(padPath, 'utf8'));
const race = await createTasRace({ start });
const f32 = (c, p, n) => new Float32Array(c.HEAPF32.buffer, p, n);
const NAMES = [...Array.from({ length: 16 }, (_, i) => `state${i}`), ...Array.from({ length: 6 }, (_, i) => `rng${i}`)];
let first = -1;
const n = Math.min(dump.length, frames.length);
for (let t = 0; t < n; t++) {
  race.tick(toChannels(frames[t]));
  const row = [...race.state(), ...race.rng()];
  for (const npc of race.aiRace.racers.npcs) row.push(...f32(npc.core, npc.core._rider_state(), 3));
  const page = dump[t];
  const bad = row.findIndex((v, i) => !Object.is(Math.fround(v), Math.fround(page[i])) && !(i >= 16 && i < 22 && v >>> 0 === page[i] >>> 0));
  if (bad >= 0) {
    first = t;
    const name = NAMES[bad] ?? `npc${Math.floor((bad - 22) / 3) + 1}.${(bad - 22) % 3}`;
    console.log(`tick ${t}: first difference at ${name}: node ${row[bad]} page ${page[bad]}`);
    console.log(' node', row.slice(0, 22).join(' '));
    console.log(' page', page.slice(0, 22).join(' '));
    break;
  }
}
console.log(first < 0 ? `exact for ${n} ticks` : `diverges at tick ${first} of ${n}`);
