// Options > Save/Load > New game (pv newGameReset; docs/ctm-parity.md "New game"): the PS2's Yes (0x18D460 event 0x16 ->
// 0x18D4F4..0x18D5E4) resets profile 0 (0x149A88(0) -> 0x1567B8(0x4A6CA8, 1): the ten career blocks as at boot; the player name
// kT_MEMPlayerName 1, the rider Zoe without a cheat skin; the relationships +0xBC1, the outfit records, the owned cheat characters
// +0xF30..+0xF7F) and keeps the records (0x535C18, the options file written by 0x152758) and the options.
import assert from 'node:assert/strict';
import fs from 'node:fs';

class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } key(i) { return [...this.map.keys()][i] ?? null; } get length() { return this.map.size; } }
const local = new Memory(); globalThis.localStorage = local;
const { Career } = await import('./career.js');
const { FeSaveLoad } = await import('./fe-saveload.js');
const { loadSelection } = await import('./save-store.js');
const root = new URL('public/assets/', import.meta.url);
if (!fs.existsSync(new URL('CAREER/career.json', root))) { console.log('test-new-game: career tables not exported; skipped'); process.exit(0); }
const data = JSON.parse(fs.readFileSync(new URL('CAREER/career.json', root)));

function run() {
  for (const k of [...local.map.keys()]) local.removeItem(k);
  const career = new Career(data, { storage: local });
  const r = career.rider('zoe'); r.cash = 5000; career.save.records[0][0] = { ...career.save.records[0][0], name: 'OWEN', value: 1234 };
  career.persist();
  local.setItem('ssx3.relationships.v1', '[1]'); local.setItem('ssx3.outfit.free.v1', '{"zoe":3}'); local.setItem('ssx3.cheatCharacters', '["all"]');
  local.setItem('ssx3.playerName', 'OWEN'); local.setItem('ssx3.selection.v1', JSON.stringify({ rider: 'mac', peak: 2 })); local.setItem('ssx3.feOptions', '{"x":1}');
  const ui = { careerUI: { career }, riders: [{ id: 'mac' }, { id: 'zoe' }], riderIndex: 0, feScreens: { playerName: 'OWEN' }, playerName: 'OWEN' };
  const s = new FeSaveLoad(ui); s.newGame();
  return { career, ui };
}
{
  { const { career, ui } = run();
    assert.deepEqual(career.save.riders, {}, 'every rider\'s career block as at boot (0x151600)');
    assert.equal(career.save.records[0][0].name, 'OWEN', 'the records are kept (the options file, 0x535C18)');
    for (const k of ['ssx3.relationships.v1', 'ssx3.outfit.free.v1', 'ssx3.cheatCharacters']) assert.equal(local.getItem(k), null, `${k} reset`);
    assert.equal(local.getItem('ssx3.playerName'), 'PLAYER 1', 'the name: kT_MEMPlayerName 1 (0x149A88)'); assert.equal(ui.playerName, 'PLAYER 1'); assert.equal(ui.feScreens.playerName, 'PLAYER 1');
    assert.equal(loadSelection(local).rider, 'zoe', 'the rider: Zoe (+0x11 = 4)'); assert.equal(ui.riderIndex, 1);
    assert.equal(local.getItem('ssx3.feOptions'), '{"x":1}', 'the options are kept'); }
}
console.log('test-new-game: New game resets profile 0 and keeps the records and options (0x18D4F4)');
