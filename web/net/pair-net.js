// Rider-vs-rider across the network: the original race world (web/race_world.cpp) of an online race, run in this
// client's own core with every racer in its lobby slot.
//
// In the original one world holds every rider: 0x10F560 ranks and refreshes the pair proximity records at the
// start of every sixth tick, and each rider's second motion phase 121750 runs the rider pairs 0x107888 against
// every other rider (separation shared by weight, impulses to both, the attacker's hit to the victim). Online no
// client owns another racer's rider, so each client runs that world for its own rider only:
//   * the other racers are kinematic ghosts: their streamed pair view (body spheres, velocity, motion/control,
//     attack state; web/net/remote-riders.js ghost(), positions dead-reckoned to this tick);
//   * the local rider's 121750 dispatches 0x107888 for its own slot (Module.riderHost.pairs, flag 2): contacts
//     with a ghost apply only the local rider's half (its separation share, its impulse and reaction); the
//     ghost's half is applied by the ghost's own client, which sees the mirror contact against our ghost;
//   * an attack (0x107888 attack branch) that hits a ghost is sent to the victim (rider-packet ATTACK); the
//     victim's client applies the same 107E70 response to its own rider at its next pair point
//     (race_world_pair_respond), so the victim decides its own knockdown with its own state;
//   * the manager refresh 0x10F560 (ranking 0x10F998 -> +0xEC place, pair proximity records) runs every sixth tick
//     on every racer's state at one common past instant (the newest packet tick RANK_DELAY ticks back: this rider's
//     own recorded state and the others' packets of that tick), so every client ranks from the same numbers and the
//     place displays agree; the original refreshes these only every sixth tick anyway. The place drives the original
//     place display (0x21E1B0 / 0x1EA930).
//   * contact reactions draw from one deterministic stream per (race seed, contact tick, rider pair, reacting
//     rider) -- contactStream() -- instead of each machine's own game RNG, so a reaction is the same wherever it is
//     computed: the owner's real response and the other client's prediction of it draw identical words (docs/
//     multiplayer.md "Contact randomness" for how this differs from the PS2's one world RNG);
//   * lag compensation: the ghost's half of a contact (its separation share and impulse, which the ghost's own
//     client applies) is predicted here too and added to the ghost until that client's packets carry it, so this
//     rider is not pushed again by an overlap the other rider has already resolved (remote-riders.js correct()).
import { rngNext } from '../ai-racers.js';
import { placeGlowStep } from '../race-place-hud.js';
import { DELAY_TICKS } from './remote-riders.js';

export const RANK_DELAY = DELAY_TICKS;

const f32 = (core, p, n) => new Float32Array(core.HEAPF32.buffer, p, n);
// Zoe's pair inputs (npc-riders.json world.pair_inputs[0]) for a racer that did not send its own.
export const DEFAULT_PAIR = { weight_attribute: 65, collision_stat: 0.09090909361839294, attack_stat: 0.09090909361839294 };

// Contact random stream: six words of the shared generator (0x317810 layout) keyed by the race seed, the contact
// tick, the pair and the reacting rider (a 32-bit murmur finaliser chain).
export function contactStream(seed, tick, a, b, target) {
  let h = (seed ^ 0x9e3779b9) >>> 0;
  const mix = (x) => { h = Math.imul(h ^ (x >>> 0), 0x85ebca6b) >>> 0; h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35) >>> 0; h ^= h >>> 16; return h >>> 0; };
  mix(tick); mix(Math.min(a, b)); mix(Math.max(a, b)); mix(target);
  const w = new Uint32Array(6); for (let k = 0; k < 6; k++) w[k] = mix(0x632be5ab + k);
  return w;
}

