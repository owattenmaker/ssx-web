// Conquer the Mountain progress persistence (browser stand-in for the PS2 memory-card profile; web/save-store.js).
// Every access is guarded: private windows, blocked storage and quota errors fall back to an in-memory copy.
//
// Schema v2 (ssx3.career.v2): the v1 career object ({version, seed, records, riders{id: cash, earned, medals, best,
// events, attributes, level, peaks, lodgePeak, collected, gearFlags, ubers, songs, messages, ...}, roster, pending})
// with version 2, savedAt and schema-checked riders, written atomically (temp key, then the key). v1 saves
// (ssx3.career.v1) migrate on first load; the v1 key is left as it was, as a backup.
import { readJSON, writeJSON, removeKey, storage as defaultStorage } from './save-store.js';

export const CAREER_SAVE_KEY = 'ssx3.career.v2', CAREER_SAVE_V1 = 'ssx3.career.v1', CAREER_SAVE_VERSION = 2;
const isObject = (v) => !!v && typeof v === 'object' && !Array.isArray(v);

// v1 -> v2 (and v2 schema check). Returns the save or null when it is not a career save.
export function migrateCareer(data) {
  if (!isObject(data) || !(data.version === 1 || data.version === 2)) return null;
  const riders = isObject(data.riders) ? data.riders : {};
  for (const [id, r] of Object.entries(riders)) if (!isObject(r)) delete riders[id];
  const out = { ...data, riders, version: CAREER_SAVE_VERSION };
  if (data.version === 1) out.migratedFrom = 1;
  if (out.records != null && !Array.isArray(out.records)) out.records = null;
  return out;
}
const store = (s) => (s === undefined ? defaultStorage() : s);
export function loadSave(storage = undefined, key = CAREER_SAVE_KEY) {
  const s = store(storage); if (!s) return null;
  try {
    const v2 = readJSON(key, migrateCareer, s); if (v2) return v2;
    if (key !== CAREER_SAVE_KEY) return null;
    const v1 = readJSON(CAREER_SAVE_V1, migrateCareer, s);   // first load after the update: migrate
    if (v1) writeJSON(CAREER_SAVE_KEY, v1, s);
    return v1;
  } catch { return null; }
}
// savedAt: the write time (ms since 1970) the Load game row shows as date/time (web/fe-options.js saveDateTime);
// saves written before it have none and still load.
export function writeSave(data, storage = undefined, key = CAREER_SAVE_KEY) {
  const s = store(storage); if (!s || !isObject(data)) return false;
  try { data.version = CAREER_SAVE_VERSION; data.savedAt = Date.now(); return writeJSON(key, data, s); } catch { return false; }
}
export function clearSave(storage = undefined, key = CAREER_SAVE_KEY) {
  const s = store(storage); if (!s) return false;
  return removeKey(key, s) && (key !== CAREER_SAVE_KEY || removeKey(CAREER_SAVE_V1, s));
}
