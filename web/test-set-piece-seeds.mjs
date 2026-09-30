// The location Spline set pieces (web/generated/set_piece_seed_<LOC>.hpp, web/set_piece_gameplay.inc; docs/set-pieces.md
// "Regenerated location seeds") against the kept PS2 savestates of four event captures, every Spline / Position / MultiSpline
// entity at every kept state (web/set-piece-presence-hook.mjs):
// - the resident looping ravens and eagles whose own slot-1 program rebuilds them are destroyed at their section leave
//   (CHP2 ravensplineanima 203799 by 3218, EHP3 eaglesplineanima 19756 by 4018, CRA3 eaglesplineanima 332056 by 1218);
// - the blimps' rebuild programs (BHP1 fencecollision_1001 program 39, CHP2 96279 program 82) return at their builtin52 head,
//   the blimp being resident: no second blimp Spline;
// - launched pieces bit-exact (distance, translation), frozen ones as PositionModifiers, the trams / traffic MultiSplines exact.
// Skips without the local captures.
//   node test-set-piece-seeds.mjs [CORE_JS=path/to/core.js]
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
const web = new URL('.', import.meta.url).pathname, runs = new URL('../local/ps2-capture/runs/', import.meta.url).pathname;
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'set-piece-seeds-'));
// [capture, ticks compared through (the solo replay's exact span), expected destroyed resources]
const cases = [
  ['setpieces-bhp1/full', 4420, []],
  ['peak2/chp2-full', 3618, [203799]],
  ['peak3/perpendiculous-full', 4018, [19756]],
  ['peak2/cra3-full', 1218, [332056]], // the solo replay leaves the capture at 1384 (computer riders)
];
// Every live entity (Object 0x490E80, LiveComp 0x490B10, type 13 0x48EE60) whose instance points back at it and whose first
// modifier is a Spline / Position / MultiSpline modifier.
const py = `
import json,struct,sys,zipfile,glob,re
cap,through=sys.argv[1],int(sys.argv[2]);res=dict(splines={},multi={})
for f in glob.glob(cap+'.tick*.p2s'):
  t=int(re.search(r'tick(\\d+)',f).group(1))
  if t>through: continue
  ee=zipfile.ZipFile(f).read('eeMemory.bin');u=lambda a:struct.unpack_from('<I',ee,a&0x1FFFFFF)[0];spl={};multi={}
  for vt in (0x490E80,0x490B10,0x48EE60):
    pat=struct.pack('<I',vt);p=ee.find(pat,0x100000)
    while p>=0:
      if p%4==0:
        e=p-12;inst=u(e+0x18);c=u(e+0x1C)
        if 0x100000<inst<0x2000000 and u(inst+0xC)==e and 0x100000<c<0x2000000:
          m=u(c)
          if 0x100000<m<0x2000000:
            r=u(inst+0x78);k=u(m)
            if k==0x48F250: spl[r]=dict(kind='Spline',distance=u(m+0x3C),t=[u(m+0x90+4*j) for j in range(3)])
            elif k==0x48F5F0: spl[r]=dict(kind='Position')
            elif k==0x48F168: cl=u(m+0x44);multi[r]=dict(distance=u(m+0x10),matrices=[[u(u(cl+4*n)+0x10+4*j) for j in range(16)] for n in range(u(m+4))])
      p=ee.find(pat,p+1)
  res['splines'][t]=spl;res['multi'][t]=multi
print(json.dumps(res))`;
let ran = 0;
for (const [capture, through, destroyed] of cases) {
  const bin = `${runs}${capture}.bin`;
  if (!fs.existsSync(bin) || !fs.readdirSync(path.dirname(bin)).some((f) => f.startsWith(`${path.basename(capture)}.tick`))) { console.log(`set-piece seeds: ${capture} skipped (capture not present)`); continue; }
  const fixture = path.join(dir, `${capture.replace('/', '-')}.json`), report = fixture.replace(/\.json$/, '.report.json'), empty = path.join(dir, 'empty.snapshots.json');
  fs.writeFileSync(fixture, execFileSync('python3', ['-c', py, `${runs}${capture}`, String(through)], { encoding: 'utf8', maxBuffer: 1 << 28 }));
  fs.writeFileSync(empty, '{"snapshots":[]}'); // STAGE_WORLD_PS2: the stage world, and the run cut at an event restart (chp2-full restarts at 3882)
  const ps2 = JSON.parse(fs.readFileSync(fixture, 'utf8'));
  execFileSync(process.execPath, ['compare-ps2-capture.mjs', bin, '--pad', '--zoe', '--event', '--sync-rng', '--report', report], {
    cwd: web, env: { ...process.env, STAGE_WORLD: '1', STAGE_WORLD_PS2: empty, TICK_HOOK: './set-piece-presence-hook.mjs', SET_PIECE_PRESENCE: fixture }, stdio: ['ignore', 'ignore', 'ignore'], maxBuffer: 1 << 28 });
  const s = JSON.parse(fs.readFileSync(report, 'utf8')).summary;
  assert.deepEqual(s.presenceBad, [], `${capture}: set pieces as the PS2 (timeline ${JSON.stringify(s.presenceTimeline).slice(0, 1500)})`);
  const states = Object.keys(ps2.splines).length;
  assert.ok(states >= 3 && s.presenceChecked >= states, `${capture}: ${s.presenceChecked} pieces at ${states} states`);
  assert.ok(s.firstInexact == null || s.firstInexact.tick > through, `${capture}: rider exact through ${through} (first inexact ${JSON.stringify(s.firstInexact)?.slice(0, 200)})`);
  for (const r of destroyed) { // present at the first kept state, gone by the last one: the section-leave destroy happened in between
    const ticks = Object.keys(ps2.splines).map(Number).sort((a, b) => a - b);
    assert.ok(ps2.splines[ticks[0]][r] && !ps2.splines[ticks.at(-1)][r], `${capture}: PS2 ${r} destroyed within the compared span`);
  }
  ran++;
  console.log(`set-piece seeds: ${capture} ${s.presenceExact}/${s.presenceChecked} spline entities at ${states} PS2 states (${s.presenceLoops} resident loops), ${s.presenceMultiExact}/${s.presenceMultiChecked} MultiSplines exact${destroyed.length ? `; ${destroyed.join(', ')} destroyed at the section leave as on the PS2` : ''}`);
}
fs.rmSync(dir, { recursive: true, force: true });
if (!ran) console.log('set-piece seeds: skipped (local captures not present)');
