// Selectable riders as the human (docs/characters.md). Each original rider package RIDER_<ID> carries a
// settings.json from tools/export_characters.py: the leaf differences of this character's own live human
// actor (its derived Snow Jam countdown savestate) from Zoe's, over the course initial.json, plus identity
// values the core takes outside initial.json. Zoe and Sam (the port's own rider, Zoe's gameplay profile)
// have no settings.json, so their settings stay the course initial.json exactly.
//
// A cheat character (ids 10..29) is a skin on the chosen base rider (setup slot +0x11 base, +0x12 cheat): the
// base rider's differences (stance, uber table, stats, weight) come first, then the skin's own (scale, rig contact,
// masks, hair slot) and its uber overrides (Stretch/Gutless/Canhuck Nose Grab, Snowballs Tail Grab). In Conquer
// the Mountain the human keeps the base rider's CHARDB scale (0x14EFA8 with 0x535C11 == 0).

const isObject = (v) => v && typeof v === 'object' && !Array.isArray(v);

// Deep merge: objects recurse, arrays and scalars replace (settings.json holds leaf differences only).
export function mergeCharacterSettings(base, overrides) {
  if (!isObject(overrides)) return base;
  const out = { ...base };
  for (const [key, value] of Object.entries(overrides)) out[key] = isObject(value) && isObject(base?.[key]) ? mergeCharacterSettings(base[key], value) : value;
  return out;
}

async function fetchSettings(pkg) {
  const response = await fetch(`/assets/${pkg}/settings.json`);
  // Zoe/Sam have none: a 404, or the dev server's HTML fallback page
  if (response.status === 404 || !(response.headers.get('content-type') || '').includes('json')) return null;
  if (!response.ok) throw new Error(`${pkg}/settings.json: ${response.status}`);
  return response.json();
}

// Compose a cheat skin over its base rider's document (either may be null = Zoe/Sam).
export function composeCheat(baseDoc, skinDoc, { career = false } = {}) {
  if (!skinDoc) return baseDoc;
  let skin = { ...(skinDoc.settings || {}) };
  if (career) {                               // Conquer the Mountain: the human keeps the base rider's scale
    if (skin.original_animation) { skin.original_animation = { ...skin.original_animation }; delete skin.original_animation.scale; }
    if (skin.original_landing?.profile) { skin.original_landing = { ...skin.original_landing, profile: { ...skin.original_landing.profile } }; delete skin.original_landing.profile.body_scale; }
    delete skin.original_event_start;          // the grid spot and pivot follow the scale: the base rider's
    if (skin.original_air_entry) { skin.original_air_entry = { ...skin.original_air_entry }; delete skin.original_air_entry.pivot; }
  }
  // The skin's own states ride on Zoe (regular). On a goofy base rider the original mirrors the air-entry pivot x and
  // grids the rider in reverse stance (derived state brodi-on-psymon: only these differ from the composition).
  if (baseDoc?.settings?.original_reset?.stance === 1) {
    if (skin.original_air_entry?.pivot) skin.original_air_entry = { ...skin.original_air_entry, pivot: [-skin.original_air_entry.pivot[0], ...skin.original_air_entry.pivot.slice(1)] };
    if (skin.original_event_start) skin.original_event_start = { ...skin.original_event_start, state: { ...skin.original_event_start.state, reverse_stance: true } };
  }
  return {
    character: skinDoc.character, kind: 'cheat', base: baseDoc?.character ?? 'zoe',
    settings: mergeCharacterSettings(baseDoc?.settings || {}, skin),
    uber_rows: skinDoc.uber_rows || [],
    // the channel-1 bone masks come from the skin's skeleton (11C298 bone lists); rider-pair weight from the base rider
    identity: { ...(skinDoc.identity || {}), pair: baseDoc?.identity?.pair ?? null },
  };
}

// Ubertrick Setup (web/fe-screens.js): a rider's chosen ubers that differ from its defaults, as grab-profile rows
// [set 1, category, row], per base rider id. The skin's own overrides still win (0x150198 returns them first).
const uberChoices = new Map();
export function setUberChoice(id, rows) { if (rows?.length) uberChoices.set(id, rows); else uberChoices.delete(id); }

// { settings, identity, uber_rows } for a riders.json entry (a cheat entry carries `base`), or null (Zoe/Sam).
// rider.uber_choice (pv careerRider, web/career-rider.js): the rows of the profile's current selection, resolved with the
// rider entry at each world / event load and run start; otherwise the rows the front-end screen last set.
export async function loadCharacter(rider) {
  const doc = withOutfit(await loadCharacterDoc(rider), rider), choice = rider && (rider.uber_choice ?? uberChoices.get(rider.kind === 'cheat' ? rider.base || 'zoe' : rider.id));
  return choice?.length ? { ...(doc || { settings: {} }), uber_rows: [...choice, ...(doc?.uber_rows || [])] } : doc;
}
// An Equip Gear outfit (web/wardrobe.js outfitRider) lays its skeleton-dependent values over the character's:
// bone_mask, secondary motion enables (0x11CF70) and the channel-1 masks (0x11C298).
function withOutfit(doc, rider) {
  if (!rider?.outfit_settings) return doc;
  return { ...(doc || {}), settings: mergeCharacterSettings(doc?.settings || {}, rider.outfit_settings), identity: { ...(doc?.identity || {}), ...(rider.outfit_identity || {}) } };
}
async function loadCharacterDoc(rider) {
  if (!rider?.package || rider.settings === false) return null;
  if (rider.kind === 'cheat') {
    const base = rider.base && !['zoe', 'sam'].includes(rider.base) ? await fetchSettings(`RIDER_${rider.base.toUpperCase()}`) : null;
    return composeCheat(base, await fetchSettings(rider.package), { career: !!rider.career });
  }
  return fetchSettings(rider.settings_package || rider.package);   // settings_package: Sam's outfit packages share RIDER_SAM's (web/wardrobe.js)
}

// The human's settings: course initial.json + this character's differences; the channel-1 masks
// rider+0x8C0/+0x8C8/+0x8D0 go to init_animation as original_rider_identity (web/animation_bridge.cpp).
export function humanSettings(base, character) {
  if (!character) return base;
  const merged = mergeCharacterSettings(base, character.settings || {});
  if (character.uber_rows?.length) {
    const grab = merged.original_grab_control, uber = grab.profile.uber.map((rows) => rows.slice());
    for (const [set, index, row] of character.uber_rows) uber[set][index] = row;
    merged.original_grab_control = { ...grab, profile: { ...grab.profile, uber } };
  }
  const id = character.identity || {};
  if (id.upper_mask8c0) merged.original_rider_identity = { upper_mask8c0: id.upper_mask8c0, upper_mask8c8: id.upper_mask8c8, upper_mask8d0: id.upper_mask8d0 };
  return merged;
}

// Rider pairs 0x107888: the human slot's weight (CHARDB +0x40 via 0x11FF98) and stats in the six-rider world
// (web/ai-racers.js world.reset reads doc.world.pair_inputs). The grid document holds Zoe's; keep it to restore.
export function applyHumanPairInputs(aiRace, character) {
  const inputs = aiRace?.doc?.world?.pair_inputs;
  if (!inputs?.[0]) return;
  aiRace.zoePairInputs ??= { ...inputs[0] };
  const pair = character?.identity?.pair;
  inputs[0] = pair ? { weight_attribute: pair.weight_attribute, collision_stat: pair.collision_stat, attack_stat: pair.attack_stat } : { ...aiRace.zoePairInputs };
}
