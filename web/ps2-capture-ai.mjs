// Reader for ARMSX2 captures built with `tools/ps2_capture.py build ... --ai-state` (32 KiB records).
// Layout comes from the capture manifest (RUN.capture.json, layout.ai_state); offsets 0..10752 are the default
// human record.  Timing of record N (game tick N), all sampled at the human provider exit 0x128630, i.e. in the
// rider manager 128AF0's pass 121068 for the human (first in roster order), before the human's controllers:
//   * human fields, AI actor/owner windows, game info, RNG words: state after tick N's pre-passes
//     (12BB20, 113C20, 10F560 and 120F20 for every rider) and before any computer rider's 121068 of tick N;
//   * ai[k].words: the 8-byte command NPC provider 0x10A768 returned for that rider in tick N-1
//     (ai[k].wordsTick is the game tick read at that return, ai[k].providerCalls the returns since record N-1);
//   * rng.log: the draws (0x317810 entries) after record N-1 was written, in order; each entry carries the
//     resolved caller (through the 317830/317890 range wrappers), the leaf $ra and the last attribution marker
//     (entry hook on a rider-manager pass: pass function + its a0, mapped to a rider where possible).
// CLI: node web/ps2-capture-ai.mjs RUN.bin [--json]  prints a per-AI and RNG attribution summary.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Rider-manager 128AF0 call sites (return addresses) -> the pass function called there (from its disassembly).
export const PASS_BY_RA = {
  0x128b14: 0x12bb20, 0x128b1c: 0x113c20, 0x128b24: 0x10f560, 0x128ce4: 0x120ed8,
  0x128d44: 0x120f20, 0x128d6c: 0x121068, 0x128d94: 0x1210b0, 0x128dbc: 0x1211f8, 0x128de4: 0x1216e0, 0x128e0c: 0x121700,
  0x128e34: 0x121728, 0x128e5c: 0x121750, 0x128e84: 0x1217f8, 0x128eac: 0x121818, 0x128ed4: 0x1218d0, 0x128efc: 0x121950,
  0x128f28: 0x2dd0b8, 0x128f58: 0x2dabc8, 0x128f88: 0x2e8938, 0x128fb8: 0x2e66b8, 0x128fe8: 0x2eadd0, 0x129018: 0x2ef6d0,
  0x129048: 0x2d4c08, 0x129078: 0x2e39d8, 0x1290a8: 0x2df920, 0x1290d0: 0x2f1150, 0x1290f8: 0x2f6518, 0x12911c: 0x120e88,
  0x12912c: 0x101b60, 0x129134: 0x1013a8,
};

const hex = (v) => '0x' + (v >>> 0).toString(16);

// tools/ps2_capture.py writes the computer riders' record blocks in ascending actor-address order (manifest.others is sorted), while
// a lineup document (npc-riders.json, a career lineup of tools/export_lineups.py export-career) lists them in roster order (C+0x28).
// The two agree in every Single Event capture; a career heat rebuilt by WS13 (128958) allocates its riders out of order
// (docs/ctm-events-in-world.md section 6). rosterOrder(manifest)[k] = the record block of roster slot k + 1 (identity when unknown).
export function rosterOrder(manifest) {
  const others = (manifest.others || []).map(Number), human = Number(manifest.rider);
  const roster = (manifest.roster || []).map(Number).filter((a) => a !== human);
  const order = roster.map((a) => others.indexOf(a));
  return order.length === others.length && order.every((j) => j >= 0) ? order : others.map((_, k) => k);
}

