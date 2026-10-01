// The browser's memory card: every piece of player progress the port keeps, in localStorage (docs/characters.md
// "Saving progress").
//
// Why localStorage and not cookies: a cookie is sent to the server with every request (each of the ~100 MB of
// /assets fetches behind the password gate), is capped at ~4 KB per cookie (the career save alone is larger), and
// would need server-side code to be of any use. localStorage stays in the browser, holds ~5 MB per origin, and is
// read synchronously at start-up, which the original's auto-load on boot needs. (IndexedDB would add nothing for a
// save of a few tens of KB but an asynchronous API.) Everything is per origin: the hosted site keeps one
// save per browser profile; Export / Import in Options > Save/Load moves it between browsers.
//
// Robustness:
//  - Versioned keys (ssx3.career.v2, ssx3.outfit.free.v1, ...) and a versioned save file (SAVE_FORMAT/SAVE_VERSION);
//    older keys migrate on first load (career-save.js v1 -> v2).
//  - Atomic writes: the value goes to '<key>~tmp' first, then to the key, then the temp copy is removed. A write that
//    dies half way (tab killed, quota error) leaves either the old value or a complete temp copy, which the next read
//    finishes (recover). setItem itself never stores half a value.
//  - Blocked storage (private windows on some browsers, sandboxed frames, disabled site data) and quota errors never
//    throw: reads fall back to defaults and play continues from memory for the session (memoryStorage()).
export const SAVE_FORMAT = 'ssx3-save', SAVE_VERSION = 2;
export const TMP = '~tmp';
export const SELECTION_KEY = 'ssx3.selection.v1';
// The keys that make up a player's save file (Export/Import). Session-only or per-device keys are left out:
// ssx3.mp.token (online identity), ssx3.diag.last (crash marker), ssx3.loadingHint (sessionStorage).
export const SAVE_KEYS = Object.freeze([
  'ssx3.career.v2', 'ssx3.career.v1',                 // Conquer the Mountain (web/career-save.js); v1 = pre-migration
  'ssx3.outfit.free.v1', 'ssx3.outfit.v1',            // free-play outfits (web/wardrobe.js), first-version outfit
  'ssx3.relationships.v1',                            // rider relationships (web/ai-race.js)
  'ssx3.cheatCharacters', 'ssx3.playerName', 'ssx3.musicMode',
  SELECTION_KEY,                                      // last rider / peak / mode / event
  'ssx3.feOptions', 'ssx3.widescreen', 'ssx3.keyboard', 'ssx3.soundMode', 'ssx3.audio',
  'ssx3.touch', 'ssx3.quality',
]);

const memory = new Map();
// An in-memory stand-in with the Storage methods the save code uses.
export function memoryStorage(map = memory) {
  return { getItem: (k) => (map.has(k) ? map.get(k) : null), setItem: (k, v) => { map.set(k, String(v)); }, removeItem: (k) => { map.delete(k); },
    key: (i) => [...map.keys()][i] ?? null, get length() { return map.size; } };
}
// localStorage when it can be read (the property itself throws in sandboxed frames / with site data blocked); a
// full or read-only one is still returned so the existing save loads, and its writes report false.
export function storage() {
  try { const s = globalThis.localStorage; if (!s) return memoryStorage(); s.getItem('ssx3.probe'); return s; }
  catch { return memoryStorage(); }
}
export function storageWorks(s = globalThis.localStorage) { try { if (!s) return false; s.setItem('ssx3.probe', '1'); s.removeItem('ssx3.probe'); return true; } catch { return false; } }

// Atomic write of a string. Returns true when the key holds the new value.
export function writeText(key, text, s = storage()) {
  if (!s) return false;
  try {
    let staged = false;
    try { s.setItem(key + TMP, text); staged = s.getItem(key + TMP) === text; } catch { staged = false; }
    s.setItem(key, text);                       // atomic per key; with no room for the temp copy this is the only write
    if (staged) try { s.removeItem(key + TMP); } catch {}
    return true;
  } catch { return false; }
}
export function writeJSON(key, value, s = storage()) { let text; try { text = JSON.stringify(value); } catch { return false; } return writeText(key, text, s); }
// Read a JSON value, finishing an interrupted write: a complete temp copy wins over the key (it is the newer write),
// a broken one is dropped. validate(value) -> value or null.
export function readJSON(key, validate = (v) => v, s = storage()) {
  if (!s) return null;
  const parse = (text) => { if (text == null) return null; try { return validate(JSON.parse(text)) ?? null; } catch { return null; } };
  let tmpText = null; try { tmpText = s.getItem(key + TMP); } catch {}
  if (tmpText != null) {
    const v = parse(tmpText);
    try { if (v != null) s.setItem(key, tmpText); s.removeItem(key + TMP); } catch {}
    if (v != null) return v;
  }
  try { return parse(s.getItem(key)); } catch { return null; }
}
export function removeKey(key, s = storage()) { try { s?.removeItem(key); s?.removeItem(key + TMP); return true; } catch { return false; } }

