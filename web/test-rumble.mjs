// Pad vibration (web/rumble.js) against the PS2: replays the core's rumble events (AE_RUMBLE_IMPACT 30 /
// AE_RUMBLE_SLIDE 31, web/audio_events.hpp) of a capture run through the 0x125B18 model and checks v0 against the
// recorded owner +0xDFC and v1 against +0xE00 on every tick, and the motor levels derived from both.
// The rumble captures are derived from the mix-glide / air-tricks scripts with --watch owner+0xDFC:8 (the default
// record window owner_de0_e00 stops at +0xDFF):
//   python3 tools/ps2_capture.py build local/reference/pcsx2/snow-jam-glide.p2s local/ps2-capture/scripts/mix-glide.json \
//     local/ps2-capture/runs/rumble/mix-glide-rumble.p2s --isolate --watch 0x147018c:8        (owner 0x146F390)
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import { Rumble, rumbleMotors, AE_RUMBLE_IMPACT, AE_RUMBLE_SLIDE } from './rumble.js';

{ // model: max-raise / set, then the per-frame decay (single precision); motors
  assert.deepEqual(rumbleMotors(0, 0), { strong: 0, weak: 0 }); assert.equal(rumbleMotors(974.2222, 0).strong, 1);
  assert.equal(rumbleMotors(101, 0).weak, 1); assert.equal(rumbleMotors(99, 0).weak, 0); assert.equal(rumbleMotors(0, 70.5).weak, 1);
  const r = new Rumble(() => null);
  r.update([[AE_RUMBLE_IMPACT, 4989.197]]); assert.ok(Math.abs(r.v0 - 4538.28) < 0.01, 'mix-glide 617 crash entry');
  r.update([[AE_RUMBLE_IMPACT, 100]]); assert.ok(r.v0 > 4000, 'a smaller hit does not lower v0');
  r.update([[AE_RUMBLE_SLIDE, 0.376]]); assert.ok(Math.abs(r.v1 - 0.373) < 5e-4, 'mix-glide 648 slide');
  r.update([[AE_RUMBLE_SLIDE, 0.2]]); assert.ok(Math.abs(r.v1 - 0.2 * 0.9916667) < 1e-6, 'v1 is set, not raised');
  for (let k = 0; k < 400; k++) r.update(); assert.equal(r.v0, 0);
  r.reset(); assert.equal(r.v0 + r.v1, 0);
}
{ // Gamepad dual-rumble; Vibration Off stops the motors
  const calls = [], act = { playEffect: (type, p) => { calls.push([type, p]); return Promise.resolve(); }, reset: () => { calls.push(['reset']); return Promise.resolve(); } };
  const q = new Float32Array(1 + 64 * 5); q[0] = 2; q.set([AE_RUMBLE_IMPACT, 3000, 0, 0, 0, 9, 2, 0, 0, 0], 1);
  const core = { HEAPF32: q, _audio_events: () => 0 };
  const r = new Rumble(() => ({ vibrationActuator: act })); calls.length = 0;
  r.tick(core, true); assert.equal(calls.at(-1)[0], 'dual-rumble'); assert.equal(calls.at(-1)[1].weakMagnitude, 1); assert.equal(q[0], 2, 'the queue is left for web/sfx-game.js');
  r.tick(core, false); assert.equal(calls.at(-1)[0], 'reset'); const n = calls.length; r.tick(core, false); assert.equal(calls.length, n);
}
const runs = [
  { name: 'rumble/mix-glide-rumble', args: ['--zoe'], v1: true, why: 'crash entry 617, ragdoll impacts, slide 648-690, get-up 691-738, landings' },
  { name: 'rumble/air-tricks-rumble', args: ['--zoe'], v1: true, why: 'trick landings, crash 722, slide and get-up' },
  { name: 'event-race', args: ['--zoe', '--event'], v1: false, why: 'race landings, two crashes, obstacle contacts (+0xE00 not recorded)' },
];
for (const run of runs) {
  const bin = `../local/ps2-capture/runs/${run.name}.bin`;
  if (!fs.existsSync(bin)) { console.warn('rumble: skipping', run.name, '(capture not present)'); continue; }
  const out = spawnSync(process.execPath, ['compare-ps2-capture.mjs', bin, '--pad', '--sync-rng', ...run.args], { env: { ...process.env, RUMBLE_TRACE: '1' }, encoding: 'utf8', maxBuffer: 1 << 28 });
  assert.equal(out.status, 0, out.stderr.slice(-2000));
  const rows = out.stderr.split('\n').filter((l) => l.startsWith('rumble ')).map((l) => l.split(' '));
  assert.ok(rows.length > 600, `${run.name}: ${rows.length} traced ticks`);
  const r = new Rumble(() => null), seen = { [AE_RUMBLE_IMPACT]: 0, [AE_RUMBLE_SLIDE]: 0 }; let on = 0;
  for (const p of rows) {
    const ev = JSON.parse(p[5]).map((e) => [e[0], e[1]]); for (const [t] of ev) if (t in seen) seen[t]++;
    const m = r.update(ev), dfc = +p[3];
    assert.ok(Math.abs(r.v0 - dfc) <= Math.max(0.002, 1e-3 * dfc), `${run.name} tick ${p[1]}: v0 ${r.v0} vs +0xDFC ${dfc}`);
    if (run.v1) {
      assert.equal(+p[11], dfc, `${run.name} tick ${p[1]}: the watch window is owner +0xDFC`);
      const e00 = +p[12]; assert.ok(Math.abs(r.v1 - e00) <= Math.max(0.002, 1e-3 * e00), `${run.name} tick ${p[1]}: v1 ${r.v1} vs +0xE00 ${e00}`);
      const ps2 = rumbleMotors(dfc, e00); if (ps2.strong) on++;
      assert.ok(Math.abs(ps2.strong - m.strong) < 2e-3 && ps2.weak === m.weak, `${run.name} tick ${p[1]}: motors ${JSON.stringify(m)} vs ${JSON.stringify(ps2)}`);
    } else if (rumbleMotors(dfc, 0).strong) on++;
  }
  assert.ok(seen[AE_RUMBLE_IMPACT] > 0 && seen[AE_RUMBLE_SLIDE] > 0, `${run.name}: impacts and slides exercised`);
  console.log(`rumble ${run.name}: ${rows.length} ticks exact (${seen[AE_RUMBLE_IMPACT]} impacts, ${seen[AE_RUMBLE_SLIDE]} slide/get-up sets, large motor on ${on} ticks) -- ${run.why}`);
}
console.log('rumble ok');
