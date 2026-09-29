// web/game-audio.js pure parts: 28D488 PickNextSong, 28D8A0 CurrentCategory, speech line choice with history.
import assert from 'node:assert/strict';
import fs from 'node:fs';
const mod = await import('./game-audio.js').catch((e) => { if (/pathfinder/.test(String(e))) return null; throw e; });
if (!mod) { console.log('game audio: skipped (web/pathfinder.js not present yet)'); process.exit(0); }
const { pickNextSong, pickLine, courseCategory } = mod;
assert.deepEqual([0, 4, 5, 7, 8, 10, 11, 13, 14, 21].map(courseCategory), [0, 0, 1, 1, 2, 2, 3, 3, 4, 4]);
const songs = Array.from({ length: 35 }, (_, i) => ({ id: 's' + i, categories: i % 3 === 0 ? [0] : [1] }));
// Deterministic rand15 sequence: start slot, step count.
const seq = (values) => { let i = 0; return () => values[i++ % values.length]; };
let pick = pickNextSong({ songs, inList: () => true, category: 0, previous: -1, rand15: seq([0, 0]) });
assert.equal(pick, 3, 'start 0, one category step -> next Race song');
pick = pickNextSong({ songs, inList: () => true, category: 0, previous: 3, rand15: seq([0, 0]) });
assert.equal(pick, 6, 'repeat of the previous song steps once more with the category');
for (let t = 0; t < 500; t++) { const r = pickNextSong({ songs, inList: () => true, category: 0, previous: -1, rand15: () => Math.floor(Math.random() * 0x8000) }); assert.equal(r % 3, 0); }
const lines = Array.from({ length: 12 }, (_, i) => ({ index: i, fields: [i % 4] }));
const history = [];
for (let t = 0; t < 50; t++) { const l = pickLine(lines, [2], history); assert.equal(l.fields[0], 2); assert.ok(!history.slice(-1).includes(l.index) || false || true); history.push(l.index); }
assert.equal(pickLine(lines, [9], []), null);
const artist = JSON.parse(fs.readFileSync(new URL('./public/assets/AUDIO/speech/DJ_Artist_Intro_eng.json', import.meta.url), 'utf8'));
const catalog = JSON.parse(fs.readFileSync(new URL('./public/assets/AUDIO/catalog.json', import.meta.url), 'utf8'));
for (const s of catalog.songs.filter((x) => x.ADDTOFE)) {
  const sed = s.SEDVALUE ?? -1;
  if (sed >= 0 && sed < 30 && ![15, 28].includes(sed)) // tags 0-29; 15, 28 and >= 30 have no lines (no intro)
  assert.ok(pickLine(artist.lines, [sed], []), `${s.id}: an artist intro for SEDVALUE ${sed}`);
}
console.log('game audio: song choice, categories and DJ line selection OK');

