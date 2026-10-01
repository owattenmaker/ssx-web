// Conquer the Mountain audio across world switches, the post-event DJ commentary, the saved first-visit mask and the DJ queue
// rules (docs/ctm-decomp-freeride.md ranked 2, 9, 10, 12; docs/audio-logic.md 9.14). The director runs on a manual clock with the
// engine locked (songs are recorded, not played); the speech scheduler runs on the exported Events.evt, so posted DJ events are real.
// PS2 reference: local/ps2-capture/ctm-decomp/audio/postevent2 (the Transport after the Snow Jam final, Snow Jam -> Metro-City):
//   tick 15440 (the confirm): 28F520 stop, 28E8C0(20, 1), pick, speech 0x20E5 (Radio BIG intro), flush; 15442: request 2 -> Peak1
//   event 12; 15614 (WS11, the ride): speech 0x2102 (Char_Progress, from the pool-5 hub chatter); 15963: 28E8C0(19). No loading loop.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { setPv } from './pv-flags.js';

const root = new URL('./public', import.meta.url).pathname;
if (!fs.existsSync(root + '/assets/AUDIO/catalog.json')) { console.log('ctm audio: skipped (no exported audio)'); process.exit(0); }
const { createGameAudio } = await import('./game-audio.js');
const FREE = { kind: 4, mode: 12 };
let T = 1000;
const FRAME = 1000 / 60;
async function make(context = {}) {
  const ga = createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(root + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(root + p)) });
  await ga.whenReady(); await ga.speechEngine.init();
  ga.context = () => (typeof context === 'function' ? context() : context);
  return ga;
}
async function run(ga, ms) { for (const end = T + ms; T < end;) { T += FRAME; ga._director.pump(); ga.speechEngine.update(); await new Promise((r) => setImmediate(r)); } }
const kinds = (ga, from = 0) => ga.timeline().slice(from).map(([, k, a, b]) => [k, a, b].filter((x) => x !== undefined).join(' '));
const trace = (ga) => ga.debug().director.trace.map((l) => l.replace(/^\d+ /, ''));
const SWITCHES = ['postEventDj', 'djVisited', 'djQueueRules', 'mailFreeze'];

