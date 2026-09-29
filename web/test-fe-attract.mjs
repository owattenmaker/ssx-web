// The front end's boot and attract movies (web/fe-attract.js, docs/intro-movies.md): the movie order, the skip rules, the
// DJ cut once, the 1801-frame title idle, and (with the executable present, local/disc/SLUS_207.72) the constants read
// from SLUS_207.72 itself.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createAttractState, nextMovie, bootSkip, automatedBrowser, IDLE_FRAMES, NTSC_HZ, MASK, BOOT_MASK, DEFAULT_BOOT_MASK, BOOT_START_MS } from './fe-attract.js';

const FRAME = 1000 / NTSC_HZ;
let checks = 0; const ok = (c, m) => { assert.ok(c, m); checks++; };

// 1. The mask walk of 0x1A27A0: EABIG, THX (not skippable), then the intro (skippable), the DJ cut only once.
{
  const a = createAttractState({ attract: true, boot: true });
  const played = [];
  for (let i = 0; i < 10; i++) { const m = a.step(FRAME, { idle: true }); if (m) { played.push(m); ok(a.key('Enter') === (m.skippable ? 'skip' : 'eat'), `${m.key}: Enter`); ok(a.key('Space') === (m.skippable ? 'skip' : 'eat'), `${m.key}: Space`); ok(a.key('ArrowDown') === 'eat', `${m.key}: other keys are eaten`); ok(a.key('Escape') === 'eat', `${m.key}: Triangle does not skip`); a.ended(); } }
  assert.deepEqual(played.map((m) => m.key), ['EABIG', 'THX', 'INTRO_DJ']);
  assert.deepEqual(played.map((m) => m.skippable), [false, false, true]);
  ok(a.state.mask === 0 && !a.state.djOnce, 'mask empty, DJ flag used');
  ok(a.key('Enter') === null, 'no movie: keys pass');
}
// 1b. The port's power-on (pv bootMovies, Owen 2026-09-27): the DJ intro only, skippable, marked as a boot movie; EA SPORTS BIG /
// THX stay behind the mask (?bootlogos=1 = the PS2's 7); after it the attract plays the plain intro.
{
  assert.equal(DEFAULT_BOOT_MASK, MASK.INTRO); assert.ok(BOOT_START_MS > 0 && BOOT_START_MS <= 2500); checks += 2;
  const a = createAttractState({ attract: true, boot: false });
  ok(a.step(FRAME, { idle: true }) === null, 'nothing before the boot');
  a.boot();
  const m = a.step(FRAME, { idle: true });
  ok(m.key === 'INTRO_DJ' && m.skippable && m.boot && m.fallback === 'INTRO', 'boot: the DJ intro, skippable, a boot movie');
  ok(a.key('Enter') === 'skip' && a.key('Space') === 'skip' && a.key('KeyR') === 'eat', 'the first Start / Cross skips; other keys eaten');
  a.ended(); ok(a.step(FRAME, { idle: true }) === null && a.state.mask === 0, 'then the title');
  let n = 0, next = null; while (!(next = a.step(FRAME, { idle: true })) && n < 5000) n++;
  ok(next.key === 'INTRO' && !next.boot, 'the attract after it: intro.mpc, not a boot movie');
  const logos = createAttractState({ attract: false, boot: false }); logos.boot(BOOT_MASK);
  const seq = []; for (let i = 0; i < 8; i++) { const x = logos.step(FRAME, { idle: true }); if (x) { seq.push([x.key, x.boot]); logos.ended(); } }
  assert.deepEqual(seq, [['EABIG', true], ['THX', true], ['INTRO_DJ', true]]); checks++;
}
// 1c. When the power-on intro is skipped: event / online links, ?qa=1, automated browsers (all unless ?femovies=1).
{
  const q = (s) => new URLSearchParams(s);
  const cases = [['', false, null], ['?course=ARA1&autostart=1', false, 'autostart'], ['?online=1', false, 'online'], ['?lobby=ab12', false, 'online'],
    ['?qa=1', false, 'qa'], ['?qa=1&femovies=1', false, null], ['', true, 'automated'], ['?femovies=1', true, null], ['?femovies=0', false, 'femovies=0'],
    ['?course=ARA1', false, null], ['?femovies=1&autostart=1', true, 'autostart']];
  for (const [s, automated, want] of cases) { assert.equal(bootSkip(q(s), { automated }), want, s); checks++; }
}
// 1d. Automated browsers: webdriver, ?mute=1 (testMuted) and headless Chrome (the npm test harness).
{
  ok(automatedBrowser({ webdriver: true, userAgent: '' }, false), 'webdriver');
  ok(automatedBrowser({ webdriver: false, userAgent: 'Mozilla/5.0 ... HeadlessChrome/140.0.0.0 Safari/537.36' }, false), 'headless Chrome');
  ok(automatedBrowser({ webdriver: false, userAgent: '' }, true), '?mute=1');
  ok(!automatedBrowser({ webdriver: false, userAgent: 'Mozilla/5.0 (Macintosh) AppleWebKit/605.1.15 Version/19.0 Safari/605.1.15' }, false), 'a player\'s Safari');
}
// 2. One frame between two movies (0x5ADC): the next one starts on the second step after the end.
{
  const a = createAttractState({ attract: false, boot: true });
  ok(a.step(FRAME)?.key === 'EABIG', 'first'); a.ended();
  ok(a.step(FRAME) === null, 'hold frame'); ok(a.step(FRAME)?.key === 'THX', 'next after one frame');
}
// 3. The title idle 0x1948A8: 1801 NTSC frames with no pad event -> the attract (intro.mpc); input resets; not while busy.
{
  const a = createAttractState({ attract: true, boot: false });
  let n = 0, m = null;
  while (!(m = a.step(FRAME, { idle: true })) && n < 5000) n++;
  ok(m?.key === 'INTRO' && m.skippable && m.fallback === 'INTRO', 'attract plays intro.mpc');
  ok(Math.abs(n + 1 - IDLE_FRAMES) <= 1, `attract after ${n + 1} frames (${IDLE_FRAMES})`);
  a.ended();
  for (let i = 0; i < 1000; i++) a.step(FRAME, { idle: true });
  a.input(); ok(a.state.frames === 0, 'a pad event resets the count');
  for (let i = 0; i < 3000; i++) ok(a.step(FRAME, { idle: false }) === null, 'not idle: no attract');
  ok(a.state.frames === 0, 'not counted while not idle');
  // a hidden tab / stall counts at most 100 ms per step
  a.step(60_000, { idle: true }); ok(a.state.frames < 7, 'long steps are clamped');
  const off = createAttractState({ attract: false, boot: false });
  for (let i = 0; i < 4000; i++) ok(off.step(FRAME, { idle: true }) === null, 'pv attract off: nothing');
}
// 4. The boot intro takes the DJ cut, a later attract the plain intro.
{
  assert.equal(nextMovie(MASK.INTRO, true).key, 'INTRO_DJ');
  assert.equal(nextMovie(MASK.INTRO, false).key, 'INTRO');
  assert.equal(BOOT_MASK, MASK.EABIG | MASK.THX | MASK.INTRO); checks += 3;
}
// 5. The executable (when present): 0x709 at 0x1948C4, the mask's initial 7, the path table 0x441248, the skip buttons.
const ELF = new URL('../local/disc/SLUS_207.72', import.meta.url).pathname;
if (fs.existsSync(ELF)) {
  const e = fs.readFileSync(ELF), phoff = e.readUInt32LE(0x1C), phn = e.readUInt16LE(0x2C), phs = e.readUInt16LE(0x2A), segs = [];
  for (let i = 0; i < phn; i++) { const o = phoff + i * phs; if (e.readUInt32LE(o) === 1) segs.push({ off: e.readUInt32LE(o + 4), va: e.readUInt32LE(o + 8), size: e.readUInt32LE(o + 16) }); }
  const at = (va) => { const s = segs.find((g) => va >= g.va && va < g.va + g.size); assert.ok(s, `address ${va.toString(16)}`); return s.off + va - s.va; };
  const u32 = (va) => e.readUInt32LE(at(va)), str = (va) => { const o = at(va); return e.subarray(o, e.indexOf(0, o)).toString('latin1'); };
  const imm = (va) => u32(va) & 0xFFFF;
  ok(u32(0x1948C4) >>> 26 === 0x0A && imm(0x1948C4) === IDLE_FRAMES, 'slti 0x709 at 0x1948C4');
  ok(u32(0x4A30F0 - 0x1724) === BOOT_MASK, 'gp-0x1724 starts at 7');
  assert.deepEqual([0, 1, 2, 3].map((k) => str(u32(0x441248 + 4 * k))), ['data\\movies\\eabig.mpc', 'data\\movies\\thx.mpc', 'data\\movies\\intro_dj.mpc', 'data\\movies\\intro.mpc']); checks++;
  ok(imm(0x1A29B8) === 0x70 && imm(0x1A29CC) === 0x7A, 'skip buttons UIStart 0x70 / UINext 0x7A (320C48)');
  ok(imm(0x1948D8) === MASK.INTRO, 'the idle sets bit 2');
} else console.log('SLUS_207.72 not present: executable checks skipped');
// 6. The exported files (git-ignored; tools/export_movies.py EABIG THX INTRO_DJ INTRO).
const index = new URL('./public/assets/MOVIES/movies.json', import.meta.url).pathname;
if (fs.existsSync(index)) {
  const keys = new Set(JSON.parse(fs.readFileSync(index, 'utf8')).movies.map((m) => m.key));
  const missing = ['EABIG', 'THX', 'INTRO_DJ', 'INTRO'].filter((k) => !keys.has(k));
  if (missing.length) console.log(`movies.json lacks ${missing.join(', ')} (python3 tools/export_movies.py ${missing.join(' ')}); INTRO_DJ falls back to INTRO, a missing logo is skipped`);
}
console.log(`fe-attract: ${checks} checks passed`);
