// The time-attack podium phase as long as the podium's scene (web/server/podium-scripts.mjs, sessions.mjs podiumFit; docs/big-multiplayer.md
// "The podium's world and its hand-off to the vote"):
// - the table on the game data: every podium course and base rider gets that rider's three win_ps scenes (scfilter list 9), the least
//   shown first; Sam takes Mac's; a cheat skin 10..20 / 28 gets win_ps_cheat (list 10); other skins their base rider's (Zoe without one);
//   backcountry courses and unknown riders have none;
// - the phase on a fake clock: podiumFit off = podiumMs; on = the scene's ticks at 60 Hz plus podiumMarginMs, the scene in the results
//   (podiumScript) and in a joiner's snapshot; no table / no scene = podiumMs.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import zlib from 'node:zlib';
import { podiumScripts } from './server/podium-scripts.mjs';
import { createSessions, MAX_ATTRIBUTE } from './server/sessions.mjs';
import { eventTable } from './server/records.mjs';
import { encodeReplayFile } from './server/replay-file.mjs';
import { createRecording } from './replay.js';

const assets = new URL('public/assets/', import.meta.url).pathname;
const table = podiumScripts(assets);
assert.ok(table, 'the table loads from the assets root');
assert.equal(podiumScripts('/nonexistent'), null);
const index = JSON.parse(fs.readFileSync(assets + 'CUTSCENES/index.json', 'utf8'));
const scripts = new Map(index.scripts.map((s) => [s.number, s]));
const name = (n) => scripts.get(n).name.replace('podium\\', '');
// the disc's names of the base riders' scenes (docs/cutscenes.md: arielle = Allegra, deiter = Viggo, grommet = Griff, rocco = Nate)
const disc = { moby: 'moby', kaori: 'kaori', allegra: 'arielle', mac: 'mac', zoe: 'zoe', griff: 'grommet', elise: 'elise', nate: 'rocco',
  psymon: 'psymon', viggo: 'deiter', sam: 'mac' };
const courses = ['ABA1', 'ARA1', 'ASS1', 'BHP1', 'BRA2', 'CBA2', 'CHP2', 'CRA3', 'DRA4', 'DSS2', 'EBA3', 'EHP3', 'ERA5', 'ESS3'];
for (const code of courses)
  for (const [rider, scene] of Object.entries(disc)) {
    const plays = new Map();
    const seen = [];
    for (let k = 0; k < 3; k++) {
      const pick = table.choose(code, [{ rider }, { rider: 'zoe' }, { rider: 'mac' }], plays, () => 0.99);
      assert.ok(pick, `${code} ${rider}`);
      assert.match(name(pick.script), new RegExp(`^win_ps_${scene}[abc]?$`), `${code} ${rider}: ${name(pick.script)}`);
      assert.equal(pick.ticks, scripts.get(pick.script).duration);
      plays.set(pick.script, (plays.get(pick.script) || 0) + 1);
      seen.push(pick.script);
    }
    // the least shown first: three different scenes (Mac's macc is #30)
    assert.equal(new Set(seen).size, 3, `${code} ${rider}: ${seen}`);
  }
assert.equal(name(table.choose('ARA1', [{ rider: 'brodi' }]).script), 'win_ps_cheat');
assert.equal(name(table.choose('ARA1', [{ rider: 'canhuck' }]).script), 'win_ps_cheat');
assert.match(name(table.choose('ARA1', [{ rider: 'cudmore', base: 'elise' }]).script), /^win_ps_elise/);
assert.match(name(table.choose('ARA1', [{ rider: 'cudmore' }]).script), /^win_ps_zoe/);
for (const code of ['ABC1', 'DBC2', 'EBC3']) assert.equal(table.choose(code, [{ rider: 'zoe' }]), null, code);
assert.equal(table.choose('ARA1', [{ rider: 'nobody' }]), null);
assert.equal(table.choose('ARA1', []), null);

