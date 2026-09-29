// Browser glue for the original computer riders (web/ai-racers.js) in a race event: loads the
// course's npc-riders.json roster, sets up the computer riders (rider contexts of the human's core), draws them with their own
// models (web/opponent-riders.js) and provides standings for the HUD place display (0x21E1B0), the
// Single Event Results (0x23A760 / 0x238BF8 / 0x122D78) and the career results (ui.cb.standings).
// Courses without npc-riders.json race alone (returns null).
// The lineup (web/lineup.js): prepare() runs the original roster build 0x23A4F0 for the selected human when an event
// is loaded and sets the chosen riders up (assembled from /assets/<course>/lineups.json); without lineups.json, or until
// prepare() runs, the course's anchor lineup (npc-riders.json) races.
import { createAiRacers } from './ai-racers.js';
import { eventAnchorWords } from './event-anchor-rng.js';
import { pv } from './pv-flags.js';
import { rivalCharacter, CHARACTER_ID as RIVAL_IDS } from './rival-mode.js';
import { createOpponentRiders, initOpponentLighting } from './opponent-riders.js';
import { placeGlowStep } from './race-place-hud.js';
import { FixedStepClock } from './fixed-step-clock.js';
import { ageRelationships, anchorRandomWords, applyRelationshipEvent, assembleLineup, humanCharacter, humanGridState, lineupFor, nextRosterSeed, noteRaced,
  relationshipKinds, relationshipScores, rivalAnchorWords } from './lineup.js';

// Relationship records (web/lineup.js): bank 0 is the human's profile (kept in localStorage like the memory-card profile),
// banks 1-2 live for the browser session (the original keeps them in RAM from power-on). One set per page.
const REL_KEY = 'ssx3.relationships.v1', REL_SESSION = 'ssx3.relationships.session.v1';
const store = (kind) => { try { return kind === 'local' ? globalThis.localStorage : globalThis.sessionStorage; } catch { return null; } };
function loadRelationships(fresh) {
  const tables = JSON.parse(JSON.stringify(fresh));
  try { const b0 = JSON.parse(store('local')?.getItem(REL_KEY) || 'null'); if (Array.isArray(b0) && b0.length === 10) tables[0] = b0; } catch {}
  try { const b12 = JSON.parse(store('session')?.getItem(REL_SESSION) || 'null'); if (Array.isArray(b12) && b12.length === 2) { tables[1] = b12[0]; tables[2] = b12[1]; } } catch {}
  return tables;
}
// pv newGameReset: Options > Save/Load > New game resets profile 0's relationship records (+0xBC1, 0x1519E0; web/fe-saveload.js
// clears bank 0's storage); an instance already loaded reloads its tables at its next start instead of saving its old ones back.
let relGeneration = 0;
export function relationshipsReset() { relGeneration++; }
function saveRelationships(tables) {
  try { store('local')?.setItem(REL_KEY, JSON.stringify(tables[0])); } catch {}
  try { store('session')?.setItem(REL_SESSION, JSON.stringify([tables[1], tables[2]])); } catch {}
}

const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
const fround = Math.fround;
const DISPLAY = { psymon: 'Psymon', allegra: 'Allegra', moby: 'Moby', griff: 'Griff', luther: 'Luther', zoe: 'Zoe', mac: 'Mac', sam: 'Sam' }; // + riders.json names

// 0x122D78: estimated finish ticks of a rider still on course when the last human finished.
export function estimateFinishTicks(raceTicks, origin, remaining, slot) {
  let average = fround(fround(fround(origin) - fround(remaining)) / fround(raceTicks));
  const floor = fround(30 - slot);
  if (!(average >= floor)) average = floor;
  return raceTicks + Math.trunc(fround(fround(remaining) / average));
}
// 0x238BF8: slots ordered by recorded time (unsigned ascending; stable for equal times).
export function orderByTime(times) { return times.map((t, slot) => ({ t: t >>> 0, slot })).sort((a, b) => a.t - b.t || a.slot - b.slot).map((x) => x.slot); }

