// The one reader of navigator.getGamepads() (race input, menus, control hints, rumble, cutscene skip, audio unlock,
// diagnostics). Every poll re-reads the whole array: Chromium returns snapshots, so a Gamepad kept from an earlier
// call never changes, while Firefox's objects are live, so nothing here keeps a reference to a raw button.
//
// All slots are scanned. On Windows the controller is often not in slot 0: a headset or wheel driver, Steam Input's
// virtual pad, vJoy / ViGEm, or a pad unplugged earlier can hold the lower slots. The active pad is the one used most
// recently (a button going down, or an axis moving half its travel away from where it rested when first seen); before
// any input, a 'standard' pad is preferred, then anything shaped like a gamepad, then the lowest slot. Browsers only
// expose a pad after its first button press, and that press counts as a use (so "Press START" works on first touch).
//
// Each pad is mapped to the W3C standard layout by web/gamepad-map.js (standard pads are copied unchanged) with the
// player's remap for that pad (Options > Controller, localStorage 'ssx3.padmap') on top.
import { layoutFor, compileLayout, withRemap, mapPad, newStandardPad, newCalibration, calibrate, padKey, hatBits, parsePadId } from './gamepad-map.js';

const REMAP_KEY = 'ssx3.padmap', POLL_MS = 2, USE_AXIS = 0.5;
const nowMs = () => globalThis.performance?.now?.() ?? Date.now();
function browserPads() { try { return globalThis.navigator?.getGamepads?.() || []; } catch { return []; } }

const S = { source: browserPads, clock: nowMs, entries: new Map(), active: null, polledAt: -Infinity, serial: 0, listeners: new Set(), remaps: null, remapFrozen: false };

function loadRemaps() { try { return JSON.parse(globalThis.localStorage?.getItem(REMAP_KEY) || '{}') || {}; } catch { return {}; } }
function remaps() { return S.remaps ??= loadRemaps(); }
function emit(type, entry) { for (const fn of S.listeners) { try { fn(type, entry); } catch {} } }

function makeEntry(raw, slot) {
  const e = { slot, id: raw.id ?? '', key: padKey(raw), raw, cal: newCalibration(), std: newStandardPad(), rest: [], hats: new Set(),
    prevB: [], prevA: [], lastUse: 0, usedNow: false, connectedAt: S.clock(), layout: null, compiled: null };
  calibrate(e.cal, raw);
  const axes = raw.axes || [];
  for (let i = 0; i < axes.length; i++) { const v = +axes[i]; e.rest[i] = Number.isFinite(v) && Math.abs(v) <= 1 ? v : 0; if (Math.abs(v) > 1.05) e.hats.add(i); }
  applyLayout(e);
  return e;
}
function applyLayout(e) {
  e.baseLayout = layoutFor(e.raw);
  e.layout = withRemap(e.baseLayout, remaps()[e.key]);
  e.compiled = compileLayout(e.layout);
  for (const src of e.layout.buttons) for (const m of String(src).matchAll(/h(\d+):/g)) e.hats.add(+m[1]);
}
// Raw input edges on this snapshot (buttons pressed, axes leaving their rest by half the travel, hat directions).
function detectUse(e, raw) {
  let used = false;
  const buttons = raw.buttons || [], axes = raw.axes || [];
  for (let i = 0; i < buttons.length; i++) {
    const b = buttons[i], on = typeof b === 'number' ? b > 0.5 : !!b && (b.pressed || +b.value > 0.5);
    if (on && !e.prevB[i]) used = true; e.prevB[i] = on;
  }
  for (let i = 0; i < axes.length; i++) {
    const v = +axes[i]; if (!Number.isFinite(v)) continue;
    if (Math.abs(v) > 1.05) e.hats.add(i);
    const on = e.hats.has(i) ? hatBits(v) !== 0 : Math.abs(v - (e.rest[i] ?? 0)) > USE_AXIS;
    if (on && !e.prevA[i]) used = true; e.prevA[i] = on;
  }
  return used;
}
function gamepadShaped(e) { return (e.raw.buttons?.length ?? 0) >= 4 && (e.raw.axes?.length ?? 0) >= 2; }
function choose() {
  let best = null;
  const current = S.active && S.entries.get(S.active.slot) === S.active ? S.active : null;
  if (current?.usedNow) best = current;                       // two pads mirroring each other (Steam Input) do not flap
  if (!best) for (const e of S.entries.values()) if (e.lastUse && (!best || e.lastUse > best.lastUse)) best = e;
  if (!best && current) best = current;
  if (!best) for (const e of S.entries.values()) {
    const score = (x) => (x.raw.mapping === 'standard' ? 2 : 0) + (gamepadShaped(x) ? 1 : 0);
    if (!best || score(e) > score(best) || (score(e) === score(best) && e.slot < best.slot)) best = e;
  }
  if (best !== S.active) { S.active = best; emit('active', best); }
}

