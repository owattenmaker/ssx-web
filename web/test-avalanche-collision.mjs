// The avalanche pieces' collision (web/avalanche_gameplay.inc "Pieces' collision", docs/avalanche.md "Collision"): Much 2 Much
// (EBA3) replayed from the much-2-much-event-tuck capture (the same pad script as much-2-much-full up to its end, 3140), the
// five rocks' Object entities (instance flags, AvaSpline bounds / radius / matrix) against the kept much-2-much-full savestates,
// bit for bit, and the rider exact through the run; the rock-hit captures; the computer riders' definitions (parity-ai/era5). Skips
// without the local captures.
//   node test-avalanche-collision.mjs [CORE_JS=path/to/core.js]
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';

// The human's definitions survive its location name (core57 regression: keyed by it, they were lost at the race start when
// avalanches.json loaded before the world, or in a streamed world that renames the location with every append): ABC1 loaded
// in both orders, then a race reset and the QA trigger of avalanche 10.
{
  const createCore = (await import(process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js')).default;
  const read = (p) => fs.readFileSync(new URL(`public/assets/${p}`, import.meta.url));
  for (const worldFirst of [true, false]) {
    const core = await createCore(), put = (b) => { const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; }, str = (p) => put(Buffer.concat([read(p), Buffer.from([0])]));
    const ava = () => core._init_avalanches(str('ABC1/avalanches.json'), 0);
    core._init_animation(str('ANIMATIONS/animation-packets.json'), str('RIDER_ZOE/rider.json'), str('ABC1/initial.json'), put(read('ANIMATIONS/animation-packets.bin')), read('ANIMATIONS/animation-packets.bin').length);
    core._init_race(str('ABC1/initial.json'));core._animation_use_physics(1);
    if (!worldFirst) ava();
    const mesh = read('ABC1/collision.bin'), hash = put(Buffer.from(JSON.parse(read('ABC1/terrain.json')).source_sha256 + '\0'));
    core._init_world(put(mesh), mesh.length / 4);core._init_terrain(str('ABC1/terrain.json'));core._init_world_collision(str('ABC1/world_collision.json'), hash);
    if (worldFirst) ava();
    core._reset_race();
    assert.equal(core._avalanche_trigger_qa(10), 1, `ABC1 avalanche 10 after a race reset (avalanches loaded ${worldFirst ? 'after' : 'before'} the world)`);
  }
  console.log('avalanche collision: the definitions survive the location name (both load orders)');
}
const runs = new URL('../local/ps2-capture/runs/', import.meta.url).pathname;
const bin = runs + 'peak3/much-2-much-event-tuck.bin';
const ticks = [820, 1219, 1620, 2019, 2419, 2819].filter((t) => fs.existsSync(`${runs}peak3/much-2-much-full.tick${t}.p2s`));
if (!fs.existsSync(bin) || !ticks.length) { console.log('avalanche collision: skipped (local captures not present)'); process.exit(0); }
// PS2 words: each rock's instance (found by its resource at +0x78 and its authored translation at +0x40), instance+8, and the
// AvaSpline of its entity (instance+0xC -> +0x1C container -> +0 primary modifier: +0x10 / +0x20 bounds, +0x30 radius, +0x40 matrix).
const py = `
import json,struct,zipfile,sys
w=json.load(open('public/assets/EBA3/world_collision.json'))
rocks={(i['rid']<<8)|i['track']:i for i in w['instances'] if (i['rid']<<8)|i['track'] in (65065,184105,37929,3369,141353)}
out={};trails={}
for t in ${JSON.stringify(ticks)}:
  ee=zipfile.ZipFile('${runs}peak3/much-2-much-full.tick%d.p2s'%t).read('eeMemory.bin');out[t]={}
  for r,i in rocks.items():
    tx=struct.pack('<f',i['matrix'][12]);at=None;p=ee.find(tx)
    while p>=0:
      if p>=0x40 and struct.unpack_from('<I',ee,p-0x40+0x78)[0]==r: at=p-0x40;break
      p=ee.find(tx,p+1)
    if at is None: sys.exit('rock %d not found at %d'%(r,t))
    flags,entity=struct.unpack_from('<II',ee,at+8);mod=struct.unpack_from('<I',ee,struct.unpack_from('<I',ee,entity+0x1C)[0])[0]
    words=lambda o,n:list(struct.unpack_from('<%dI'%n,ee,mod+o))
    out[t][r]=dict(flags=flags,radius=words(0x30,1)[0],low=words(0x10,4),high=words(0x20,4),matrix=words(0x40,16))
  trails[t]={}
  for k in range(64): # the trails emitters (tumbler +0xD0) of the tumblers whose group has one
    tu=0x4EE770+k*0x2F0;g=struct.unpack_from('<I',ee,tu+736)[0]
    if not g or not ee[g+0xF2]: continue
    e=tu+0xD0;w=list(struct.unpack_from('<130I',ee,e));n=max(0,struct.unpack_from('<i',ee,e+0x178)[0]);ra,rb,rc=w[0x1A0//4],w[0x1A4//4],w[0x204//4]
    trails[t][k]=dict(words=w,ringA=list(struct.unpack_from('<%dI'%(4*n),ee,ra)),ringB=list(struct.unpack_from('<%dI'%(4*n),ee,rb)),colours=list(struct.unpack_from('<%dI'%n,ee,rc)))
print(json.dumps(dict(states=out,trails=trails)))`;
const fixture = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'avalanche-collision-')), 'fixture.json');
fs.writeFileSync(fixture, execFileSync('python3', ['-c', py], { cwd: new URL('.', import.meta.url).pathname, encoding: 'utf8' }));
const report = fixture.replace(/fixture\.json$/, 'report.json');
execFileSync(process.execPath, ['compare-ps2-capture.mjs', bin, '--pad', '--sync-rng', '--zoe', '--event', '--report', report], {
  cwd: new URL('.', import.meta.url).pathname, env: { ...process.env, STAGE_WORLD: '1', TICK_HOOK: 'avalanche-collision-hook.mjs', AVALANCHE_FIXTURE: fixture }, stdio: ['ignore', 'ignore', 'ignore'], maxBuffer: 1 << 28 });
