// The human rider's profile-dependent identity: the gear it wears and the ubers it performs (docs/career-events.md "Lodge
// shops, awards and attributes"; careerRider).
//
// The original keeps no copy of either. Every world load assembles the race rider from the profile record's committed gear
// rows (0x14D068 from 0x22ED5C, then 0x11BBE8 / 0x11C138), and in the air the uber of grab slot c is read live from the
// runtime bank 0x530EC0 + char*0x1FE + c*6 + (tier >= 5) (0x1352A8 -> 0x150198 -> 0x14FEA8), which the lodge's and the
// front end's Ubertrick Setup write (0x14FD80). PS2 local/ps2-capture/lodge/runs: l2 buys and selects Madonna for Nose Grab
// (0x5316D1 = 2, cash - $10,000); l8 / l10 equip a bought board and ride out of the station on it.
//
// The browser's rider entry (main.js selectRider) carries both: the outfit package of the current mode's record
// (web/wardrobe.js outfitRider: the career record in Conquer the Mountain, the free-play store otherwise) and the uber rows
// of the record's selection (uberRows below, laid over the grab profile by web/character-roster.js). riderStamp is what that
// entry was resolved from (the mode, the gear record, the uber selection); main.js resolves the entry again when the stamp
// changed, before an event or world load finishes and at every run start, so a change made in the lodge, in the front end
// or by entering / leaving the career reaches the next run, as a world load does on the PS2.
import { uberChoiceRows } from './lodge.js';
import { outfitStamp } from './wardrobe.js';

// A cheat skin performs its base rider's ubers (0x14A080: setup slot +0x11) and keeps its own overrides (character-roster.js).
const recordId = (rider) => (rider?.kind === 'cheat' ? rider.base || 'zoe' : rider?.id);
const careerOf = (ui) => ui?.careerUI?.career || null;
const points = (ui) => ui?.characterSelect?.data?.uber_points?.table || null;   // trick score table 0x530600 (UI/character-select.json)

// {category: selected entry} of the rider's profile record, or null: no career tables, or a record that has not been created
// yet (its selection is the defaults, which the rider package's settings already hold).
export function uberSelection(ui, rider) {
  const c = careerOf(ui), id = recordId(rider), u = id && c?.shop ? c.save?.riders?.[id]?.uber : null;
  return u ? Object.fromEntries(Object.entries(u).map(([cat, st]) => [cat, st.selected])) : null;
}
// Grab-profile rows [1, slot, row] of the selection (web/lodge.js uberChoiceRows); none without the score table, and none online
// (the other clients simulate the package's own rows: the lobby profile carries no uber selection).
export function uberRows(ui, rider) {
  const c = careerOf(ui), sel = uberSelection(ui, rider), table = points(ui);
  if (!sel || !table || ui?.onlineMode) return [];
  return uberChoiceRows(c.shop, table, c.save.riders[recordId(rider)].character, sel);
}
const mode = (ui) => (ui?.onlineMode ? 'online' : ui?.careerMode ? 'career' : 'free');
export function riderStamp(ui, rider) {
  if (!rider) return '';
  return [mode(ui), outfitStamp(ui, rider), JSON.stringify(uberSelection(ui, rider)), !!points(ui)].join('|');
}
// The entry main.js loads: `resolved` (the outfit's package, web/wardrobe.js outfitRider) with the uber rows and what it was
// resolved from. `uber` keys the rows (main.js sameRider: a new selection re-initialises the rider, the model stays).
export function withProfile(ui, entry, resolved, stamp = riderStamp(ui, entry)) {
  const rows = uberRows(ui, entry);
  return { ...resolved, entry, stamp, uber_choice: rows, uber: rows.length ? JSON.stringify(rows.map(([, slot, r]) => [slot, r.semantic, r.upper_semantic, r.score_id])) : '' };
}