export function readAiCapture(binPath) {
  const manifest = JSON.parse(fs.readFileSync(binPath.replace(/\.bin$/, '.capture.json'), 'utf8'));
  const L = manifest.layout, A = L.ai_state;
  if (!A) throw new Error(`${binPath}: not an --ai-state capture (manifest layout.ai_state missing)`);
  const RECORD = manifest.record;
  const raw = fs.readFileSync(binPath);
  const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  const roster = (manifest.roster || []).map(Number), others = manifest.others.map(Number);
  const owners = (manifest.others_owners || []).map(Number), humanActor = Number(manifest.rider), humanOwner = Number(manifest.human_owner);
  const pathBank0 = Number(manifest.ai_path_bank);
  const order = rosterOrder(manifest), slotOfBlock = [];   // record block -> roster slot (identity in Single Event captures)
  order.forEach((j, k) => { slotOfBlock[j] = k; });
  // a0 of a marker -> which rider (actor or motion-owner component), -1 = human, k = computer rider slot k.
  const bases = [[humanActor, -1, 'actor'], [humanOwner, -1, 'owner'], ...others.map((a, k) => [a, slotOfBlock[k], 'actor']), ...owners.map((o, k) => [o, slotOfBlock[k], 'owner'])];
  const riderOf = (a0) => { let best = null; for (const [b, k, kind] of bases) if (a0 >= b && a0 - b < 0x1000 && (!best || b > best[0])) best = [b, k, kind]; return best ? { slot: best[1], kind: best[2], offset: a0 - best[0] } : null; };
  // Next heat (local/ctm-events/ws13_capture.py, manifest.ws13.relisted): WS13's 128958 makes the next round's riders at new addresses and
  // the run lists their blocks again (ascending) from that record; their roster order is the game roster (C+0x28) the records hold then.
  const relist = manifest.ws13?.relisted?.[0] ?? null, heatFrom = relist ? relist.record : Infinity;
  const heat = relist ? (() => { const others2 = relist.game.map(Number), owners2 = others2.map((a) => a - 0xE10), at = heatFrom * RECORD, g = A.game_info_00_a0;
    const roster2 = Array.from({ length: 6 }, (_, j) => dv.getUint32(at + g + 0x28 + 4 * j, true)).filter((a) => a !== humanActor);
    const order2 = roster2.map((a) => others2.indexOf(a)); if (!order2.every((j) => j >= 0)) throw new Error(`${binPath}: the next round's roster is not the re-listed blocks`);
    const slot2 = []; order2.forEach((j, k) => { slot2[j] = k; });
    const bases2 = [[humanActor, -1, 'actor'], [humanOwner, -1, 'owner'], ...others2.map((a, k) => [a, slot2[k], 'actor']), ...owners2.map((o, k) => [o, slot2[k], 'owner'])];
    const riderOf2 = (a0) => { let best = null; for (const [b, k, kind] of bases2) if (a0 >= b && a0 - b < 0x1000 && (!best || b > best[0])) best = [b, k, kind]; return best ? { slot: best[1], kind: best[2], offset: a0 - best[0] } : null; };
    return { others: others2, order: order2, riderOf: riderOf2 }; })() : null;
  const records = [];
  for (let at = 0; at + RECORD <= raw.length; at += RECORD) {
    const u = (o) => dv.getUint32(at + o, true), i32 = (o) => dv.getInt32(at + o, true), f = (o) => dv.getFloat32(at + o, true);
    const fv = (o, n) => Array.from({ length: n }, (_, k) => f(o + 4 * k));
    const view = (o, n) => new Uint8Array(raw.buffer, raw.byteOffset + at + o, n);
    const rider = (off) => f(32 + off - 0x100);
    const human = {
      word0: u(8), word1: u(12), mode: u(16), control: u(20), padCalls: u(24), scriptIndex: u(28),
      position: [rider(0x110), rider(0x114), rider(0x118)], quat: [rider(0x120), rider(0x124), rider(0x128), rider(0x12c)],
      velocity: [rider(0x1e0), rider(0x1e4), rider(0x1e8)], turn: rider(0x1f0), brake: rider(0x214), crouch: rider(0x220), speedLimit: rider(0x2e4),
      normal: [rider(0x370), rider(0x374), rider(0x378)],
      remaining: rider(0x4d0), best: rider(0x4d4), timeScale: rider(0x300),
      pairRecords: pairs(A.human_actor_000_100), rank: i32(A.human_actor_000_100 + 0xec),
      raw: { actor_000_100: view(A.human_actor_000_100, 0x100), actor_100_b40: view(L.rider_100_b40, 0xa40), owner_00_40: view(L.owner_00_40, 0x40), owner_de0_e00: view(L.owner_de0_e00, 0x20) },
    };
    function pairs(base) {
      return Array.from({ length: 6 }, (_, j) => { const o = base + 36 * j;
        return { enabled: u(o), human: u(o + 4), distance: f(o + 8), bearing: f(o + 12), t10: u(o + 16), t14: u(o + 20), t18: u(o + 24), t1c: u(o + 28), t20: u(o + 32) }; });
    }
    const ai = [], inHeat = heat && at / RECORD >= heatFrom, riderAt = inHeat ? heat.riderOf : riderOf, orderAt = inHeat ? heat.order : order, othersAt = inHeat ? heat.others : others;
    for (let k = 0; k < othersAt.length; k++) {
      const b = A.base + k * A.stride, s = A.slot_fields;
      const act = (off) => b + s.actor_000_b40 + off, oc = (off) => b + s.owner_de0_f50 + off - 0xde0, n = b + s.npc_words_w0_w1_tick_calls;
      const ab8 = u(act(0xab8));
      ai.push({
        actor: othersAt[k], position: fv(act(0x110), 3), velocity: fv(act(0x1e0), 3), quaternion: fv(act(0x120), 4),
        motionMode: u(oc(0xde0)), controlState: u(oc(0xde4)),
        words: [u(n), u(n + 4)], wordsTick: u(n + 8), providerCalls: u(n + 12),
        pairRecords: pairs(act(0)), rank: i32(act(0xec)), peerF0: i32(act(0xf0)), peerF8: i32(act(0xf8)),
        pacing: [f(act(0xdc)), f(act(0xe0)), u(act(0xe4))],
        remaining: f(act(0x4d0)), best: f(act(0x4d4)), courseLength4d8: f(act(0x4d8)), finish470: f(act(0x470)), finishTicks478: u(act(0x478)),
        timeScale: f(act(0x300)), routePathAddress: ab8, routePath: ab8 >= pathBank0 ? (ab8 - (u(A.globals_pathbank_coursepaths_game_humanowner) || pathBank0)) / 64 : ab8,
        behaviour: { f44: u(oc(0xf44)), f48: u(oc(0xf48)) },
        raw: { actor_000_b40: view(act(0), 0xb40), owner_00_40: view(b + s.owner_00_40, 0x40), owner_1c0_300: view(b + s.owner_1c0_300, 0x140), owner_de0_f50: view(b + s.owner_de0_f50, 0x170) },
      });
    }
    const R = A.rng_draws_total_marka0_markra_npcunmatched, draws = u(R);
    const log = Array.from({ length: Math.min(draws, A.rng_log_entries) }, (_, i) => { const o = A.rng_log + 16 * i, ma0 = u(o + 8), mra = u(o + 12);
      return { ra: u(o), leafRa: u(o + 4), markerA0: ma0, markerRa: mra, pass: PASS_BY_RA[mra] ?? null, rider: riderAt(ma0) }; });
    records.push({
      seq: u(0), tick: u(4), human, ai: orderAt.map((j) => ai[j]),
      rng: { words: Array.from({ length: 6 }, (_, k) => u(L.shared_rng_6 + 4 * k)), draws, total: u(R + 4), logged: log.length, truncated: draws > log.length,
        markerAtRecord: { a0: u(R + 8), ra: u(R + 12) }, npcUnmatched: u(R + 16), log },
      game: { pointer: u(A.globals_pathbank_coursepaths_game_humanowner + 8), aiPathBank: u(A.globals_pathbank_coursepaths_game_humanowner), coursePaths: u(A.globals_pathbank_coursepaths_game_humanowner + 4),
        tick: u(A.game_info_00_a0 + 8), roster: Array.from({ length: 6 }, (_, j) => u(A.game_info_00_a0 + 0x28 + 4 * j)), riderCount: u(A.game_info_00_a0 + 0x78), raw: view(A.game_info_00_a0, 0xa0) },
    });
  }
  // (a CTM run from free ride through the event, manifest.ai_dynamic: the Continue's 1297C8(C, 1) restarts the game tick at 0; through the
  // results and Next heat (local/ctm-events/caps/c0a-ws13) also the auto replay's start (tick 1), WS13 and its grid placement (0))
  for (let i = 1; i < records.length; i++) if (records[i].tick !== records[i - 1].tick + 1 && !(manifest.ai_dynamic && records[i].tick <= (manifest.ws13 ? 1 : 0))) throw new Error(`tick gap at record ${i}`);
  return { manifest, records, roster, others: order.map((j) => others[j]), owners: owners.length ? order.map((j) => owners[j]) : owners, riderOf };
}