// ---- last selection (rider, Single Event peak / mode / event) ----
export function loadSelection(s = storage()) { return readJSON(SELECTION_KEY, (v) => (v && typeof v === 'object' ? v : null), s); }
export function saveSelection(patch, s = storage()) { const v = { ...(loadSelection(s) || {}), ...patch, at: Date.now() }; writeJSON(SELECTION_KEY, v, s); return v; }

// ---- the save file (Options > Save/Load > Export / Import) ----
export function exportSave(s = storage(), now = Date.now()) {
  const keys = {};
  for (const k of SAVE_KEYS) { try { const v = s?.getItem(k); if (v != null) keys[k] = v; } catch {} }
  if (keys['ssx3.career.v2']) delete keys['ssx3.career.v1'];      // migrated: v1 is only kept when v2 does not exist
  return { format: SAVE_FORMAT, version: SAVE_VERSION, exportedAt: new Date(now).toISOString(), keys };
}
// Validate and apply a save file. Unknown keys are ignored; every value must be a string (the stored form) and the
// JSON ones must parse. The career goes through migrateCareer (a v1 export lands as v2). Returns {ok, keys, error}.
export function importSave(file, s = storage(), { migrateCareer = null } = {}) {
  if (!file || typeof file !== 'object' || file.format !== SAVE_FORMAT) return { ok: false, error: 'Not an SSX 3 save file.' };
  if (!(file.version >= 1 && file.version <= SAVE_VERSION)) return { ok: false, error: `Save file version ${file.version} is not supported.` };
  const src = file.keys && typeof file.keys === 'object' ? file.keys : null; if (!src) return { ok: false, error: 'The save file is empty.' };
  const plain = new Set(['ssx3.playerName', 'ssx3.musicMode', 'ssx3.widescreen', 'ssx3.keyboard', 'ssx3.soundMode']);
  const out = {};
  for (const k of SAVE_KEYS) {
    if (!(k in src)) continue; const v = src[k]; if (typeof v !== 'string' || v.length > 2_000_000) return { ok: false, error: `Bad value for ${k}.` };
    if (!plain.has(k)) { try { JSON.parse(v); } catch { return { ok: false, error: `Damaged data in ${k}.` }; } }
    out[k] = v;
  }
  if (out['ssx3.career.v1'] && !out['ssx3.career.v2'] && migrateCareer) {
    const m = migrateCareer(JSON.parse(out['ssx3.career.v1'])); if (!m) return { ok: false, error: 'The career in the save file is damaged.' };
    out['ssx3.career.v2'] = JSON.stringify(m); delete out['ssx3.career.v1'];
  }
  if (out['ssx3.career.v2'] && migrateCareer && !migrateCareer(JSON.parse(out['ssx3.career.v2']))) return { ok: false, error: 'The career in the save file is damaged.' };
  if (!Object.keys(out).length) return { ok: false, error: 'The save file has no progress in it.' };
  const written = [];
  for (const [k, v] of Object.entries(out)) { if (!writeText(k, v, s)) return { ok: false, error: 'Browser storage is full or blocked.', keys: written }; written.push(k); }
  if (out['ssx3.career.v2']) removeKey('ssx3.career.v1', s);
  return { ok: true, keys: written };
}
export function saveFileName(now = new Date()) {
  const p = (n) => String(n).padStart(2, '0');
  return `ssx3-save-${now.getFullYear()}${p(now.getMonth() + 1)}${p(now.getDate())}-${p(now.getHours())}${p(now.getMinutes())}.json`;
}
// Browser helpers: download the save file / pick one to import (resolves the parsed object or null).
export function downloadSave(s = storage()) {
  const blob = new Blob([JSON.stringify(exportSave(s), null, 1)], { type: 'application/json' }), url = URL.createObjectURL(blob), a = document.createElement('a');
  a.href = url; a.download = saveFileName(); document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 10000);
  return a.download;
}
export function pickSaveFile() {
  return new Promise((resolve) => {
    const input = document.createElement('input'); input.type = 'file'; input.accept = 'application/json,.json'; input.style.display = 'none';
    input.onchange = async () => { const f = input.files?.[0]; input.remove(); if (!f) return resolve(null); try { resolve(JSON.parse(await f.text())); } catch { resolve({ invalid: true }); } };
    document.body.appendChild(input); input.click();
  });
}