// ---- switches off: the shipped path (load-screen loop, world-load song, DJ kind 4, no commentary) -------------------------
{
  for (const k of SWITCHES) setPv(k, false);
  const ga = await make({ career: true, round: 3, mode: 0 });
  await ga.worldLoaded({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1', character: 'zoe' });
  await ga.runStart({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1', character: 'zoe' });
  ga.finish({ place: 0, stats: { ko: 9, ubers: 40 } });
  ga.leaveWorld(); ga.timelineReset();
  await ga.loadingStart({ courseCode: 'PEAK1', character: 'zoe' });
  await ga.freeWorldLoaded({ courseIndex: 1, courseCode: 'PEAK1', character: 'zoe', freeRide: FREE });
  const k = kinds(ga);
  assert.ok(k.includes('loading 1') && k.includes('worldload 1'), `off: the load path (${k.join(', ')})`);
}

// ---- The world switch + pv postEventDj: the post-event Transport (postevent2) -------------------------------------------------
for (const k of SWITCHES) setPv(k, true);
{
  const ctx = { career: true, round: 3, mode: 0 };
  const ga = await make(ctx);
  await ga.worldLoaded({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1', character: 'zoe' });
  await ga.runStart({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1', character: 'zoe' });
  // 287070 -> 2A45C0: the final won (place 0), no KOs, few Ubers: only Char_Progress is earned
  ga.finish({ place: 0, stats: { ko: 1, ubers: 3 } });
  assert.ok(trace(ga).includes('event record armed 1 final 1 place 0 hits 1 score 3'));
  await ga.screen('ctm-results');                   // 288AE0 + the podium's chartune
  assert.equal(ga.debug().music, 'chartune');
  assert.equal(ga.eventMap(), true); ga.leaveWorld(); // the results' Transport: the run quits under the map
  assert.equal(ga.debug().music, 'chartune', 'the podium song plays on under the map');
  ga.timelineReset(); const t0 = T;
  assert.equal(ga.travelSwitch(1), true);           // the confirm: Metro-City (course 1)
  let k = kinds(ga).filter((x) => !x.startsWith('carry'));
  assert.deepEqual(k.slice(0, 4).map((x) => x.replace(/^pick .*/, 'pick')), ['stop', 'code 20', 'pick', 'speech 20e5'], k.join(', '));
  await run(ga, 20);
  k = kinds(ga);
  const req = ga.timeline().find(([, kind, a]) => kind === 'request' && a === 2);
  assert.ok(req && req[0] - t0 <= 20, 'request kind 2 ~10 ms after the confirm');
  assert.ok(k.includes('play Peak1 12'), `Peak1 event 12 (${k.join(', ')})`);
  // the pool-5 hub chatter (2A4718) at the next idle edge: 2A4770's commentary first (the locked engine plays no line, so the edge
  // comes at once; the PS2 said it 174 ticks later, after the Radio BIG intro line)
  assert.ok(k.includes('commentary 0') && k.includes('speech 2102'), `Char_Progress (${k.join(', ')})`);
  assert.ok(k.indexOf('speech 2102') > k.indexOf('speech 20e5'));
  // the page's course switch: teardown, load screen, the streamed world; a pending DJ flag carries over it
  ga._director.djTimer(2); // (hub chatter pending, not yet taken: no speech update before the switch)
  const mark = ga.timeline().length;
  ga.leaveWorld();
  assert.equal(ga.debug().music, 'Peak1', 'the carried song survives the teardown');
  await ga.loadingStart({ courseCode: 'PEAK1', character: 'zoe' });
  await ga.freeWorldLoaded({ courseIndex: 1, courseCode: 'PEAK1', character: 'zoe', freeRide: FREE });
  const all = kinds(ga); k = kinds(ga, mark);
  assert.ok(!k.some((x) => x.startsWith('loading 1')), 'no LoadingScreen loop');
  assert.ok(!k.some((x) => x.startsWith('worldload')), 'no world load (2867E8 / 2A4A78)');
  assert.ok(!k.some((x) => x.startsWith('pick')), 'no world-load pick');
  assert.ok(!k.some((x) => x.startsWith('dj ')), 'no world-load DJ timer');
  assert.equal(ga.debug().director.pending.hubChatter, 1, 'the pending hub chatter carries over the switch');
  assert.equal(all.filter((x) => x === 'commentary 0').length, 1, 'the record is said once');
  // the arrival cut: 28E8C0(19); Metro-City is a course: nothing more
  await ga.runStart({ courseIndex: 1, courseCode: 'PEAK1', character: 'zoe', freeRide: FREE });
  assert.ok(kinds(ga).includes('code 19'));
  // the record is spent: the next hub chatter is a plain category
  ga._director.idle(); ga.leaveWorld();
}

// ---- 2A4770's choice rules -----------------------------------------------------------------------------------------------------
{
  const ctx = { career: true, round: 1, mode: 0 };
  const ga = await make(ctx);
  await ga.worldLoaded({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1', character: 'zoe' });
  // round 1 resets; rounds 1 / 2 only add (no final): KOs 3 + 3 (races only), Ubers 10 + 10 + 10
  ga.finish({ place: 2, stats: { ko: 3, ubers: 10 } }); ctx.round = 2;
  ga.finish({ place: 1, stats: { ko: 3, ubers: 10 } }); ctx.round = 3;
  ga.finish({ place: 4, stats: { ko: 0, ubers: 10 } });
  assert.ok(trace(ga).includes('event record armed 1 final 1 place 4 hits 6 score 30'));
  // not 1st: Aggression (>= 5 KOs) and High_Trick_Score (>= 27 Ubers at a race course); two earned -> random of the two
  const said = new Set();
  for (let i = 0; i < 40; i++) {
    await run(ga, 3100); ga.timelineReset(); // (the game request table's 180-frame life: 2B1458 admits one request per event)
    ctx.round = 3; ga.finish({ place: 4, stats: { ko: 0, ubers: 0 } }); // re-arm (running totals keep growing)
    ga.speechEngine.stop(); ga._director.djTimer(2); ga._director.idle(); // hub chatter pending -> 2A2E50
    const c = ga.timeline().find(([, kind]) => kind === 'commentary'); assert.ok(c, 'a commentary each time');
    said.add(c[2]);
    const sp = ga.timeline().filter(([, kind]) => kind === 'speech').map(([, , a]) => a);
    assert.ok(sp.includes(c[2] === 1 ? '212e' : '212f'), `speech for ${c[2]}: ${sp}`);
  }
  assert.deepEqual([...said].sort(), [1, 2], 'both earned lines come up, never Char_Progress');
  // with two earned the last one said is left out: never twice in a row
  let last = -1, repeats = 0;
  for (let i = 0; i < 30; i++) {
    ga.timelineReset(); ga.finish({ place: 4, stats: { ko: 0, ubers: 0 } }); ga.speechEngine.stop(); ga._director.djTimer(2); ga._director.idle();
    const c = ga.timeline().find(([, kind]) => kind === 'commentary')[2]; if (c === last) repeats++; last = c;
  }
  assert.equal(repeats, 0, 'the last commentary is avoided (+0x580C)');
  // not armed (a round-1 finish) -> the ordinary hub chatter
  ctx.round = 1; ga.finish({ place: 0, stats: { ko: 0, ubers: 0 } }); ga.timelineReset();
  ga.speechEngine.stop(); ga._director.djTimer(2); ga._director.idle();
  assert.ok(!ga.timeline().some(([, kind]) => kind === 'commentary'), 'a qualifier arms nothing');
  // armed but nothing earned, no travel pending (+0x6254 clear): falls through to the ordinary chatter
  ctx.round = 3; ga.finish({ place: 3, stats: { ko: 0, ubers: 0 } }); ga.timelineReset();
  ga.speechEngine.stop(); ga._director.djTimer(2); ga._director.idle();
  assert.ok(!ga.timeline().some(([, kind]) => kind === 'commentary'));
  ga.leaveWorld();
  // Big Air threshold 24: 24 Ubers earn High_Trick_Score at a Big Air course, not at a race course
  const g2 = await make({ career: true, round: 3, mode: 3 });
  await g2.worldLoaded({ courseIndex: 8, singleEvent: false, courseCode: 'ABA1', character: 'zoe' });
  g2.finish({ place: 3, stats: { ko: 9, ubers: 24 } }); g2.timelineReset();
  g2.speechEngine.stop(); g2._director.djTimer(2); g2._director.idle();
  assert.deepEqual(g2.timeline().filter(([, kind]) => kind === 'commentary').map((e) => e[2]), [2], 'Big Air: 24 Ubers; KOs count in races only');
  g2.leaveWorld();
}

// ---- nothing earned during a travel: by the destination ------------------------------------------------------------------------
{
  const ga = await make({ career: true, round: 1, mode: 4 });   // a rival challenge: one round, armed, never final
  await ga.worldLoaded({ courseIndex: 15, singleEvent: false, courseCode: 'DBC2', character: 'zoe' });
  ga.finish({ place: 0, stats: { ko: 0, ubers: 0 }, challengeKind: 5 });
  ga.eventMap(); ga.leaveWorld();
  ga.travelSwitch(19);                                            // Yellow Mid Station (a hub): 2A2E50(pool 0) itself
  ga.timelineReset();
  await run(ga, 3000);
  const k = kinds(ga);
  assert.ok(k.includes('commentary -1'), `nothing earned -> by the destination (${k.join(', ')})`);
  ga.leaveWorld();
}

// ---- crossWorld: carried, no code 20 -------------------------------------------------------------------------------------------
{
  const ga = await make({ career: true, round: 3, mode: null });
  await ga.runStart({ courseIndex: 3, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 3000);
  const song = ga.debug().music; ga.timelineReset();
  assert.equal(ga.travelSwitch(19, { cross: true }), true);
  ga.leaveWorld(); await ga.loadingStart({ courseCode: 'PEAK2', character: 'zoe' });
  await ga.freeWorldLoaded({ courseIndex: 19, courseCode: 'PEAK2', character: 'zoe', freeRide: FREE });
  await ga.runStart({ courseIndex: 19, courseCode: 'PEAK2', character: 'zoe', freeRide: FREE });
  const k = kinds(ga);
  assert.ok(!k.some((x) => /^(code|loading 1|worldload|stop)/.test(x)), `crossing: no director call, no load path (${k.join(', ')})`);
  assert.equal(ga.debug().music, song);
  ga.leaveWorld();
}

// ---- the page's ride start before its load-screen notification (main.js startRun, then audioScreens): one carried load, not two ---
{
  const ga = await make({ career: true, round: 3, mode: null });
  await ga.runStart({ courseIndex: 0, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 3000); ga.timelineReset();
  ga.travelSwitch(17); ga.leaveWorld(); await ga.loadingStart({ courseCode: 'MOUNTAIN', character: 'zoe' });
  await ga.runStart({ courseIndex: 17, courseCode: 'MOUNTAIN', character: 'zoe', freeRide: FREE });
  await ga.freeWorldLoaded({ courseIndex: 17, courseCode: 'MOUNTAIN', character: 'zoe', freeRide: FREE });
  const k = kinds(ga);
  assert.ok(k.includes('carried travel') && !k.some((x) => x.startsWith('worldload')), `no second world load (${k.join(', ')})`);
  ga.leaveWorld();
}

// ---- a Transport to another peak's unvisited backcountry across a world switch: pktrans paused at the confirm, resumed at WS10 ---
{
  const ctx = { career: true, round: 3, mode: null, visited: (1 << 14) | (1 << 17), peak2Locked: true };
  const ga = await make(ctx);
  await ga.runStart({ courseIndex: 17, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 500); ga.timelineReset();
  ga.travelSwitch(15);
  assert.equal(ga.debug().music, 'pktrans');
  ga.leaveWorld(); await ga.loadingStart({ courseCode: 'PEAK2', character: 'zoe' });
  await ga.freeWorldLoaded({ courseIndex: 15, courseCode: 'PEAK2', character: 'zoe', freeRide: FREE });
  assert.ok(trace(ga).includes('first visit 15 (234FE0)'));
  assert.equal(ga.debug().director.bcIntro, 1); assert.equal(ga.debug().director.hubFirst, 0);
  await ga.runStart({ courseIndex: 15, courseCode: 'PEAK2', character: 'zoe', freeRide: FREE });
  await run(ga, 2100);
  const k = kinds(ga);
  assert.ok(k.includes('code 19') && k.includes('dj 7') && k.includes('request 1'), `the BC intro arrival (${k.join(', ')})`);
  ga.leaveWorld();
}

// ---- pv djVisited: the saved mask and the Peak 2 lock -----------------------------------------------------------------------------
{
  // Snow Jam visited in the save (a reload): no Free_Ride_Intro at the world-load DJ
  const ctx = { career: true, round: 3, mode: null, visited: (1 << 14) | (1 << 0), peak2Locked: true };
  let ga = await make(ctx);
  await ga.runStart({ courseIndex: 0, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 10);
  assert.equal(ga.debug().director.firstVisit[0], '0'); assert.ok(!ga.debug().director.pending.freeRideIntro);
  ga.leaveWorld();
  // Metro-City unvisited but Peak 2 open: 146008 fails, no intro and 579C[1] stays set
  ctx.visited = 1 << 14; ctx.peak2Locked = false;
  ga = await make(ctx);
  await ga.runStart({ courseIndex: 1, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 10);
  assert.ok(!ga.debug().director.pending.freeRideIntro); assert.equal(ga.debug().director.firstVisit[1], '1');
  assert.equal(ga.debug().director.pending.artist, 1);
  ga.leaveWorld();
  // Peak 2 locked: the intro, and the flag cleared
  ctx.peak2Locked = true;
  ga = await make(ctx);
  await ga.runStart({ courseIndex: 1, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 10);
  assert.equal(ga.debug().director.pending.freeRideIntro, 1); assert.equal(ga.debug().director.firstVisit[1], '0');
  ga.leaveWorld();
  // a world load into visited Happiness after a reload: no pktrans / first-visit routine
  ctx.visited = 1 << 14;
  ga = await make(ctx);
  await ga.runStart({ courseIndex: 14, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  assert.notEqual(ga.debug().music, 'pktrans'); assert.equal(ga.debug().director.bcIntro, 0);
  ga.leaveWorld();
}

// ---- pv djQueueRules ------------------------------------------------------------------------------------------------------------
{
  const ga = await make({ career: true, round: 3, mode: null });
  await ga.runStart({ courseIndex: 17, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 3000);
  // a pause resume without an in-game song change keeps the queued DJ events
  const sp = ga.speechEngine;
  sp.dj(sp.EV.HUB_WEATHER, [1]);
  const before = sp.debug().pending.length;
  ga.pause(true); ga.pause(false);
  assert.equal(sp.debug().pending.length, before, 'queued DJ survives the resume');
  ga.leaveWorld();
}
for (const k of SWITCHES) setPv(k, null);
console.log('ctm audio: world-switch carry, post-event commentary, saved first visits, DJ queue rules OK');