const s = JSON.parse(fs.readFileSync(report, 'utf8')).summary;
assert.deepEqual(s.avalancheBad, [], 'rock entities as the PS2');
assert.equal(s.avalancheChecked, 5 * ticks.length, 'every rock at every kept state');
assert.deepEqual(s.trailsBad, [], 'trails emitters as the PS2');
assert.ok(s.trailsChecked >= 3, `trails emitters checked (${s.trailsChecked})`);
assert.equal(s.exactTicks, s.ticks, `rider exact through the run (first inexact ${JSON.stringify(s.firstInexact)})`);
console.log(`avalanche collision: ${s.avalancheExact} rock entities and ${s.trailsExact} trails emitters bit-exact at ${ticks.join(', ')}; rider exact over ${s.ticks} ticks`);
// The rock-hit captures (local/ps2-capture/scripts/avalanche-eba3-rock-hit*.json: the same run steered into rockslide_1001 after
// the trigger; the watches hold every rock's AvaSpline and instance header, at the heap addresses of this pad script's trigger):
// eba3-rock-hit passes it at 1.8 m (no contact), -a / -b hit it at 1113 / 1112 (105398 instance contact, the rigid response):
// the rocks bit-exact on every record, the rider exact to the end.
const watch = [[3369, 0x5A3400, 0xE40170], [37929, 0x5A5000, 0xE58980], [65065, 0x5A3700, 0xE6B6A0], [141353, 0x5A5900, 0xED3ED0], [184105, 0x5B4700, 0xEF0250]].map((w) => w.join(':')).join(',');
for (const name of ['eba3-rock-hit', 'eba3-rock-hit-a', 'eba3-rock-hit-b']) {
  const capture = `${runs}avalanche/${name}.bin`;
  if (!fs.existsSync(capture)) { console.log(`avalanche collision: ${name} skipped (capture not present)`); continue; }
  execFileSync(process.execPath, ['compare-ps2-capture.mjs', capture, '--pad', '--sync-rng', '--zoe', '--event', '--report', report], {
    cwd: new URL('.', import.meta.url).pathname, env: { ...process.env, STAGE_WORLD: '1', TICK_HOOK: 'avalanche-collision-hook.mjs', AVALANCHE_WATCH: watch, AVALANCHE_FIXTURE: '' }, stdio: ['ignore', 'ignore', 'ignore'], maxBuffer: 1 << 28 });
  const h = JSON.parse(fs.readFileSync(report, 'utf8')).summary;
  assert.deepEqual(h.avalancheBad, [], `${name}: rock entities as the PS2`);
  assert.ok(h.avalancheChecked > 5 * 1200, `${name}: every rock on every record after the trigger (${h.avalancheChecked})`);
  assert.equal(h.exactTicks, h.ticks, `${name}: rider exact (first inexact ${JSON.stringify(h.firstInexact)})`);
  console.log(`avalanche collision: ${name} ${h.avalancheExact} rock records bit-exact, rider exact over ${h.ticks} ticks`);
}
// The computer riders' contexts take the human's definitions (web/avalanche_gameplay.inc avalanche_sync): Gravitude with the five
// computer riders (parity-ai/era5: a computer rider sets avalanche 28 off at 1998), the avalanche plays in all six cores alike.
const era5 = `${runs}parity-ai/era5.bin`;
if (fs.existsSync(era5)) {
  execFileSync(process.execPath, ['compare-ai-capture.mjs', era5, '--world-draws', '--report', report, '--zoe', '--isolate'], { cwd: new URL('.', import.meta.url).pathname, stdio: ['ignore', 'ignore', 'ignore'], maxBuffer: 1 << 28 });
  const a = JSON.parse(fs.readFileSync(report, 'utf8')).summary.avalanches;
  assert.equal(a?.length, 6, 'six cores report their avalanches');
  assert.ok(a.every((c) => c[0] === 1 && c[1] === 1 && c[2] > 0 && JSON.stringify(c) === JSON.stringify(a[0])), `every core plays avalanche 28 alike: ${JSON.stringify(a)}`);
  console.log(`avalanche collision: parity-ai/era5 every core (human + 5) triggered once and stepped ${a[0][2]} ticks`);
} else console.log('avalanche collision: parity-ai/era5 skipped (capture not present)');
