// Audio director event timeline vs the PS2 (web/ps2-audio-timelines.json, reduced from ARMSX2 call logs of the music
// director, DJ scheduler and speech; docs/audio-logic.md 9.11). Each scenario drives web/game-audio.js through the same
// flow with a manual clock and compares its timeline (gameAudio.timeline()):
//   - the music / director events (play, event, pick, fade, stop, pause, resume, dj, request, code, fe, worldload, leave)
//     in order, with the timer-driven ones (dj, request) at the PS2 offsets from the scenario anchor (+-60 ms);
//   - the DJ / PA speech events in order (the browser test has no line durations, so only their order is compared);
//   - the Radio BIG intro variant (DJ_Radio_Big_Intro_FX for the spoke / song-change DJ, the plain one otherwise).
// Canonical forms: `*playlist` = any PickNextSong song; `hub` = any hub-chatter category (2A2E50 picks at random);
// the lodge's 28F140(1) + charsel = `charsel 1`; loading-screen tokens are compared only where the page has a load.
import assert from 'node:assert/strict';
import fs from 'node:fs';

const root = new URL('./public', import.meta.url).pathname;
if (!fs.existsSync(root + '/assets/AUDIO/catalog.json')) { console.log('audio timeline: skipped (no exported audio)'); process.exit(0); }
const { createGameAudio } = await import('./game-audio.js');
const ref = JSON.parse(fs.readFileSync(new URL('./ps2-audio-timelines.json', import.meta.url), 'utf8')).scenarios;

let T = 1000;
const FRAME = 1000 / 60;
const FREE = { kind: 4, mode: 12 };
const SPECIAL = new Set(['Peak1', 'Peak2', 'Peak3', 'Peak1Amb', 'Peak2Amb', 'Peak3Amb', 'pktrans', 'charsel', 'chartune']);
async function make(context = {}) {
  const ga = createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(root + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(root + p)) });
  await ga.whenReady(); await ga.speechEngine.init();
  ga.context = () => context;
  return ga;
}
// 60 Hz frames: the audio timer queue (2ADCA0) and the speech manager (2B0C78, OnIdle 2A43B8).
async function run(ga, ms) {
  for (const end = T + ms; T < end;) { T += FRAME; ga._director.pump(); ga.speechEngine.update(); await new Promise((r) => setImmediate(r)); }
}

// ---- canonical tokens ------------------------------------------------------------------------------------------------
const MUSIC = new Set(['play', 'event', 'pick', 'fade', 'stop', 'pause', 'resume', 'dj', 'request', 'code', 'fe', 'worldload', 'leave']);
function canonical(events, { loading = false } = {}) {
  const out = [];
  for (let i = 0; i < events.length; i++) {
    const [t, kind, a, b] = events[i];
    if (kind === 'hub') { const n = events[i + 1]; if (n?.[1] === 'speech') { out.push({ t: n[0], k: 'speech', v: 'hub' }); i++; } continue; }
    if (kind === 'speech') { out.push({ t, k: 'speech', v: a }); continue; }
    if (kind === 'line') { out.push({ t, k: 'line', v: a }); continue; }
    if (kind === 'loading') { if (loading) out.push({ t, k: 'music', v: `loading ${a}` }); continue; }
    if (!MUSIC.has(kind)) continue;
    let v = kind;
    if (kind === 'play') v = a === 'charsel' ? 'charsel' : `play ${SPECIAL.has(a) ? a : '*playlist'} ${b}`;
    else if (kind === 'fe') v = `charsel-event ${a}`;
    else if (kind === 'fade') v = `fade ${Number(a)}`;
    else if (kind === 'worldload') v = 'worldload';
    else if (['event', 'dj', 'request', 'code'].includes(kind)) v = `${kind} ${a}`;
    if (kind === 'event' && out.length && out.at(-1).v === `charsel-event ${a}`) continue; // 28F140 = its SendEvent
    out.push({ t, k: 'music', v });
  }
  // the lodge: charsel + 28F140(1), in either order within the same step
  const merged = [];
  for (const e of out) {
    const last = merged.at(-1);
    if (e.k === 'music' && last?.k === 'music' && ((e.v === 'charsel' && last.v.startsWith('charsel-event')) || (e.v.startsWith('charsel-event') && last.v === 'charsel'))) { last.v = `charsel ${(e.v.startsWith('charsel-event') ? e.v : last.v).split(' ')[1]}`; continue; }
    merged.push({ ...e });
  }
  return merged.map((e) => (e.v === 'charsel' ? { ...e, v: 'charsel 0' } : e));
}
function compare(name, portEvents, anchorOf, { loading = false, upTo = Infinity, timeBase = anchorOf } = {}) {
  const ps2 = canonical(ref[name].events, { loading }), port = canonical(portEvents, { loading });
  const a0 = ps2.find(anchorOf)?.t ?? 0, p0 = port.find(anchorOf)?.t;
  assert.ok(p0 !== undefined, `${name}: the port has the anchor event`);
  const from = Math.min(0, (ps2[0]?.t ?? a0) - a0) - 1000; // the port's events from the PS2 log's first one (before the anchor)
  const pick = (list, k, base) => list.filter((e) => e.k === k && e.t - base >= from && e.t - base <= upTo);
  const music = (l, base) => pick(l, 'music', base).map((e) => e.v), speech = (l, base) => pick(l, 'speech', base).map((e) => e.v);
  assert.deepEqual(music(port, p0), music(ps2, a0), `${name}: music / director events`);
  assert.deepEqual(speech(port, p0), speech(ps2, a0), `${name}: DJ / PA speech events`);
  // timer-driven events at the PS2 offsets from the anchor
  const timed = (l, base) => pick(l, 'music', base).filter((e) => /^(dj|request) /.test(e.v)).map((e) => [e.v, e.t - base]);
  const b0 = ps2.find(timeBase)?.t ?? a0, q0 = port.find(timeBase)?.t ?? p0; // the page has no load screen between the lodge and the world
  const want = timed(ps2, b0), got = timed(port, q0);
  want.forEach(([v, dt], i) => { assert.equal(got[i]?.[0], v, `${name}: ${v}`); assert.ok(Math.abs(got[i][1] - dt) <= 60, `${name}: ${v} at +${Math.round(got[i][1])} ms, PS2 +${dt} ms`); });
  // Radio BIG intro variant: the FX bank for the spoke / song-change DJ timers (28E548 kinds 0, 3), the plain one otherwise
  const fx = (l) => pick(l, 'line', -Infinity).filter((e) => /Radio_Big_Intro/.test(e.v)).map((e) => /_FX_/.test(e.v));
  assert.deepEqual(fx(port), fx(ps2), `${name}: Radio BIG intro variants`);
  return { music: music(port, p0), speech: speech(port, p0) };
}
const is = (v) => (e) => e.v === v;