// Re-read every slot (at most once per POLL_MS unless forced). Returns the active pad in the standard layout, or null.
export function pollPads(force = false) {
  const now = S.clock();
  if (!force && now >= S.polledAt && now - S.polledAt < POLL_MS) return S.active?.std ?? null;
  S.polledAt = now;
  const list = S.source() || [], seen = new Set();
  for (let i = 0; i < list.length; i++) {
    const raw = list[i]; if (!raw || raw.connected === false) continue;
    const slot = Number.isInteger(raw.index) ? raw.index : i; if (seen.has(slot)) continue; seen.add(slot);
    let e = S.entries.get(slot);
    if (e && e.id !== (raw.id ?? '')) { S.entries.delete(slot); emit('disconnect', e); e = null; }
    const fresh = !e;
    if (fresh) { e = makeEntry(raw, slot); S.entries.set(slot, e); }
    e.raw = raw; calibrate(e.cal, raw);
    e.usedNow = detectUse(e, raw);
    if (e.usedNow) e.lastUse = ++S.serial;
    mapPad(raw, e.compiled, e.cal, e.std);
    if (fresh) emit('connect', e);
    if (e.usedNow) emit('use', e);
  }
  for (const [slot, e] of S.entries) if (!seen.has(slot)) { S.entries.delete(slot); emit('disconnect', e); }
  choose();
  return S.active?.std ?? null;
}
// The active pad (standard layout) from the last poll, polling first when that is stale.
export function activePad() { return pollPads(); }
// The active pad's latest raw Gamepad snapshot (vibration lives on it).
export function activeRawPad() { pollPads(); return S.active?.raw ?? null; }
export function activeEntry() { pollPads(); return S.active; }
// Make this pad the active one (Options > Controller Settings > Gamepad, Left / Right), as if it had just been used.
export function selectPad(entry) { if (!entry || S.entries.get(entry.slot) !== entry) return; for (const e of S.entries.values()) e.usedNow = false; entry.lastUse = ++S.serial; choose(); }
// The pads seen by the last poll, by slot (does not poll: call pollPads() first when it may be stale).
export function connectedPads() { return [...S.entries.values()].sort((a, b) => a.slot - b.slot); }
// Listener: (type, entry) with type 'connect' | 'disconnect' | 'use' | 'active'. Returns an unsubscribe function.
export function onPads(fn) { S.listeners.add(fn); return () => S.listeners.delete(fn); }
export function padSummary(e) {
  if (!e) return null;
  const { vendor, product, name } = parsePadId(e.id);
  return { slot: e.slot, name: name.slice(0, 60), vendor, product, mapping: e.raw.mapping || '', buttons: e.raw.buttons?.length ?? 0, axes: e.raw.axes?.length ?? 0,
    layout: e.layout?.name ?? '', vibration: !!(e.raw.vibrationActuator || e.raw.hapticActuators?.length) };
}

// ---- remaps ----------------------------------------------------------------------------------------------------------
export function padRemap(entryOrKey) { const k = typeof entryOrKey === 'string' ? entryOrKey : entryOrKey?.key; return k ? remaps()[k] ?? null : null; }
export function saveRemap(entry, remap) {
  if (!entry?.key) return;
  const all = remaps();
  if (remap && (Object.keys(remap.buttons || {}).length || Object.keys(remap.axes || {}).length)) all[entry.key] = remap; else delete all[entry.key];
  try { globalThis.localStorage?.setItem(REMAP_KEY, JSON.stringify(all)); } catch {}
  for (const e of S.entries.values()) if (e.key === entry.key) applyLayout(e);
  emit('remap', entry);
}
export function clearRemap(entry) { saveRemap(entry, null); }
// The raw control a player just used on this pad, for "press the button for X": the first raw button that went down,
// else an axis that moved past `threshold` from its rest ('aN+' / 'aN-'), else a hat direction ('hN:up'...). Pass the
// previous result of rawControls() as `before` so only new presses count.
export function rawControls(entry, threshold = 0.6) {
  const out = new Set(), raw = entry?.raw; if (!raw) return out;
  (raw.buttons || []).forEach((b, i) => { if (typeof b === 'number' ? b > 0.5 : b && (b.pressed || +b.value > 0.5)) out.add('b' + i); });
  (raw.axes || []).forEach((a, i) => {
    const v = +a; if (!Number.isFinite(v)) return;
    if (entry.hats.has(i) || Math.abs(v) > 1.05) { const bits = hatBits(v); for (const [d, bit] of [['up', 1], ['down', 2], ['left', 4], ['right', 8]]) if (bits & bit) out.add(`h${i}:${d}`); return; }
    const d = v - (entry.rest[i] ?? 0);
    if (Math.abs(d) > threshold) out.add(`a${i}${d > 0 ? '+' : '-'}`);
  });
  return out;
}

// ---- tests / QA ----------------------------------------------------------------------------------------------------
// source() returns the getGamepads()-like array; clock() the time in ms.
export function setPadSource(source = browserPads, clock = nowMs) { S.source = source; S.clock = clock; resetPads(); }
export function resetPads() { S.entries.clear(); S.active = null; S.polledAt = -Infinity; S.serial = 0; S.remaps = null; }

if (typeof addEventListener === 'function') {
  // Hot-plug: re-read at once (the next poll would see it too; this keeps the hints and the active pad current).
  addEventListener('gamepadconnected', () => pollPads(true));
  addEventListener('gamepaddisconnected', () => pollPads(true));
}