// Containing original function for an address (local/output/sub_XXXXXXXX_0x....cpp names give function starts).
export function functionIndex(outputDir = path.join(path.dirname(fileURLToPath(import.meta.url)), '../local/output')) {
  let starts = [];
  try { starts = fs.readdirSync(outputDir).map((n) => /^sub_([0-9A-Fa-f]{8})_/.exec(n)).filter(Boolean).map((m) => parseInt(m[1], 16)).sort((a, b) => a - b); } catch { /* no recompiled output */ }
  return (address) => { let lo = 0, hi = starts.length - 1, best = null; while (lo <= hi) { const mid = (lo + hi) >> 1; if (starts[mid] <= address) { best = starts[mid]; lo = mid + 1; } else hi = mid - 1; } return best; };
}

function summarize(binPath, asJson) {
  const { manifest, records, others } = readAiCapture(binPath);
  const fnOf = functionIndex();
  const out = { capture: binPath, records: records.length, firstTick: records[0]?.tick, lastTick: records.at(-1)?.tick, isolated: manifest.isolated_from_computer_riders, ai: [], rng: {} };
  for (let k = 0; k < others.length; k++) {
    const r0 = records[0].ai[k]; let moved = null, finish = null; const hist = {}, ranks = [];
    for (const r of records) {
      const a = r.ai[k];
      if (moved === null && a.position.some((v, i) => v !== r0.position[i])) moved = r.tick;
      hist[a.controlState] = (hist[a.controlState] || 0) + 1;
      if (finish === null && a.finishTicks478) finish = { tick: r.tick, finish470: a.finish470, finishTicks478: a.finishTicks478 };
      if (!ranks.length || ranks.at(-1).rank !== a.rank) ranks.push({ tick: r.tick, rank: a.rank });
    }
    const wordsLag = records.slice(1).filter((r) => r.ai[k].providerCalls).map((r) => r.tick - r.ai[k].wordsTick);
    out.ai.push({ slot: k, actor: hex(others[k]), firstMoveTick: moved, controlStates: hist, finish, rankChanges: ranks,
      npcWordLag: [...new Set(wordsLag)], providerCallsPerRecord: [...new Set(records.slice(1).map((r) => r.ai[k].providerCalls))] });
  }
  const byRa = new Map(); let total = 0, logged = 0, truncated = 0; const perRecord = {};
  const who = (e) => !e.rider ? (e.markerA0 ? hex(e.markerA0) : '-') : e.rider.slot < 0 ? `human(${e.rider.kind}+${hex(e.rider.offset)})` : `ai${e.rider.slot}(${e.rider.kind}+${hex(e.rider.offset)})`;
  for (const r of records) {
    total += r.rng.draws; logged += r.rng.logged; if (r.rng.truncated) truncated++;
    perRecord[r.rng.draws] = (perRecord[r.rng.draws] || 0) + 1;
    for (const e of r.rng.log) {
      const key = e.ra; let row = byRa.get(key);
      if (!row) byRa.set(key, row = { ra: hex(e.ra), function: fnOf(e.ra) !== null ? hex(fnOf(e.ra)) : null, leaf: new Set(), count: 0, riders: {}, passes: {}, firstTick: r.tick });
      row.count++; row.leaf.add(hex(e.leafRa));
      const w = e.rider ? (e.rider.slot < 0 ? 'human' : `ai${e.rider.slot}`) : who(e); row.riders[w] = (row.riders[w] || 0) + 1;
      const p = e.pass !== null ? hex(e.pass) : `ra ${hex(e.markerRa)} (${fnOf(e.markerRa) !== null ? hex(fnOf(e.markerRa)) : '?'})`; row.passes[p] = (row.passes[p] || 0) + 1;
    }
  }
  const words5 = records.map((r) => r.rng.words[5]);
  const consistent = records.slice(1).every((r, i) => ((r.rng.words[5] - words5[i]) >>> 0) === r.rng.draws);
  out.rng = { totalDraws: total, logged, recordsTruncated: truncated, drawsPerRecordHistogram: perRecord, drawCountMatchesWord5: consistent,
    callers: [...byRa.values()].sort((a, b) => b.count - a.count).map((r) => ({ ...r, leaf: [...r.leaf] })) };
  if (asJson) { console.log(JSON.stringify(out, null, 2)); return; }
  console.log(`${binPath}: ${out.records} records, ticks ${out.firstTick}..${out.lastTick}, isolated=${out.isolated}`);
  for (const a of out.ai) {
    console.log(`AI${a.slot} ${a.actor}: first move tick ${a.firstMoveTick}, finish ${a.finish ? JSON.stringify(a.finish) : 'none'}, NPC word lag ${a.npcWordLag.join('/')} (calls/record ${a.providerCallsPerRecord.join('/')})`);
    console.log(`   control states ${Object.entries(a.controlStates).map(([s, n]) => `${s}:${n}`).join(' ')}`);
    console.log(`   rank ${a.rankChanges.map((c) => `${c.rank}@${c.tick}`).join(' ')}`);
  }
  console.log(`RNG: ${total} draws (${logged} logged, ${truncated} records over the ${manifest.layout.ai_state.rng_log_entries}-entry log), per-record counts ${JSON.stringify(perRecord)}, draws == word5 delta: ${consistent}, unmatched NPC provider returns: ${Math.max(...records.map((r) => r.rng.npcUnmatched))}`);
  console.log('count  caller-ra   function    riders                              passes');
  for (const c of out.rng.callers.slice(0, 40))
    console.log(`${String(c.count).padStart(5)}  ${c.ra.padEnd(10)} ${String(c.function).padEnd(10)}${c.leaf.some((l) => l !== c.ra) ? ` via ${c.leaf.join('/')}` : ''}  ${Object.entries(c.riders).map(([k, n]) => `${k}:${n}`).join(' ').padEnd(34)}  ${Object.entries(c.passes).map(([k, n]) => `${k}:${n}`).join(' ')}`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === path.resolve(process.argv[1])) {
  const bin = process.argv[2];
  if (!bin) { console.error('usage: node web/ps2-capture-ai.mjs RUN.bin [--json] | RUN.bin --rng-order GATE.bin'); process.exit(2); }
  if (process.argv.includes('--rng-order')) writeRngOrder(bin, process.argv[process.argv.indexOf('--rng-order') + 1]);
  else summarize(bin, process.argv.includes('--json'));
}

// --rng-order GATE.bin: GATE.rng-order.json for compare-ps2-capture.mjs --sync-rng, from an --ai-state re-run of the same
// baseline + script (every record's tick, rider window and shared-RNG words must equal GATE's). Per record index i of GATE
// with draws between records i and i+1: one character per draw in PS2 order, 'O' = a computer rider's pass (not replayed
// by the comparer), 'C' = the human's controller pass 121068, 'm' = any other draw (human motion/progress passes, world).
function writeRngOrder(aiBin, gateBin) {
  const { records, manifest } = readAiCapture(aiBin);
  const gate = JSON.parse(fs.readFileSync(gateBin.replace(/\.bin$/, '.capture.json'), 'utf8')), GR = gate.record || 8192, raw = fs.readFileSync(gateBin);
  const n = Math.floor(raw.length / GR); if (records.length < n) throw new Error(`${aiBin}: ${records.length} records < ${n}`);
  const AR = manifest.record, aiRaw = fs.readFileSync(aiBin);
  for (let i = 0; i < n; i++) if (!raw.subarray(i * GR + 4, i * GR + 8).equals(aiRaw.subarray(i * AR + 4, i * AR + 8)) || !raw.subarray(i * GR + 32, i * GR + 32 + 0xa40).equals(aiRaw.subarray(i * AR + 32, i * AR + 32 + 0xa40))
    || !raw.subarray(i * GR + 8896, i * GR + 8920).equals(aiRaw.subarray(i * AR + 8896, i * AR + 8920))) throw new Error(`record ${i} differs from ${gateBin}`);
  const order = {};
  for (let i = 0; i + 1 < n; i++) { const r = records[i + 1]; if (!r.rng.draws) continue; if (r.rng.truncated) throw new Error(`record ${i + 1}: RNG log truncated`);
    order[i] = r.rng.log.map((e) => e.rider && e.rider.slot >= 0 ? 'O' : e.rider && e.pass === 0x121068 ? 'C' : 'm').join(''); }
  const out = gateBin.replace(/\.bin$/, '.rng-order.json');
  fs.writeFileSync(out, JSON.stringify({ source: aiBin, records: n, order }));
  console.log(`${out}: ${Object.keys(order).length} records with draws`);
}