// ---- 1. A new career: the Happiness plane drop (docs/audio-logic.md 9.11 "First arrival") --------------------------------
// PS2: charsel fades (286200), loading, pktrans (event 1) for the plane, the world load with no PickNextSong / PlayMusic and no
// DJ timer, the plane intro ends (28E8C0(19)), DJ kind 0 at 2.5 s (Radio BIG intro FX, First_Spoke, Text_Message), the Peak1
// hub song at 3 s. No playlist song, so no "Now Playing" box and no artist intro.
{
  const ga = await make({ career: true });
  await ga.screen('main'); await run(ga, 500); ga.timelineReset();
  await ga.loadingStart({ courseCode: 'PEAK1', character: 'zoe' }); await run(ga, 1000);
  ga.arrivalCinematic(true); // main.js: the plane cutscene (kind 'arrival') plays
  await ga.runStart({ courseIndex: 14, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 15000);
  ga.arrivalCinematic(false);
  await run(ga, 30000);
  const r = compare('new-career', ga.timeline(), is('code 19'), { loading: true });
  assert.ok(!r.music.some((v) => v.startsWith('pick')), 'no song is picked while pktrans plays');
  assert.ok(!r.speech.includes('20cf'), 'no artist intro');
  assert.equal(ga.debug().director.pending.textMessage, undefined);
  ga.leaveWorld();
}

// ---- 2. The in-world MCOMM (free-ride pause menu) and the Transport map: a pause, nothing else -------------------------------
{
  const ga = await make({ career: true, visited: 1 << 17 });
  await ga.runStart({ courseIndex: 17, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 20000); ga.timelineReset();
  ga.pause(true); for (const s of ['ctm-mcomm', 'ctm-messages', 'ctm-message', 'ctm-messages', 'ctm-mcomm', 'options', 'ctm-mcomm']) { await ga.screen(s); await run(ga, 1000); }
  ga.pause(false); await run(ga, 1000);
  compare('mcomm', ga.timeline(), is('pause'));
  assert.equal(ga.debug().music, 'Peak1', 'the hub song, never charsel, in the world');
  ga.timelineReset();
  ga.pause(true); for (const s of ['ctm-mcomm', 'ctm-peaks', 'ctm-events', 'ctm-peaks', 'ctm-mcomm']) { await ga.screen(s); await run(ga, 1000); }
  ga.pause(false); await run(ga, 1000);
  compare('transport-map', ga.timeline(), is('pause'));

  // ---- 3. The lodge: 286A80 leaves the world (Stop), charsel with 28F140(1); Return to Game = a world load at the station ----
  ga.pause(true); await ga.screen('ctm-enterlodge'); ga.timelineReset(); // the lodge door prompt pauses the ride
  await ga.screen('ctm-lodge'); await run(ga, 3000); await ga.screen('ctm-saveprompt'); await run(ga, 1000);
  compare('lodge-enter', ga.timeline(), is('leave'));
  assert.equal(ga.debug().music, 'charsel');
  ga.timelineReset();
  await ga.runStart({ courseIndex: 17, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 10000);
  compare('lodge-exit', ga.timeline(), is('fade 1'), { timeBase: is('worldload') });
  assert.equal(ga.debug().music, 'Peak1');
  ga.leaveWorld();
}

// ---- 4. MCOMM Transport from Happiness to Snow Jam, then Snow Jam's MusicTrigger 18 (first visit) ---------------------------
{
  const ga = await make({ career: true, visited: (1 << 14) | (1 << 17) });
  await ga.runStart({ courseIndex: 14, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 20000); ga.timelineReset();
  ga.pause(true); await ga.screen('ctm-mcomm'); await ga.screen('ctm-peaks'); await run(ga, 2000);
  ga.travel(0); await run(ga, 9000); // 28F520 Stop, map close (resume), 27A860 -> 28E8C0(20); the transport cut
  ga.freeRideCourse(0, { kind: 4 }); ga.arrived(); // 28E888 at the arrival cut's end
  await run(ga, 3000);
  compare('transport', ga.timeline(), is('stop'));
  ga.timelineReset();
  ga._director.musicTrigger(-1, 0); ga._director.musicTrigger(0, 18); // Snow Jam's course trigger
  await run(ga, 25000);
  compare('change-song', ga.timeline(), is('event 18'));
  ga.leaveWorld();
}

// ---- 5. A spoke arrival after the new-career intro (+0x5790 == 0): ARA1_B's MusicTrigger 11 into Blue Base Station ----------
{
  const ga = await make({ career: true });
  ga.arrivalCinematic(true);
  await ga.runStart({ courseIndex: 14, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  await run(ga, 2000); ga.arrivalCinematic(false); await run(ga, 30000);
  ga.freeRideCourse(0, { kind: 4 }); ga._director.musicTrigger(0, 18); await run(ga, 30000); ga._director.musicTrigger(-1, 0);
  ga.freeRideCourse(18, { kind: 4 }); ga.timelineReset();
  ga._director.musicTrigger(0, 11); await run(ga, 500); ga._director.musicTrigger(-1, 0);
  await run(ga, 7000); ga._director.musicTrigger(0, 13); await run(ga, 500); ga._director.musicTrigger(-1, 0);
  await run(ga, 15300); ga._director.musicTrigger(0, 17); await run(ga, 500); ga._director.musicTrigger(-1, 0);
  const r = compare('spoke', ga.timeline().filter((e) => !(e[1] === 'speech' && e[2] === '20e6')), is('event 11'));
  assert.deepEqual(r.speech.slice(0, 5), ['20e5', '20ba', '212c', '20ca', '20cc']);
  ga.leaveWorld();
}

// ---- 6. Single Event start: the world load's song in its idle section (36) with the PA venue intro; GO: event 0 -------------
{
  const ga = await make({});
  await ga.screen('event'); await run(ga, 500); ga.timelineReset();
  await ga.loadingStart({ courseCode: 'ARA1', character: 'zoe' }); await run(ga, 13000);
  await ga.worldLoaded({ courseIndex: 0, singleEvent: true, courseCode: 'ARA1', character: 'zoe' });
  await run(ga, 15000);
  ga.go(); await run(ga, 5000);
  compare('single-event', ga.timeline(), is('fade 1'), { loading: true });
  ga.leaveWorld();
}
// ---- 7. pv boothDj: the booth's flag (2A49E8, 30 s) with the DJ speaking keeps the travel from queuing the radio big intro (2A4A38) --
{
  const { setPv } = await import('./pv-flags.js');
  setPv('boothDj', true);
  try {
    const travelSpeech = async (booth, waitMs = 0) => {
      const ga = await make({ career: true, visited: (1 << 14) | (1 << 17) });
      await ga.runStart({ courseIndex: 17, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
      await run(ga, 20000);
      ga.speechEngine.speaking = (speaker) => speaker === 0xa;   // a DJ line is being spoken (2A10C0(audio, 10))
      if (booth) ga.booth();
      await run(ga, waitMs); ga.timelineReset();
      ga.pause(true); ga.travel(0); await run(ga, 500);
      const posts = ga.timeline().filter((e) => e[1] === 'speech').map((e) => e[2]);
      ga.leaveWorld(); return posts;
    };
    assert.ok((await travelSpeech(false)).includes('20e5'), 'no booth: the travel queues the radio big intro (2A26F0)');
    assert.ok(!(await travelSpeech(true)).includes('20e5'), 'the booth flag with the DJ speaking: no radio big intro (2A4A38)');
    assert.ok((await travelSpeech(true, 31000)).includes('20e5'), 'after 30 s (the 2ADCA0 timer, 2A4A68) the flag is gone');
  } finally { setPv('boothDj', null); }
}
console.log('audio timeline: new career, MCOMM, Transport map, lodge, Transport, song change, spoke arrival and Single Event start match the PS2 logs');
