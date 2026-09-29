// Sam's Equip Gear in parts mode (tools/sam_wardrobe.py; docs/characters.md "Sam's Equip Gear"): every listed item of
// the Sam build's bucket gives a Sam model/texture assembly through the riders' own path (0x11BBE8 assembly,
// buildPackage race + FE), the default outfit equals RIDER_SAM, Sam's own outfits are owned from a fresh profile,
// his hair comes along under every hat, and the online key resolves to the same package.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {loadWardrobe, Wardrobe, assembly, buildPackage, outfitKey, remoteOutfitRider} from './wardrobe.js';
const pub = new URL('public', import.meta.url).pathname;
const fetcher = async (path) => { const file = pub + path; if (!fs.existsSync(file)) return {ok: false, status: 404}; const data = fs.readFileSync(file);
  return {ok: true, json: async () => JSON.parse(data), arrayBuffer: async () => data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength), text: async () => data.toString()}; };
const w = await loadWardrobe('sam', fetcher);
assert(w.parts_mode, 'WARDROBE/SAM is in parts mode (tools/sam_wardrobe.py)');
const fresh = new Wardrobe(w);
assert.deepEqual(fresh.menu(22).map((e) => e.name), ['Midwest Unc', 'Sunday Unc', 'Earn Your Turns', 'Uphill Club', 'Lodge Legend']);
assert.deepEqual(fresh.menu(24).map((e) => e.item), [180, 181]);
// the default assembly is RIDER_SAM's mesh
const base = JSON.parse(fs.readFileSync(pub + '/assets/RIDER_SAM/rider.json'));
const def = buildPackage(w, assembly(w, fresh.raceEquipped()), {riderId: 'sam'});
assert.equal(def.world.vertex_count, fs.statSync(pub + '/assets/RIDER_SAM/vertices.bin').size / 40, 'default outfit = RIDER_SAM');
assert.deepEqual(def.rig.bones.slice(0, 24).map((b) => b.name), base.bones.slice(0, 24).map((b) => b.name));
// every listed item builds race and FE packages with every material textured
const leaves = w.entries.filter((e) => (e[4] & 4) && e[7] && !(e[4] & 0x20)).map((e) => e[0]);
let built = 0;
for (const item of leaves) {
  const wd = new Wardrobe(w); for (const [k, v] of wd.inv.f) wd.inv.f.set(k, v | 2);
  if (!wd.set(item, true)) continue; wd.commit();
  for (const fe of [false, true]) { buildPackage(w, assembly(w, fe ? wd.feEquipped() : wd.raceEquipped()), {riderId: 'sam', fe}); built++; }
}
assert(built >= 2 * 280, `${built} packages`);
// hats keep Sam's hair; a hair style replaces it; Skint shows none
const parts = (wd) => assembly(w, wd.raceEquipped()).parts.map((p) => p.resource);
for (const [item, hair] of [[151, true], [352, true], [239, true], [146, false], [142, true], [158, false], [97, false]]) {
  const wd = new Wardrobe(w); for (const [k, v] of wd.inv.f) wd.inv.f.set(k, v | 2); wd.set(item, true); wd.commit();
  assert.equal(parts(wd).includes('sam_hair.mnf'), hair, `hair with ${item}`);
}
// online: the w1 key resolves to the same package root
const wd = new Wardrobe(w); wd.toggle(50); wd.toggle(180);
const ui = {careerUI: null, onlineMode: false};
globalThis.localStorage = {store: {}, getItem(k) { return this.store[k] ?? null; }, setItem(k, v) { this.store[k] = String(v); }, removeItem(k) { delete this.store[k]; }};
localStorage.setItem('ssx3.outfit.v1', JSON.stringify({sam: wd.serialize()}));
const realFetch = globalThis.fetch; globalThis.fetch = fetcher;
try {
  const key = await outfitKey(ui, {id: 'sam', package: 'RIDER_SAM', kind: 'custom'});
  assert(key && key.startsWith('w1:') && key.includes('50') && key.includes('180'), key);
  const remote = await remoteOutfitRider({id: 'sam', package: 'RIDER_SAM', kind: 'custom'}, key, {fetcher});
  assert(remote.root && remote.root.includes('/WARDROBE/SAM/o'), remote.root);
} finally { globalThis.fetch = realFetch; }
console.log('Sam Equip Gear (parts):', {items: leaves.length, packages: built, parts: Object.keys(w.parts).length, textures: Object.keys(w.textures).length});