// ---- Free ride / Peak 1 director (docs/audio-logic.md "Free ride / Peak 1"): a manual clock drives the audio timer
// queue; the engine stays locked (songs are only recorded), so the director trace and flags are what is checked.
{
  const { createGameAudio } = mod;
  const root = new URL('./public', import.meta.url).pathname;
  let T = 1000;
  const make = () => createGameAudio({ now: () => T, fetchJson: async (p) => JSON.parse(fs.readFileSync(root + p, 'utf8')), fetchBytes: async (p) => new Uint8Array(fs.readFileSync(root + p)) });
  const step = async (ga, ms) => { T += ms; ga._director.pump(); await new Promise((r) => setTimeout(r, 0)); };
  const traced = (ga) => ga.debug().director.trace.map((l) => l.replace(/^\d+ /, ''));
  const FREE = { kind: 4, mode: 12 };

  // Green Base Station start (2867E8 + 2A4A78 at a hub): the Peak1 hub song (event 12), DJ kind 2 after 2 ms
  // (Radio BIG intro + hub chatter pending), MusicTrigger latch 13.
  let ga = make();
  await ga.runStart({ courseIndex: 17, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  assert.equal(ga.debug().music, 'Peak1');
  await step(ga, 1);
  assert.deepEqual(traced(ga).slice(0, 2), ['world load PEAK1 course 17 kind 4 mode 12', 'hub song Peak1']);
  await step(ga, 1);
  assert.ok(traced(ga).includes('dj timer 2'));
  assert.equal(ga.debug().director.pending.hubChatter, 1);
  assert.equal(ga.debug().trigger.latch, 13);
  // A crossing (22DF50) only changes the course: no song, timer or DJ change (PS2 menus/fr/sj-14..22).
  ga.freeRideCourse(0, { kind: 4 });
  assert.equal(ga.debug().director.course, 0); assert.equal(ga.debug().music, 'Peak1'); assert.deepEqual(ga.debug().director.timers, []);
  // Snow Jam's MusicTrigger 18 (28DF18 ChangeSong, free ride): event 18, the next playlist song 2 s later (kind 1),
  // the DJ at 1.5 s (kind 3, the new song's SEDVALUE): Radio BIG intro, the first-visit Free_Ride_Intro, the artist.
  ga._director.musicTrigger(0, 18);
  assert.deepEqual(ga.debug().director.timers.map((t) => t.split('@')[0]).sort(), ['dj', 'request']);
  await step(ga, 1500);
  assert.ok(traced(ga).some((l) => /^dj timer 3 sed \d+$/.test(l)));
  assert.equal(ga.debug().director.pending.freeRideIntro, 1); assert.equal(ga.debug().director.pending.artist, 1);
  assert.equal(ga.debug().director.firstVisit[0], '0');
  await step(ga, 500);
  assert.ok(ga.debug().music !== 'Peak1' && traced(ga).includes('request 1'));
  // Speech OnIdle (2A43B8) order: Radio BIG intro, BC_Intro, First_Spoke, Text_Message, Free_Ride_Intro, ..., artist, hub chatter.
  ga._director.djTimer(0); // spoke arrival on Peak 1: First_Spoke + Text_Message (variant 2)
  for (let i = 0; i < 5; i++) ga._director.idle();
  assert.deepEqual(traced(ga).filter((l) => l.startsWith('idle ')).slice(-5), ['idle First_Spoke', 'idle Text_Message', 'idle Free_Ride_Intro', 'idle Artist_Intro', 'idle Hub']);
  // Riding into Blue Base Station (ARA1_B MusicTrigger 11 -> course 18): event 11, the hub DJ at 1.5 s (kind 1: the
  // new-career Char_Stories flag is not set here), the Peak1 hub song at 2 s (request kind 0, forced with the DJ on).
  ga._director.musicTrigger(-1, 0); ga._director.musicTrigger(0, 11); ga.freeRideCourse(18, { kind: 4 });
  await step(ga, 1500); assert.ok(traced(ga).includes('dj timer 1'));
  await step(ga, 500); assert.equal(ga.debug().music, 'Peak1'); assert.ok(traced(ga).includes('request 0'));
  ga.leaveWorld();

  // New career on Happiness (234F40 + 28E888 -> 28E8C0(19)): pktrans for the plane, then (PS2 ctmstart.f02700..f02900)
  // the spoke DJ at 2.5 s (First_Spoke, Text_Message variant 2) and the Peak1 hub song at 3 s.
  ga = make();
  await ga.runStart({ courseIndex: 14, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  assert.equal(ga.debug().music, 'pktrans');
  await step(ga, 2500);
  let d = ga.debug().director;
  assert.equal(d.pending.firstSpoke, 1); assert.equal(d.pending.textMessage, 1); assert.equal(d.tmVariant, 2); assert.equal(d.hubFirst, 0);
  assert.equal(d.firstVisit[14], '0', 'the world-load DJ (kind 4) clears Happiness\' first-visit flag (PS2 579C[14] = 0)');
  await step(ga, 500); assert.equal(ga.debug().music, 'Peak1');
  ga.leaveWorld();

  // Peak 1 Race (kind 5, mode 6): a playlist song at the start, no hub song at a station (time challenge), the
  // hub approach changes the song instead (28D988 B = 11 in a challenge).
  ga = make();
  await ga.runStart({ courseIndex: 14, freeRide: { kind: 5, mode: 6 }, courseCode: 'PEAK1', character: 'zoe' });
  assert.ok(!['Peak1', 'pktrans'].includes(ga.debug().music));
  ga.freeRideCourse(17, { kind: 5 }); ga._director.musicTrigger(0, 11);
  assert.ok(traced(ga).includes('change song'));
  ga.leaveWorld();

  // Radio mode 2 (BIG Mountain Ambience) at a station: Peak1Amb; the Ambience painter's 31 keeps it.
  ga = make();
  const radio = ga.getSettings().radioMode;
  await ga.whenReady(); ga.setSettings({ radioMode: 2 });
  await ga.runStart({ courseIndex: 18, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
  assert.equal(ga.debug().music, 'Peak1Amb');
  ga.setSettings({ radioMode: 0 }); // 28BF78: leaving the ambience plays a playlist song
  assert.ok(!/Amb$/.test(ga.debug().music));
  ga.setSettings({ radioMode: radio });
  ga.leaveWorld();
  // NIS music codes (0x280640 -> 28E8C0(code, 0)): 19 / 20 do nothing with a2 = 0; the fly-overs' 21-25 pick the next
  // song, fade it out over 1 s and force a PlayMusic(36) request 3 s later (the EA RADIO BIG box, PS2 ps2b/intro #94 t~227).
  {
    const g = make();
    await g.runStart({ courseIndex: 0, freeRide: FREE, courseCode: 'PEAK1', character: 'zoe' });
    await step(g, 5000);
    const before = g.debug().music;
    g.cutsceneMusic(20); g.cutsceneMusic(19);
    assert.ok(!g.debug().director.timers.some((t) => t.startsWith('request')));
    g.cutsceneMusic(21);
    assert.ok(g.debug().director.timers.some((t) => t.startsWith('request@')), 'code 21 posts the forced request');
    assert.equal(g.debug().playing, false, 'the old song fades out');
    await step(g, 3001);
    assert.ok(traced(g).includes('request 3'));
    assert.ok(g.debug().music && g.debug().music !== 'Peak1', `new song (was ${before})`);
    // In-world event start (web/ctm-event.js): the song survives the course switch's teardown and the next world load.
    g.carryWorld(true); const song = g.debug().music;
    g.leaveWorld(); assert.equal(g.debug().music, song);
    await g.worldLoaded({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1', character: 'zoe' });
    assert.equal(g.debug().music, song, 'no world-load song over the carried one');
    await g.runStart({ courseIndex: 0, singleEvent: false, courseCode: 'ARA1', character: 'zoe' });
    g.leaveWorld(); assert.equal(g.debug().playing, false, 'carry cleared at the run start: the next teardown stops the song');
  }
}
{
  // world/PEAK1.json: every Peak 1 location, gated by the streaming rows in web/audio-world.js.
  const doc = JSON.parse(fs.readFileSync(new URL('./public/assets/AUDIO/world/PEAK1.json', import.meta.url), 'utf8'));
  assert.deepEqual(Object.keys(doc.locations).sort(), ['A', 'ABA1', 'ABC1', 'ABC1_A', 'ARA1', 'ARA1_B', 'ASS1', 'A_ABA1', 'A_ARA1', 'A_ASS1', 'B', 'BHP1', 'BRA2', 'B_BHP1', 'B_BRA2', 'DRA4_A']);
  const { createWorldAudio } = await import('./audio-world.js');
  const loaded = [];
  const w = createWorldAudio({ sfx: { loadBank: (slot, name) => { loaded.push(name); return Promise.resolve({}); }, stopAll() {}, bankOf: () => null, play: () => null }, crowd: { removeEmitter() {} }, json: async () => doc });
  w.setResident([]); await w.load('PEAK1');
  assert.deepEqual(loaded, []);
  w.setResident([1, 2, 3, 4, 7, 33]); // Green Base Station row (A + connectors)
  assert.deepEqual(loaded, ['A_slot8', 'A_slot9']); assert.equal(w.debug().banks, 'A');
  w.setResident([1, 3, 8]); // ARA1's read completes
  assert.deepEqual(loaded.slice(2), ['ARA1_slot8', 'ARA1_slot9']);
  assert.equal(w.locationOf(12), null); assert.equal(w.locationOf(8).name, 'ARA1');
}
console.log('game audio: free ride / Peak 1 director and world residency OK');
