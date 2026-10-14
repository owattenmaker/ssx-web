// pv coreCareerCascade (docs/core-migration.md M5d): the career's award cascade and its messages in the rules module
// (engine/career_cascade.hpp through web/rules_module.cpp career_kernel). web/career.js asks cascadeKernel() for complete1591E8 (with
// the event's messages), earnCash, exploreGoal and grantAward; it returns the kernel's result after writing the new save into the
// page's save in place (the objects the page holds stay the same objects), or undefined: the switch is off or the module or the
// session's generator is not there (the JS version runs), or FAILED: the call failed (nothing changed; the kernel is off for the rest
// of the session and the caller runs the JS version with the messages' hook steps the kernel would have made).
import { pv } from './pv-flags.js';
import { rulesModule } from './rules-module.js';
import { presentationDraw, presentationInModule } from './lineup.js';
import { bigChallengeRows } from './big-challenges.js';

const encoder = new TextEncoder();
const decoder = new TextDecoder();
// the static data last given to the module (by identity)
const given = { module: null, data: null, shop: undefined, messages: undefined, rows: undefined, failed: false };
export const FAILED = Symbol('cascade kernel failed');

// The module to use for this career, or null.
export function cascadeModule(career) {
  if (!pv('coreCareerCascade') || given.failed) return null;
  const m = rulesModule();
  if (!m || typeof m._career_kernel !== 'function') return null;
  // the cascade draws from the session's presentation generator: only when the career draws from it and it lives in the module
  if (career.presentation !== presentationDraw || !presentationInModule()) return null;
  return m;
}
function sendText(m, fn, text) {
  const bytes = encoder.encode(text);
  const p = m._malloc(bytes.length + 1);
  m.HEAPU8.set(bytes, p);
  m.HEAPU8[p + bytes.length] = 0;
  try {
    return fn(p);
  } finally {
    m._free(p);
  }
}
function giveData(m, career, messages) {
  const rows = bigChallengeRows();
  const shop = career.shop ?? null;
  const update = {};
  if (given.module !== m) {
    given.module = m;
    given.data = null;
    given.shop = undefined;
    given.messages = undefined;
    given.rows = undefined;
  }
  if (given.data !== career.data) {
    update.rules = career.data.rules;
    update.courses = career.data.courses;
    update.rewards = career.data.rewards ?? null;
  }
  if (given.shop !== shop) update.shop = shop;
  if (given.messages !== messages) update.messages = messages;
  if (given.rows !== rows) update.challengeRows = rows ?? null;
  if (Object.keys(update).length === 0) return true;
  const ok = sendText(m, (p) => m._career_data(p), JSON.stringify(update)) === 0;
  if (ok) {
    given.data = career.data;
    given.shop = shop;
    given.messages = messages;
    given.rows = rows;
  }
  return ok;
}
// The reward record (web/career.js clearRewardRecord) as JSON and back (the Maps' and Sets' orders kept).
function recordOut(r) {
  if (!r) return null;
  return {
    cash: r.cash,
    medal: r.medal,
    counts: [...r.counts.entries()],
    items: r.items.map((s) => [...s]),
    cheats: [...r.cheats.entries()],
    gear: [...r.gear],
    gearItem: r.gearItem ?? null
  };
}
function recordIn(o) {
  return {
    cash: o.cash,
    medal: o.medal,
    counts: new Map(o.counts),
    items: o.items.map((a) => new Set(a)),
    cheats: new Map(o.cheats),
    gear: new Set(o.gear),
    gearItem: o.gearItem
  };
}
// The kernel's save written into the page's save in place: objects and arrays stay the objects the page holds, new keys are added
// in the kernel's order, and a value JSON cannot carry (undefined, NaN) stays as the page had it.
export function syncInto(target, source) {
  if (Array.isArray(target)) {
    for (let k = 0; k < source.length; k++) target[k] = merge(target[k], source[k]);
    target.length = source.length;
    return target;
  }
  for (const k of Object.keys(source)) target[k] = merge(target[k], source[k]);
  for (const k of Object.keys(target)) if (!(k in source) && target[k] !== undefined) delete target[k];
  return target;
}
function merge(a, b) {
  if (b !== null && typeof b === 'object' && a !== null && typeof a === 'object' && Array.isArray(a) === Array.isArray(b)) return syncInto(a, b);
  if (a === b) return a;
  if (b === null && typeof a === 'number' && !Number.isFinite(a)) return a;
  return b;
}
// pv coreCareerLodge (docs/core-migration.md M5e): the module for the lodge's purchases (they draw nothing), or null. The gear, uber
// and song purchases need the shop (the JS versions throw without it).
export function lodgeModule(career, needsShop) {
  if (!pv('coreCareerLodge') || given.failed) return null;
  if (needsShop && !career.shop) return null;
  const m = rulesModule();
  return m && typeof m._career_kernel === 'function' ? m : null;
}
// One kernel call. args: the op's fields (web/rules_module.cpp career_kernel). Returns the kernel's output or undefined.
export function cascadeKernel(career, op, args, module = cascadeModule(career)) {
  const m = module;
  if (!m) return undefined;
  const host = career.messagesHost;
  const messages = host?.ready ? host.data : null;
  if (!giveData(m, career, messages)) {
    given.failed = true;
    return FAILED;
  }
  const input = { op, ...args, save: career.save, record: recordOut(career.rewardRecord), messages: !!messages };
  const p = sendText(m, (q) => m._career_kernel(q), JSON.stringify(input));
  if (!p) {
    console.warn('career kernel failed: the JS cascade runs from here on');
    given.failed = true;
    return FAILED;
  }
  const out = JSON.parse(decoder.decode(m.HEAPU8.subarray(p, p + m._career_out_size())));
  syncInto(career.save, out.save);
  // (the record object the page holds stays the same object)
  if (out.record) career.rewardRecord = Object.assign(career.rewardRecord ?? {}, recordIn(out.record));
  // the gear inventory the page keeps for one rider is made again from the save's flags
  career._gear = null;
  if (out.posts && host) host.kernelPosts?.(op);
  return out.result;
}