// ---- the phase on a fake clock (a synthetic career whose course 0 is Snow Jam) ----
const courseRows = Array.from({ length: 17 }, (_, i) => ({ index: i, code: i === 0 ? 'ARA1' : `X${i}`, name: `c${i}` }));
const recordSlots = Array.from({ length: 17 }, (_, i) => (i === 0 ? [0, 12, 14, 26] : [9, 26, 14, 26]));
const records = Array.from({ length: 26 }, () => Array.from({ length: 5 }, (_, k) => ({ value: 170 + 10 * k, character: k, name: `D${k}` })));
const events = eventTable({ rules: { record_slots: recordSlots, records }, courses: courseRows });
function runFile(ticks) {
  const rec = createRecording();
  const p = new Float32Array(24);
  const total = ticks + 180;
  for (let t = 0; t < total; t++) {
    p[0] = (t >> 6) & 1;
    rec.push(p);
  }
  const meta = { event: '0:ARA1', mode: 0, course: 'ARA1', round: 3, name: 'X', character: 4, rider: { id: 'zoe', attributes: Array(7).fill(MAX_ATTRIBUTE) },
    claim: { ticks }, ticks: total, finishTick: total - 1, events: [], lineup: { values: [], enabled: false }, core: 'abcdef0123456789', build: 'test' };
  return zlib.deflateRawSync(encodeReplayFile({ meta, pad: rec.exportBytes() }));
}
function podiumPhase(options, podium = table) {
  let clock = 1_000_000;
  const inbox = new Map();
  const clients = new Map();
  const c = { id: 'a', name: 'a', rider: 'zoe', pkg: 'RIDER_ZOE', token: 'tok-a', session: null };
  clients.set('a', c);
  const S = createSessions({ events, now: () => clock, send: (id, m) => { if (!inbox.has(id)) inbox.set(id, []); inbox.get(id).push(m); }, clients,
    random: () => 0, log: { log() {}, warn() {} }, podiumScripts: podium,
    options: { goDelayMs: 0, overtimeMaxMs: 1000, uploadGraceMs: 0, podiumMs: 20000, voteMs: 15000, minLimitMs: 1000, ...options } });
  const tick = (ms) => {
    for (let t = 0; t < ms; t += 250) {
      clock += 250;
      S.tick();
    }
  };
  S.message(c, { t: 'ta-create', name: 'Fit', limitMs: 200000, event: '0:ARA1', playerName: 'ANNA' });
  const sid = inbox.get('a').find((m) => m.t === 'ta-joined').session.id;
  const s = S.sessions.get(sid);
  S.message(c, { t: 'ta-loaded', window: s.window.id });
  tick(250);
  assert.equal(s.phase, 'window');
  clock = s.window.startAt;
  S.message(c, { t: 'ta-attempt', window: s.window.id, n: 1 });
  clock += ((3000 + 180) / 60) * 1000;
  assert.equal(S.submit({ compressed: runFile(3000), token: c.token, sessionId: sid, windowId: s.window.id, attempt: 1 }).status, 200);
  S.message(c, { t: 'ta-end', window: s.window.id, n: 1, reason: 'finish' });
  clock = s.window.endAt;
  tick(1000);
  assert.equal(s.phase, 'podium');
  const results = [...inbox.get('a')].reverse().find((m) => m.t === 'ta-results');
  return { ms: s.phaseEndsAt - s.phaseAt, results, S, c, inbox, clients };
}
const off = podiumPhase({});
assert.equal(off.ms, 20000, 'podiumFit off: podiumMs');
assert.equal(off.results.podiumScript, undefined);
const on = podiumPhase({ podiumFit: true });
const pick = scripts.get(on.results.podiumScript);
assert.match(pick.name, /win_ps_zoe/, 'the winner rides Zoe');
assert.equal(on.ms, Math.round((pick.duration * 1000) / 60), 'the scene at 60 Hz');
const margin = podiumPhase({ podiumFit: true, podiumMarginMs: 500 });
assert.equal(margin.ms, Math.round((scripts.get(margin.results.podiumScript).duration * 1000) / 60 + 500));
assert.equal(podiumPhase({ podiumFit: true }, null).ms, 20000, 'no table: podiumMs');
// a page joining during the podium gets the scene with the results
const d = { id: 'd', name: 'd', rider: 'mac', pkg: 'RIDER_MAC', token: 'tok-d', session: null };
on.clients.set('d', d);
on.S.message(d, { t: 'ta-join', session: [...on.S.sessions.keys()][0] });
const joined = on.inbox.get('d').find((m) => m.t === 'ta-joined');
assert.equal(joined.results?.podiumScript, on.results.podiumScript);
console.log(`podium fit: ${courses.length} courses x ${Object.keys(disc).length} riders; Snow Jam phase ${on.ms} ms for ${pick.name}`);