export function createPairNet({ core, remote, slot, count, pairInputs = [], seed = 0, sendAttack = () => {}, predictResponse = true }) {
  if (!core?._race_world_pairs || !core._race_world_pair_respond) throw new Error('core lacks the online pair exports (web/build-core.sh)');
  if (!(count >= 1 && count <= 6) || !(slot >= 0 && slot < count)) throw new Error(`online pair world: slot ${slot} of ${count}`);
  const buffer = core._malloc(36 * 4);
  const counts = { checks: 0, separations: 0, impulses: 0, attacks: 0, reactions: 0, sentAttacks: 0, receivedAttacks: 0, ghostTranslations: 0, ghostImpulses: 0, streamDraws: 0 };
  // Contact streams of the running dispatch: keyed by (key tick, pair, target); `other` = the pair's remote slot,
  // `viewed` = the rider whose response is being computed (107E70 views its target right before drawing).
  let streams = new Map(), keyTick = 0, other = -1, viewed = -1;
  const ghostVelocity = [];
  const reactions = { own: [], predicted: [] }; // QA: this rider's reactions and the ones predicted for the ghosts
  const stream = (target) => { const k = `${keyTick}:${Math.min(slot, other)}:${Math.max(slot, other)}:${target}`; let w = streams.get(k); if (!w) streams.set(k, (w = contactStream(seed, keyTick, slot, other, target))); return w; };
  const history = []; // this rider's [x, y, z, remaining] after each tick (index = tick)
  let lastRankInputs = null, rankState = null, nextRankPass = 0, rankPasses = 0, tick = 0, worldState = null, pending = [], place = -1, placeTimer = -1, glowTimer = -1, disabledMask = 0;
  const rng = () => new Uint32Array(core.HEAPU8.buffer, core._animation_rng_words(), 6);
  // Original world reset (npc-riders.json layout): records a->b enabled for every pair of racers, all human.
  {
    const init = [count, 0, 1];
    for (let a = 0; a < 6; a++) {
      for (let b = 0; b < 6; b++) init.push(a < count && b < count && a !== b ? 1 : 0, 1, 1e10, 0, 0, 0, 0, 0, 0);
      init.push(a);
    }
    for (let i = 0; i < 36; i++) init.push(0);
    const p = core._malloc(init.length * 4); core.HEAPF32.set(init, p >> 2); core._race_world_reset(p); core._free(p);
    const pairs = []; for (let s = 0; s < 6; s++) { const w = pairInputs[s] ?? DEFAULT_PAIR; pairs.push(w.weight_attribute, w.collision_stat, w.attack_stat); } pairs.push(0);
    const q = core._malloc(pairs.length * 4); core.HEAPF32.set(pairs, q >> 2); core._race_world_pair_setup(q); core._free(q);
    core._race_world_pair_disable(0);
  }
  const previous = { riderHost: core.riderHost, pairHost: core.pairHost };
  core.pairHost = {
    view(s, out) {
      viewed = s;
      if (s === slot) { new Uint32Array(core.HEAPU8.buffer, out, 140).set(new Uint32Array(core.HEAPU8.buffer, core._pair_view(), 140)); return; }
      other = s;
      const words = remote.ghost(s, tick) ?? new Uint32Array(140); // inside this tick's 121750: the ghost after this tick
      const f = new Float32Array(words.buffer); ghostVelocity[s] = [f[87], f[88], f[89]];
      new Uint32Array(core.HEAPU8.buffer, out, 140).set(words);
    },
    // The ghost's half is applied by its own client; it is predicted on the ghost until that client's packets show it.
    translate(s, x, y, z) { if (s === slot) core._pair_translate(x, y, z); else { counts.ghostTranslations++; if (predictResponse) remote.correct(s, tick, [x, y, z], null); } },
    velocity(s, x, y, z, reseed) {
      if (s === slot) { core._pair_set_velocity(x, y, z, reseed); return; }
      counts.ghostImpulses++; const v = ghostVelocity[s];
      if (predictResponse && v) remote.correct(s, tick, null, [x - v[0], y - v[1], z - v[2]]);
    },
    // A reaction of this rider: its draws (the soft reaction's 0x108388 motion draws, a crash entry's) come from the
    // contact stream, loaded into the core's game RNG for the call and taken back after.
    react(s, kind, animation, attack, eventPtr) {
      const log = s === slot ? reactions.own : reactions.predicted; log.push({ tick: keyTick, slot: s, other: s === slot ? other : slot, kind, animation, attack: !!attack }); if (log.length > 64) log.shift();
      if (s !== slot) return;
      const words = rng(), saved = words.slice(), w = stream(slot);
      words.set(w); try { core._pair_react(kind, animation, attack, eventPtr); } finally { const after = rng(); w.set(after); after.set(saved); }
    },
    random() { counts.streamDraws++; return rngNext(stream(viewed === slot ? slot : other)); },
    attack(victim, attacker, x, y, z, amount) {
      if (attacker !== slot || victim === slot) return;
      counts.sentAttacks++; sendAttack({ victim, attacker, tick, direction: [x, y, z], amount });
    },
  };
  core.riderHost = {
    // 0x107888 inside the local rider's 121750: first the attacks other clients found on this rider, then its own pass.
    pairs() {
      for (const e of pending.splice(0)) {
        if (tick - e.tick > 30 || e.attacker >= count || e.attacker === slot) continue;
        // The attacker's client drew its prediction of this response from the stream of (attack tick, pair, victim).
        keyTick = e.tick; other = e.attacker; streams = new Map();
        const out = f32(core, core._race_world_pair_respond(slot, e.attacker, e.direction[0], e.direction[1], e.direction[2], e.amount), 5);
        counts.impulses += out[2]; counts.reactions += out[4]; counts.receivedAttacks++;
      }
      keyTick = tick; streams = new Map();
      const out = f32(core, core._race_world_pairs(tick, slot), 5);
      counts.checks += out[0]; counts.separations += out[1]; counts.impulses += out[2]; counts.attacks += out[3]; counts.reactions += out[4];
    },
  };
  core._rider_host(2);
  const api = {
    counts,
    get tick() { return tick; },
    get worldState() { return worldState; },
    reactions,
    get disabledMask() { return disabledMask; },
    get rankInputs() { return lastRankInputs; }, // QA: the last ranking pass's [x, y, z, remaining] per slot
    // An attack event from another client ({victim, attacker, tick, direction, amount}).
    receiveAttack(e) { if (e.victim === slot) pending.push(e); },
    // 0x10F560 on the tick-start state (call before the local rider's pad_tick).
    beginTick() {
      const own = f32(core, core._rider_world_state(), 3), progress = f32(core, core._race_progress_info(), 1);
      history[tick - 1] = [own[0], own[1], own[2], progress[0]]; delete history[tick - 1 - 480];
      const buf = f32(core, buffer, 36);
      // Ranking 0x10F998, every sixth tick, as a pipeline every client runs identically: pass P ranks every racer's
      // state after packet tick at(P) (the newest packet tick RANK_DELAY ticks before P; packets carry every third
      // tick): this rider's own recorded state and the others' exact packets. A pass waits until those packets are
      // in, so every client computes each pass from the same numbers and the place displays agree.
      while (nextRankPass <= tick) {
        const at = Math.floor((nextRankPass - 1 - RANK_DELAY) / 3) * 3;
        if (at < 0) { nextRankPass += 6; continue; }
        const rows = [];
        for (let s = 0; s < count; s++) rows[s] = s === slot ? history[at] : remote.exactAt(s, at, !remote.live(s, tick));
        if (rows.some((r) => !r)) break;
        buf.fill(0); rows.forEach((r, s) => buf.set([r[0], r[1], r[2], r[3], 0, 1], s * 6));
        core._race_world_frame(nextRankPass, buffer);
        rankState = f32(core, core._race_world_state(), 396).slice(); lastRankInputs = rows.map((r) => [...r]); rankPasses++;
        nextRankPass += 6;
      }
      // Pair proximity records (planar distance / bearing, 0x10F6B0) on this tick's state: this rider now and the
      // ghosts dead-reckoned to now; the ranking is not run here (+0x880 of this rider cleared for the call).
      buf.fill(0); disabledMask = 0;
      for (let s = 0; s < count; s++) {
        if (s === slot) { buf.set([own[0], own[1], own[2], progress[0], 0, 0], s * 6); continue; }
        const r = remote.rankInput(s, tick - 1);
        buf.set(r ? [r[0], r[1], r[2], r[3], 0, 1] : [0, 0, 0, 1e9, 0, 1], s * 6);
        if (!remote.live(s, tick)) disabledMask |= 1 << s;
      }
      core._race_world_pair_disable(disabledMask);
      core._race_world_frame(tick, buffer);
      worldState = f32(core, core._race_world_state(), 396).slice();
    },
    // After the local rider's race_end: the place display timers (ai-race.js endTick, 0x1EA930 / 0x1EBC10).
    endTick() {
      const now = rankState ? rankState[360 + slot * 6] : -1;
      if (placeTimer >= 0) placeTimer = placeTimer >= 1 ? -1 : Math.min(1, placeTimer + (now === 0 ? 0.026456889 : 0.14841716));
      if (place >= 0 && now !== place) placeTimer = 0;
      place = now; glowTimer = placeGlowStep(glowTimer, now);
      tick++;
    },
    rank(s) { return rankState ? rankState[360 + s * 6] : s; },
    get rankPasses() { return rankPasses; },
    hud() { return place < 0 ? null : { place, total: count, timer: placeTimer, glow: glowTimer }; },
    dispose() {
      core.riderHost = previous.riderHost; core.pairHost = previous.pairHost;
      core._rider_host(previous.riderHost ? 3 : 0); core._race_world_pair_disable(0);
      core._free(buffer); pending = [];
    },
  };
  return api;
}