export async function createAiRace({ T, scene, human, course, loader, origin, humanName, environmentMeta, environmentBytes, lightAssets, worldKeys = null, isolate = new URL(globalThis.location?.href ?? 'http://x/').searchParams.get('isolate') === '1' }) { // ?isolate=1: no human<->computer rider pairs (QA replays of tools/ps2_capture.py --isolate captures)
  const get = async (path, type = 'text') => { const r = await fetch(path); if (!r.ok) throw new Error(`${path}: ${r.status}`); return type === 'buffer' ? new Uint8Array(await r.arrayBuffer()) : r.text(); };
  let documentText;
  try { documentText = await get(course.root + 'npc-riders.json'); } catch { return null; }
  const doc = JSON.parse(documentText);
  const [lineupData, roster] = await Promise.all([get(course.root + 'lineups.json').then(JSON.parse).catch(() => null), get('/assets/riders.json').then(JSON.parse).catch(() => [])]);
  // Backcountry rival events (docs/backcountry.md, tools/export_backcountry.py rivals): one document per game mode (race =
  // Rival Time, jam = Rival Points) and peak rival (0x145750: Mac, Griff when the human is Mac; Sam takes Mac's slot).
  const rivals = course.event === 'backcountry' ? await get(course.root + 'rivals.json').then(JSON.parse).catch(() => null) : null;
  for (const r of roster) DISPLAY[r.id] ??= r.name;
  // pv eventSlices (main.js: the human's course world loaded in parts, worldKeys = its parse keys and terrain hash): the riders' contexts
  // copy the parse caches by key (web/ai-racers.js attachWorld), so the course texts are fetched only if a cache misses.
  const worldTexts = async () => { const [terrainText, worldCollisionText, railsText] = await Promise.all([get(course.root + 'terrain.json'), get(course.root + 'world_collision.json'), get(course.root + 'rails.json')]); return { terrainText, worldCollisionText, railsText }; };
  const [packetsJson, packetsBin, initialText, texts] = await Promise.all([
    get('/assets/ANIMATIONS/animation-packets.json'), get('/assets/ANIMATIONS/animation-packets.bin', 'buffer'), get(course.initial),
    worldKeys ? null : worldTexts()]);
  const { terrainText = null, worldCollisionText = null, railsText = null } = texts ?? {};
  const riderText = {};
  await Promise.all(doc.riders.map(async (r) => { riderText[r.package] = await get(`/assets/${r.package}/rider.json`); }));
  if (rivals) await Promise.all(Object.values(rivals.documents).flatMap((m) => Object.values(m)).flatMap((d) => d.riders).map(async (r) => { riderText[r.package] ??= await get(`/assets/${r.package}/rider.json`); }));
  // pv eventSlices (worldKeys): the riders' setup yields a frame between its steps once a frame's work is done (web/load-slices.js framePause)
  const yieldFn = worldKeys ? (await import('./load-slices.js')).framePause('frame') : null;
  const resources = { packetsJson, packetsBin, initialText, terrainText, worldCollisionText, railsText, riderText, terrainHash: worldKeys ? worldKeys.hash : JSON.parse(terrainText).source_sha256, worldKeys, worldTexts, yieldFn };
  const racers = await createAiRacers({ human, resources, document: doc, isolate, sharedVisual: true }); // the computer riders are rider contexts of the human's core (docs/ai-racers.md) // one visual stream in the original pass order (docs/visual-rng-order.md)
  // The course-world inputs only set up the rider contexts' worlds: drop them (~20 MB of text otherwise kept for the whole
  // session). A lineup change (setDocument) needs only the animation packets, initial settings and rider packages.
  resources.terrainText = resources.worldCollisionText = resources.railsText = null;
  if (environmentMeta && environmentBytes && lightAssets) for (const n of racers.npcs) { initOpponentLighting(n.core, { environment: { meta: environmentMeta, bytes: environmentBytes }, lights: lightAssets }); if (yieldFn) await yieldFn(); }
  const load = async (path, type = 'json') => (type === 'json' ? JSON.parse(await get(path)) : type === 'buffer' ? (await get(path, 'buffer')).buffer : get(path));
  const opponents = racers.npcs.map((n) => ({ core: n.core, visible: true, reset: true }));
  const makeRenderer = () => (scene ? createOpponentRiders({ T, scene, load, loader, origin, packages: racers.npcs.map((n) => n.package),
    lighting: environmentMeta ? { configuration: environmentMeta.irradiance, viewCore: human } : undefined }) : null);
  let renderer = await makeRenderer();
  let origins = [doc.riders[0]?.progress_origin ?? 0, ...doc.riders.map((r) => r.progress_origin)];
  let placeTimer = -1, lastPlace = -1, glowTimer = -1, uncaptured = false;
  let lastBuild = null, lastFinish = [], planned = null;   // planned: plan()'s roster build, installed by the next prepare()   // career rounds reuse the round-1 roster; the semi/final advance by the last finish order
  // Relationships of this event's riders: the session tables, aged by every load/restart (0x155E58), changed by rider
  // pair reactions in the race (0x155BF0), pushed to the 10F560 world and each computer rider's row.
  // pv rivalRelations: an event without lineups.json (backcountry rivals, Gravitude, Kick Doubt) keeps them too, from the fresh table.
  const freshRelations = lineupData ? lineupData.relationships_fresh
    : pv('rivalRelations') ? await get('/assets/ARA1/lineup-sessions.json').then((t) => JSON.parse(t).fresh).catch(() => null) : null;
  let tables = freshRelations ? loadRelationships(freshRelations) : null, humanChar = { base: 4, cheat: 0 }, relGen = relGeneration;
  const participants = () => ({ characters: api.doc.relationships.characters, banks: api.doc.relationships.target_banks });
  const relationsOf = (t) => { const { characters, banks } = participants(), rival = api.doc.relationships.rival_character;
    return { scores: relationshipScores(t, characters, banks, rival), kinds: relationshipKinds(t, characters, banks, rival) }; };
  if (lineupData) racers.setAnchorRng(anchorRandomWords(lineupData, humanChar));   // the original's game RNG at the anchor (Zoe)
  // Rival challenge: the ready state's game RNG (race tick 0), until prepareRival picks this human's document. Without it
  // the core kept its seed's RNG (a glide state's) and the rival rode another line from its first draws (DBC2: Nate never
  // crashed at 1413 nor made the 1656 reset placement).
  else if (rivals) racers.setAnchorRng(rivalAnchorWords(rivals, rivals.documents.race?.[doc.riders[0]?.character], humanChar, roster));
  // pv eventAnchorRng: a course without lineups.json / rivals.json (Gravitude, Kick Doubt): the PS2's anchor RNG (web/event-anchor-rng.js).
  const eventAnchor = (who) => { if (lineupData || rivals || !pv('eventAnchorRng')) return; const a = eventAnchorWords(course.code, who); if (a) racers.setAnchorRng(a.words); };
  eventAnchor(humanChar);
  // A replay (web/replay.js) re-simulates the run from its start on a copy of the relationships as they were at its start:
  // the rider pair reactions change the copy the same way, nothing is aged, saved or messaged, and replayEnd() puts the
  // live tables back.
  let replayLive = null, racedTicks = 0;
  const clone = (x) => (x == null ? x : JSON.parse(JSON.stringify(x)));
  racers.onReact = (target, other, kind, attack) => {
    if (!tables || !api.doc.relationships.characters) return;
    const { characters, banks } = participants();
    const [before, after, score] = applyRelationshipEvent(tables, characters, banks, target, other, (kind === 2 ? 1 : 0) + (attack ? 2 : 0));
    if (!replayLive && other === 0 && before < after) api.onRelationshipNotice?.(characters[target], score);   // 0x155BF0 -> 0x1E2A08 (career messages)
    const next = relationsOf(tables); api.doc.relationships.kinds = next.kinds;
    if (JSON.stringify(next.scores) !== JSON.stringify(api.doc.relationships.scores)) racers.setRelationships(next.scores);
    if (!replayLive) saveRelationships(tables);
  };
  // 0x155E58 (-> 0x1E2A08 per record): every participant's records aged, the scores / kinds and the store updated
  let preparedEvent = null, agedAtEnd = false;
  const ageAll = () => {
    if (!tables || !api.doc.relationships.characters) return;
    const { characters, banks } = participants(), riders = racers.npcs.length + 1;   // R&B: the human and the opponent (not the stale slots)
    tables = ageRelationships(tables, characters.slice(0, riders), banks, api.onRelationshipNotice);
    const now = relationsOf(tables); api.doc.relationships.scores = now.scores; api.doc.relationships.kinds = now.kinds;
    saveRelationships(tables);
  };
  const api = {
    racers, doc, opponents, lineup: null,
    // false when this event has no computer rider (Conquer the Mountain slope style: 0x238E20 posts 5, nobody rides);
    // main.js then races the human alone (aiActive).
    enabled: true,
    names: () => (api.enabled === false ? [] : racers.npcs.map((n) => DISPLAY[n.character] || n.character)),
    // opts.replay: replaySnapshot() of the run being replayed (web/replay.js).
    start(opts = {}) {
      const replay = opts.replay ?? null;
      if (replay) {
        replayLive ??= { tables, scores: api.doc.relationships.scores, kinds: api.doc.relationships.kinds };
        tables = clone(replay.tables); api.doc.relationships.scores = clone(replay.scores); api.doc.relationships.kinds = clone(replay.kinds);
      } else api.replayEnd();
      if (!replay && relGen !== relGeneration && freshRelations && pv('newGameReset')) { relGen = relGeneration; tables = loadRelationships(freshRelations); }   // a New game since the load
      // the load (and a restart) ages every participant's records. pv relAging: only outside Conquer the Mountain (WS1's update,
      // 0x234894: game type 0x535C11 != 0 -> 155A50 / 155E58); a race's end ages them in every game type (results below)
      if (!replay) agedAtEnd = false;
      if (!replay && !(pv('relAging') && preparedEvent?.career)) ageAll();
      racers.start();
      if (tables) racers.setRelationships(api.doc.relationships.scores);
      // this race's presentation draws, noted at the next load; a replay's ticks draw the presentation stream too (the PS2 replay
      // does not restore 0x4FF018), so they count on
      if (!replay) { racedTicks = 0; noteRaced(() => ({ model: lineupData?.presentation_model, ticks: racedTicks })); }
      for (const o of opponents) o.reset = true; renderer?.reset(opponents); placeTimer = -1; lastPlace = -1; glowTimer = -1; for (const n of racers.npcs) { n.lastPlacements = 0; n.finishScore = null; n.score = 0; } api.opponentAtFinish = null;
    },
    // pv rivalCardAi: the computer riders as the ready state holds them, under a rival challenge's objectives card (main.js
    // readyView; PS2 local/reference/pcsx2/{happiness-mac,ruthless,the-throne}-ready: the rival at its start spot). The
    // placement of start() without its record ageing, saves or race notes, posed with an animation step of 0; each rider's
    // game / visual RNG words are put back. The card's Continue runs start() from scratch.
    readyPose() {
      if (api.enabled === false || !racers.npcs.length) return false;
      const views = racers.npcs.map((n) => { const c = n.core, u = (p, k) => (p ? new Uint32Array(c.HEAPU8.buffer, p, k) : null);
        return [u(c._animation_rng_words(), 6), u(c._visual_rng_words?.(), 6), u(c._visual_lcg_word?.(), 1)].filter(Boolean); });
      const kept = views.map((v) => v.map((w) => w.slice()));
      try { racers.start(); for (const n of racers.npcs) n.core._animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0); }
      finally { views.forEach((v, i) => v.forEach((w, k) => w.set(kept[i][k]))); }
      for (const o of opponents) o.reset = true; renderer?.reset(opponents); renderer?.capture(opponents); for (const o of opponents) o.reset = false; uncaptured = false;
      return true;
    },
    opponentAtFinish: null,
    // The relationship state a replay of this run starts from (after start()'s aging): web/replay.js keeps it with the run.
    replaySnapshot() { return { tables: clone(tables), scores: clone(api.doc.relationships.scores), kinds: clone(api.doc.relationships.kinds) }; },
    // The end of a replay: the live tables (as the run left them) again.
    replayEnd() { if (!replayLive) return; tables = replayLive.tables; api.doc.relationships.scores = replayLive.scores; api.doc.relationships.kinds = replayLive.kinds; replayLive = null; },
    get replaying() { return !!replayLive; },
    get relationships() { return tables; },
    onRelationshipNotice: null,   // (character, score): 0x1E2A08, set by main.js to the career messages (web/career-messages.js)
    beginTick() { racers.beginTick(); },
    endTick() {
      racers.endTick(); racedTicks++;
      // Freestyle (R&B slope style): each computer rider's run score +0x198; latched on its finish tick (0x239230 stores it
      // from 125108 after the 1193E0 payout), docs/slopestyle-bigair.md.
      for (const n of racers.npcs) { n.score = new Int32Array(n.core.HEAPU8.buffer, n.core._score_object_dump(), 0x1d0 / 4)[0x198 / 4]; if (n.finished && n.finishScore == null) n.finishScore = n.score; }
      // 0x239230 decides the round when every human has finished (12A250): the opponent as it stands on that tick.
      if (!api.opponentAtFinish && racers.npcs.length && racers.standings().find((r) => r.human)?.finished) api.opponentAtFinish = api.opponent();
      for (const [k, n] of racers.npcs.entries()) { const r = f32(n.core, n.core._reset_info(), 3)[2]; if (r !== n.lastPlacements) { n.lastPlacements = r; opponents[k].reset = true; } }
      // Skin palettes and rider lighting are presentation only (docs/ai-racers.md): a frame draws the palettes of its last
      // two ticks (previous, current), so earlier ticks of a multi-tick frame skip the capture. A reset (teleport) stays
      // pending until the next capture; a tick run outside the frame clock always captures.
      const left = FixedStepClock.ticksLeft;
      if (left == null || left <= 2) { renderer?.capture(opponents); for (const o of opponents) o.reset = false; uncaptured = false; }
      else uncaptured = true;
      // HUD update 0x1EA930 polls +0xEC every tick: a change restarts the timer at 0; it then steps
      // +0.026456889 (1st) or +0.14841716 and ends at -1 after reaching 1.
      const place = racers.worldState ? racers.worldState[360] : -1;
      if (placeTimer >= 0) placeTimer = placeTimer >= 1 ? -1 : Math.min(1, placeTimer + (place === 0 ? 0.026456889 : 0.14841716));
      if (lastPlace >= 0 && place !== lastPlace) placeTimer = 0;
      lastPlace = place;
      glowTimer = placeGlowStep(glowTimer, place); // 0x1EBC10: 1st-place glow timer (0 -> 2, wraps)
    },
    update(alpha) {
      if (uncaptured) { for (const o of opponents) o.reset = true; renderer?.capture(opponents); for (const o of opponents) o.reset = false; uncaptured = false; } // the clock ended early (a reset mid-frame)
      renderer?.update(opponents, alpha);
    },
    get renderer() { return renderer; },
    // Slope style opponent (the first computer rider): live score (the OPPONENT line 1ED5C8), its finish score, and the
    // 0x122E50 inputs (+0x4D0 remaining, +0x4D8 origin) for the final-round estimate.
    opponent() {
      const n = racers.npcs[0]; if (!n || api.enabled === false) return null;
      const row = racers.standings().find((r) => r.slot === n.slot);
      return { character: n.character, name: DISPLAY[n.character] || n.character, score: n.score ?? 0, finished: n.finishScore != null, finishScore: n.finishScore,
        remaining: row?.remaining ?? 0, origin: origins[n.slot] ?? 0 };
    },
    // Rows for ui.cb.standings (career: {human, character|name, finishTicks|null, remaining, place, origin, dnf}).
    standings() {
      const rows = racers.standings();
      return rows.map((r) => ({ human: r.human, slot: r.slot, character: r.human ? undefined : r.character, name: r.human ? humanName?.() : DISPLAY[r.character] || r.character,
        finishTicks: r.finished ? r.finishTicks : null, remaining: r.remaining, place: r.slot, rank: r.rank, origin: origins[r.slot], dnf: false }));
    },
    // Single Event Results (0x23A760): recorded finish times, estimates for riders still on course.
    // pv relAging: a race's end ages the relationships (233C10 at 0x233F08: event type 0x535C10 == 0 -> 155A50 / 155E58), once per race.
    results(raceTicks) {
      if (pv('relAging') && !agedAtEnd && !replayLive && !rivals && !(preparedEvent && preparedEvent.mode >= 1 && preparedEvent.mode <= 3)) { agedAtEnd = true; ageAll(); }
      const rows = api.standings();
      const times = rows.map((r) => (r.finishTicks != null ? r.finishTicks : estimateFinishTicks(raceTicks, r.origin, r.remaining, r.slot)));
      const order = orderByTime(times);
      lastFinish = order.map((slot) => (slot === 0 ? null : api.lineup?.values?.[slot - 1] ?? null));   // 0x536708 for the next career round
      return order.map((slot, place) => ({ ...rows[slot], place, ticks: times[slot], estimated: rows[slot].finishTicks == null }));
    },
    // The lineup of an event about to be loaded (ui.loadEvent work, before the warm-up): the original roster build for
    // this human (rider: main.js selectedRider {id, package, kind, base?}), assembled records, the changed cores set up
    // again, the renderer rebuilt for new models. event: a Conquer the Mountain race {career, round} (else a Single Event).
    // pv riderPrefetch (web/main.js warm-up): the packages of the event about to load, before the human rider loads, so their files
    // download alongside it. A race's roster build happens here instead of in prepare(), which installs it: the same one build
    // (one roster seed) per event load, and nothing draws on the presentation generator between the two (the human rider's load).
    // Rival and freestyle lineups are fixed by the event: their packages only.
    plan({ rider, event = null } = {}) {
      planned = null;
      if (!rider) return [];
      if (rivals) { const who = humanCharacter(rider, roster), mode = event?.mode === 5 || course.rivalMode === 5 ? 'jam' : 'race', m = rivals.documents[mode] ?? {};
        const d = m[RIVAL_IDS[rivalCharacter(course.peak ?? 1, who.base)]] ?? m[RIVAL_IDS[rivalCharacter(course.peak ?? 1, -1)]] ?? Object.values(rivals.documents.race)[0]; return (d?.riders ?? []).map((r) => r.package); }
      if (event && event.mode >= 1 && event.mode <= 3) { const o = event.opponent?.character; return o == null || !lineupData ? [] : assembleLineup(lineupData, humanCharacter(rider, roster).base, [o], { relationships: tables }).riders.map((r) => r.package); }
      if (!lineupData) return [];
      const who = humanCharacter(rider, roster), build = rosterBuild(who, event);
      planned = { base: who.base, cheat: who.cheat, event, ...build };
      return build.next.riders.map((r) => r.package);
    },
    async prepare({ rider, event = null } = {}) {
      preparedEvent = event;   // pv relAging: the game type (a career event) and event type of this load
      if (rivals && rider) return api.prepareRival({ rider, event });
      if (event && event.mode >= 1 && event.mode <= 3) return api.prepareFreestyle({ rider, event });
      api.enabled = true;
      if (!lineupData && rider) { humanChar = humanCharacter(rider, roster); eventAnchor(humanChar); }
      if (!lineupData || !rider) return api.lineup;
      humanChar = humanCharacter(rider, roster);
      const p = planned; planned = null;
      if (p && p.event === event && p.base === humanChar.base && p.cheat === humanChar.cheat) return install(p.next, p.lineup, rider);   // plan()'s build
      const career = !!event?.career && event.mode === 0, round = career ? event.round || 1 : 3;
      // Conquer the Mountain: the round-1 roster built on the career's roster generator when the event was fresh
      // (web/career.js startEvent, kept in the save); the semi and the final reuse it.
      const built = career && event.roster?.entries && event.roster.human === humanChar.base ? event.roster.entries : null;
      const reuse = !built && career && round > 1 && lastBuild?.human === humanChar.base;
      const seed = built ? null : reuse ? lastBuild.seed : nextRosterSeed();
      const lineup = lineupFor({ seed, entries: built ?? (reuse ? lastBuild.entries : null), human: humanChar, peak: lineupData.peak, round, career, previous: round > 1 ? lastFinish : [] });
      lastBuild = { seed, human: humanChar.base, entries: lineup.entries };
      const next = assembleLineup(lineupData, humanChar.base, lineup.values, { relationships: tables });
      return install(next, lineup, rider);
    },
    // Freestyle (docs/slopestyle-bigair.md "Opponent and posted riders"): web/career.js startEvent drew the roster and the
    // posting on the roster generator; a Single Event slope style rides its last shuffled character (event.opponent),
    // assembled for this human from /assets/<course>/lineups.json. Conquer the Mountain slope style, pipe and big air: no
    // computer rider.
    async prepareFreestyle({ rider, event }) {
      humanChar = humanCharacter(rider, roster);
      const opponent = event.opponent?.character;
      api.enabled = opponent != null;
      if (!api.enabled) {
        if (lineupData) humanGrid(rider);
        renderer?.reset(opponents);
        api.lineup = { human: humanChar, values: [], names: [], changed: [], opponent: null };
        return api.lineup;
      }
      if (!lineupData) { eventAnchor(humanChar); return api.lineup; }   // no lineups.json: the anchor's computer rider rides
      const next = assembleLineup(lineupData, humanChar.base, [opponent], { relationships: tables });
      return install(next, { seed: null, entries: event.roster?.entries ?? null, values: [opponent], opponent }, rider);
    },
    // Rival challenge (docs/backcountry.md): the document of this game mode (4 race / 5 jam, event.mode; Single Event
    // entries carry rivalMode) and of the peak rival for this human's base character.
    async prepareRival({ rider, event = null }) {
      const who = humanCharacter(rider, roster);
      const mode = event?.mode === 5 || course.rivalMode === 5 ? 'jam' : 'race';
      // the peak rival 145750 of this course's peak (Peak 1 Mac / Griff, Peak 2 Nate / Zoe, Peak 3 Psymon / Elise)
      const rival = RIVAL_IDS[rivalCharacter(course.peak ?? 1, who.base)];
      const next = JSON.parse(JSON.stringify(rivals.documents[mode]?.[rival] ?? rivals.documents[mode]?.[RIVAL_IDS[rivalCharacter(course.peak ?? 1, -1)]] ?? Object.values(rivals.documents.race)[0]));
      next.world.pair_inputs[0] = { ...api.doc.world.pair_inputs[0] };   // the human's own (character-roster.js applyHumanPairInputs)
      const packages = racers.npcs.map((n) => n.package).join();
      const changed = racers.setDocument(next);
      racers.setAnchorRng(rivalAnchorWords(rivals, next, who, roster));   // the ready state's game RNG (race tick 0; web/lineup.js)
      for (const c of [human, ...racers.npcs.map((n) => n.core)]) c._event_kind?.(next.game_mode?.kind ?? -1);   // 0x535C10 (stage builtin43): 5 Rival Time, 6 Rival Points
      api.doc = next; api.lineup = { human: who, rival, mode, names: api.names(), changed };
      origins = [next.human_start?.progress_origin ?? next.riders[0]?.progress_origin ?? 0, ...next.riders.map((r) => r.progress_origin)];
      if (renderer && racers.npcs.map((n) => n.package).join() !== packages) { renderer.dispose(); renderer = await makeRenderer(); }
      for (const o of opponents) o.reset = true;
      return api.lineup;
    },
    // In-race place display 0x21E1B0: 0-based place (+0xEC), rider count, and its change animation
    // (0x1EA930: t restarts at 0 on a change; +0.026456889/tick in 1st, +0.14841716 otherwise).
    hud() { return lastPlace < 0 ? null : { place: lastPlace, total: racers.npcs.length + 1, timer: placeTimer, glow: glowTimer }; },
  };
  // A lineup document for this human: the anchor RNG, the human's grid spot, the rider packages, the cores set up again
  // where the record changed, the renderer rebuilt for new models.
  // prepare()'s roster build for this human (plan()): the same statements, on the given human
  function rosterBuild(who, event) {
    const career = !!event?.career && event.mode === 0, round = career ? event.round || 1 : 3;
    const built = career && event.roster?.entries && event.roster.human === who.base ? event.roster.entries : null;
    const reuse = !built && career && round > 1 && lastBuild?.human === who.base;
    const seed = built ? null : reuse ? lastBuild.seed : nextRosterSeed();
    const lineup = lineupFor({ seed, entries: built ?? (reuse ? lastBuild.entries : null), human: who, peak: lineupData.peak, round, career, previous: round > 1 ? lastFinish : [] });
    lastBuild = { seed, human: who.base, entries: lineup.entries };
    return { lineup, next: assembleLineup(lineupData, who.base, lineup.values, { relationships: tables }) };
  }
  function humanGrid(rider) {   // the human's own grid spot on this course (Snow Jam's comes with the rider's settings)
    if (course.code === 'ARA1') return;
    const grid = humanGridState(lineupData, rider, humanChar.base, { career: !!rider.career });
    if (grid && human._human_grid_seed) { const t = new TextEncoder().encode(JSON.stringify(grid) + '\0'), q = human._malloc(t.length); human.HEAPU8.set(t, q); human._human_grid_seed(q); human._free(q); }
  }
  async function install(next, lineup, rider) {
    racers.setAnchorRng(anchorRandomWords(lineupData, humanChar));
    humanGrid(rider);
    next.world.pair_inputs[0] = { ...api.doc.world.pair_inputs[0] };   // the human's own (character-roster.js applyHumanPairInputs)
    await Promise.all(next.riders.map(async (r) => { if (!resources.riderText[r.package]) resources.riderText[r.package] = await get(`/assets/${r.package}/rider.json`); }));
    const packages = racers.npcs.map((n) => n.package).join();
    const changed = racers.setDocument(next);
    api.doc = next; api.lineup = { ...lineup, human: humanChar, names: api.names(), changed };
    origins = [next.riders[0]?.progress_origin ?? 0, ...next.riders.map((r) => r.progress_origin)];
    if (renderer && racers.npcs.map((n) => n.package).join() !== packages) { renderer.dispose(); renderer = await makeRenderer(); }
    for (const o of opponents) o.reset = true;
    return api.lineup;
  }
  if (typeof window !== 'undefined' && new URL(location.href).searchParams.has('qa')) window.ssxAiRace = api; // QA handle
  return api;
}
