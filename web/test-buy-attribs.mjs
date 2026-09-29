// The lodge's Buy Attributes as the PS2 runs it (cFEStateBuyAttrib 0x1F4728, web/buy-attribs.js, pv buyAttribs;
// docs/career-events.md "Buy Attributes (cFEStateBuyAttrib)"). Every expectation below is a value read from the PS2's memory in
// the ARMSX2 runs local/ps2-capture/lodge/attrs/ba1..ba3 (screen state at 0x5A8100: raw +0x4C, pending +0x84, total +0xA0,
// bank +0xA4; profile Zoe 0x4AAAC8: cash +0xAC4, bytes +0xBDF; runtime bank 0x535554).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { BuyAttribSession, BuyAttribs, levelText, pointCost, runAttributes, flashAlpha, BAR_UNIT, LEVEL_MAX, FLASH_IN, FLASH_OUT, POPUP_VEIL } from './buy-attribs.js';
import { Career, originalAttributeBytes, ATTRIBUTE_ORIGINAL_INDEX } from './career.js';
import { buyPopupModel, buyPopupEvents, BUY_POPUP_NAMES } from './buy-popup.js';

const root = new URL('public/assets/', import.meta.url);
const careerFile = new URL('CAREER/career.json', root);
// UI_JSON=path: a scratch export of UI/character-select.json (tools/export_character_select.py with UI_OUT) before it is copied in
const UI_JSON = process.env.UI_JSON ? new URL('file://' + process.env.UI_JSON) : new URL('UI/character-select.json', root);
const data = fs.existsSync(careerFile) ? JSON.parse(fs.readFileSync(careerFile, 'utf8')) : null;
const COSTS = [250, 500, 750, 1000, 1250, 1500, 1750, 2000, 2500, 5000];   // 0x440550 (+4 of each 8-byte row)

// The table against the ELF and the export.
{
  const elf = new URL('../local/disc/SLUS_207.72', import.meta.url);
  if (fs.existsSync(elf)) { const b = fs.readFileSync(elf), at = 0x440550 - 0xFF000;
    assert.deepEqual(COSTS.map((_, k) => b.readInt32LE(at + 8 * k + 4)), COSTS, '0x440550 point prices'); }
  if (data) assert.deepEqual(data.rules.attribute_cost, COSTS, 'CAREER/career.json attribute_cost');
  assert.equal(pointCost(COSTS, 1), 250); assert.equal(pointCost(COSTS, 10), 5000); assert.equal(pointCost(COSTS, 11), null);
  assert.deepEqual(ATTRIBUTE_ORIGINAL_INDEX, [1, 3, 0, 4, 6, 5, 2], '0x4780B0 row -> byte');
}

// Level text (updateLevels 0x1F4EA8, "%d.%d" of raw/5 and (raw%5)*2) and bar widths (0x1F50B0, gp-0x554C = 37/11).
{
  assert.equal(levelText(5), '1.0'); assert.equal(levelText(7), '1.4'); assert.equal(levelText(9), '1.8'); assert.equal(levelText(54), '10.8'); assert.equal(levelText(55), '11.0');
  assert.equal(BAR_UNIT, Math.fround(3.3636362552642822)); assert.equal(Math.fround(55 * BAR_UNIT), Math.fround(185));
}

// ba1 + ba2 (lodge-rich: Zoe $20,000; rows Acceleration 5, Edging 5, Speed 5, Spin 54, Stability 9, Toughness 55, Tricks 5).
{
  const s = new BuyAttribSession([5, 5, 5, 54, 9, 55, 5], 20000, COSTS);
  assert.deepEqual(s.level, [1, 1, 1, 10, 1, 11, 1]);
  const six = [0, 1, 2, 3, 4, 5].map(() => s.right(0));
  assert.deepEqual(six, ['add', 'add', 'add', 'add', 'add', 'none'], 'pending stops at the next whole level, silently');
  assert.deepEqual([s.pending[0], s.total], [5, 1250], 'five points of 1.x at $250');
  assert.equal(s.left(0), 'remove'); assert.deepEqual([s.pending[0], s.total], [4, 1000]);
  assert.equal(s.right(0), 'add');
  assert.deepEqual([s.right(3), s.right(3)], ['add', 'none'], 'Spin 10.8: one point to 11.0 at $5,000');
  assert.equal(s.total, 6250);
  assert.deepEqual([s.right(4), s.right(4)], ['add', 'none'], 'Stability 1.8: one point to 2.0');
  assert.equal(s.total, 6500);
  assert.equal(s.right(5), 'error', 'Toughness 11.0');
  for (let k = 0; k < 7; k++) s.right(2);                                  // the held Right on Speed (repeats; capped at 5)
  assert.deepEqual(s.pending, [5, 0, 5, 1, 1, 0, 0]); assert.equal(s.total, 7750); assert.equal(s.bank, 20000, "'You have' keeps the cash");
  assert.deepEqual(s.view(0), { level: '1.0', cost: '$ 250', vector: 5 * BAR_UNIT, under: 10 * BAR_UNIT }, 'pending shows as the under bar only');
  assert.deepEqual([s.view(3).level, s.view(3).cost, s.view(5).level, s.view(5).cost, s.view(4).level], ['10.8', '$ 5,000', '11.0', null, '1.8']);
  assert.equal(s.cross(), 'popup');
  // Yes: 0x150C20 per point on the profile record (the port's career), then the screen's copy.
  const career = data ? new Career(data, { storage: null }) : null;
  const bought = [];
  if (career) { const r = career.rider('zoe'); r.cash = 20000; r.attributes = [5, 5, 5, 54, 9, 55, 5]; }
  s.confirm((row) => bought.push(career ? career.buyAttributePoint('zoe', row) : true));
  assert.equal(bought.length, 12); assert.ok(bought.every(Boolean));
  assert.deepEqual([s.bank, s.total], [12250, 0]); assert.deepEqual(s.raw, [10, 5, 10, 55, 10, 55, 5]); assert.deepEqual(s.level, [2, 1, 2, 11, 2, 11, 1]);
  assert.deepEqual(s.pending, [0, 0, 0, 0, 0, 0, 0]);
  if (career) { const r = career.rider('zoe'); assert.equal(r.cash, 12250); assert.deepEqual(r.attributes, [10, 5, 10, 55, 10, 55, 5]);
    assert.deepEqual(originalAttributeBytes(r.attributes), [10, 10, 5, 5, 55, 55, 10], 'the PS2 profile bytes and runtime bank after Yes'); }
  assert.equal(s.view(0).cost, '$ 500', 'level 2: $500 a point');
  // ba2: five points of Speed 2.x, the sixth ignored; six Lefts (the last one an error); Cross with nothing pending
  const r5 = [0, 1, 2, 3, 4, 5].map(() => s.right(2));
  assert.deepEqual(r5, ['add', 'add', 'add', 'add', 'add', 'none']); assert.equal(s.total, 2500);
  const l6 = [0, 1, 2, 3, 4, 5].map(() => s.left(2));
  assert.deepEqual(l6, ['remove', 'remove', 'remove', 'remove', 'remove', 'error']); assert.equal(s.total, 0);
  assert.equal(s.cross(), 'error');
}

// ba3 (lodge-clean: Zoe $507, every row 5).
{
  const s = new BuyAttribSession([5, 5, 5, 5, 5, 5, 5], 507, COSTS);
  assert.deepEqual([s.right(0), s.right(0), s.right(0)], ['add', 'add', 'error'], '$750 > $507');
  assert.equal(s.right(1), 'error'); assert.equal(s.total, 500);
  assert.equal(s.cross(), 'popup');
  s.confirm(() => true);
  assert.deepEqual([s.bank, s.raw[0], s.view(0).level], [7, 7, '1.4']);
  assert.equal(s.right(0), 'error', '$250 > $7');
  const t = new BuyAttribSession([5, 5, 5, 5, 5, 5, 5], 100, COSTS); assert.equal(t.cross(), 'error');
}

// 0x150C20 on the career record: the byte's own level prices the point; byte 55 and short cash fail.
if (data) {
  const c = new Career(data, { storage: null }), r = c.rider('mac');
  r.cash = 5000; r.attributes = [9, 50, 55, 5, 5, 5, 5];
  assert.equal(c.attributePointCost(9), 250); assert.equal(c.attributePointCost(10), 500); assert.equal(c.attributePointCost(50), 5000); assert.equal(c.attributePointCost(55), null);
  assert.equal(c.buyAttributePoint('mac', 0), true); assert.deepEqual([r.cash, r.attributes[0]], [4750, 10]);
  assert.equal(c.buyAttributePoint('mac', 2), false, 'byte 55');
  assert.equal(c.buyAttributePoint('mac', 1), false, '$5,000 > $4,750');
  assert.equal(c.attributeCost(7), 250, 'the old path reads a 0.2-step byte by its whole level');
  assert.equal(LEVEL_MAX, 11);
}

// The run's bytes (main.js startRun): the career record in Conquer the Mountain, the defaults online, else unchanged.
{
  const ui = { careerMode: true, careerUI: { career: { save: { riders: { zoe: { attributes: [10, 5, 10, 55, 10, 55, 5] } } } } } };
  assert.deepEqual(runAttributes(ui, 'zoe'), [10, 10, 5, 5, 55, 55, 10]);
  assert.equal(runAttributes(ui, 'mac'), null, 'no record yet: unchanged');
  assert.deepEqual(runAttributes({ onlineMode: true }, 'zoe'), [5, 5, 5, 5, 5, 5, 5]);
  assert.equal(runAttributes({ careerMode: false }, 'zoe'), null);
}

// The screen's runtime fields in FE.LUI 33buyattribs (names hashed like 0x317670).
{
  const f = UI_JSON;
  if (fs.existsSync(f)) {
    const screen = JSON.parse(fs.readFileSync(f, 'utf8')).screens?.['33buyattribs'];
    if (screen) {
      const hash = (n) => { let h = 0; for (const ch of Buffer.from(n, 'ascii')) { h = ((h << 4) + ch) >>> 0; const g = h & 0xf0000000; if (g) h = (h ^ (g >>> 23) ^ g) >>> 0; } return h.toString(16).padStart(8, '0'); };
      const names = new Set(screen.elements.map((e) => e.name));
      for (const n of ['AttribMenu', 'AttributeCost', 'CurrentMoney', ...[0, 1, 2, 3, 4, 5, 6].flatMap((k) => [`lvl${k}`, `cost${k}`, `Vector${k}`, `under${k}`])]) assert.ok(names.has(hash(n)), `33buyattribs has ${n}`);
      assert.deepEqual(screen.labels.filter((l) => ['hl left', 'hl right'].map(hash).includes(l.name)).map((l) => l.frame), [90, 95]);
    }
  }
}
// TransitionOut between the lodge and this screen (PS2 lodge/attrs/ba5: Triangle at 22, white at 32, the lodge's intro under it at 34):
// transition_flash's A track 0 -> 255 in 10 frames, -> 0 in 9; the new screen appears at full white. No input meanwhile.
{
  assert.deepEqual([FLASH_IN, FLASH_OUT], [10, 9]);
  assert.deepEqual([0, 5, 10, 14.5, 19, 30].map(flashAlpha), [0, 0.5, 1, 0.5, 0, 0]);
  const f = UI_JSON;
  if (fs.existsSync(f)) {
    const d = JSON.parse(fs.readFileSync(f, 'utf8')).screens;
    const flash = d.transition_flash?.animations?.['02ed6393']?.tracks?.['13'];
    if (flash) assert.deepEqual(flash, [[0, FLASH_IN], [255, FLASH_OUT], [0, 0]], 'transition_flash alpha');
    const veil = d.popup?.animations?.['00438b41']?.tracks;   // FE.LUI 'popup' 0885e124 = 139buy_popup 0885e124 (anim 0f5016c0, same tracks)
    if (veil) { assert.deepEqual([veil['45'], veil['46'], veil['47']].map((t) => t[0][0]), POPUP_VEIL.top); assert.deepEqual([veil['27'], veil['28'], veil['29']].map((t) => t[0][0]), POPUP_VEIL.bottom);
      assert.equal(veil['22'][0][0], POPUP_VEIL.height); assert.deepEqual([veil['13'][1][0], veil['26'][1][0], veil['13'][0][1]], [175, 175, POPUP_VEIL.frames]); }
  }
  // the GS alpha 175 >> 1 against the fits over the PS2 frames (ba1 buy popup 0.681, lodge l2 Ubertrick popup 0.678)
  assert.ok(Math.abs(POPUP_VEIL.alpha - 0.681) < 0.004 && Math.abs(POPUP_VEIL.alpha - 0.678) < 0.004);
  let clock = 0, switched = 0; const shown = [];
  const ui = { screen: 'ctm-attributes', index: 2, set(s) { this.screen = s; }, sync() {}, draw() {} };
  const b = new BuyAttribs({ ui }); b.now = () => clock; b.session = new BuyAttribSession([5, 5, 5, 5, 5, 5, 5], 100, COSTS);
  const c = { save() {}, restore() {}, fillRect() { shown.push(+/,([\d.]+)\)$/.exec(this.fillStyle)[1]); }, fillStyle: '' };
  b.leave(); assert.ok(b.flash);
  const blocked = { code: 'ArrowRight', preventDefault() {} }; assert.equal(b.key(blocked), true); assert.deepEqual(b.session.pending, [0, 0, 0, 0, 0, 0, 0], 'no input during the flash');
  for (clock = 0; clock <= 20; clock++) { const before = ui.screen; b.drawFlash(c); if (before !== ui.screen) switched = clock; }
  assert.equal(switched, 10, 'the lodge at full white'); assert.equal(ui.screen, 'ctm-lodge'); assert.equal(ui.index, 3); assert.equal(b.session, null); assert.equal(b.flash, null);
  assert.equal(shown.length, 19); assert.equal(Number(shown[10]), 1);
}
// The buy popups' own screen FE.LUI 139buy_popup (cUIStateBuyPopup 0x1CAC30; web/buy-popup.js): the widget names the code fills
// (onWidgetCreate 0x1CAC98), Yes / No focus frames 35 / 40 after the stop at 30, the intro's box and veil from frame 1, the texts at 25.
// PS2 lodge/attrs/ba6 (Cross at sample 11): the veil at 0.26 / 0.44 / 0.62 / full at 15 / 17 / 19 / 21 = the popup's frame 0 at 12.
{
  const f = UI_JSON;
  const screen = fs.existsSync(f) ? JSON.parse(fs.readFileSync(f, 'utf8')).screens?.['139buy_popup'] : null;
  if (screen) {
    const hash = (n) => { let h = 0; for (const ch of Buffer.from(n, 'ascii')) { h = ((h << 4) + ch) >>> 0; const g = h & 0xf0000000; if (g) h = (h ^ (g >>> 23) ^ g) >>> 0; } return h.toString(16).padStart(8, '0'); };
    const N = BUY_POPUP_NAMES;
    for (const [k, n] of Object.entries({ type: 'item type', name: 'item name', question: 'question', cost: 'cost_answer', have: 'you have_answer', credits: 'credits remain', groupCredit: 'group credit', groupCash: 'group cash', yes: 'yes', no: 'no' })) assert.equal(N[k], hash(n), n);
    const names = new Set(screen.elements.map((e) => e.name)); for (const n of Object.values(N)) assert.ok(names.has(n), n);
    const model = buyPopupModel(screen); assert.deepEqual(model, { frames: [35, 40], intro: 30 });
    const at = (frame, index) => buyPopupEvents(screen, model, frame, index).map(({ ev }) => ev.frame);
    assert.deepEqual([...new Set(at(10, 0))], [1], 'the box grows from frame 1'); assert.ok(at(26, 0).includes(25), 'texts from 25');
    assert.ok(!at(29, 0).includes(35) && at(30, 0).includes(35) && at(30, 1).includes(40) && !at(30, 1).includes(35), 'focus after the stop');
    const veil = screen.elements.find((e) => e.name === N.veil); assert.equal(veil.anim?.hash, '0f5016c0');
    assert.deepEqual(screen.animations['0f5016c0'].tracks['13'], [[0, POPUP_VEIL.frames], [175, 0]]);
  }
}
console.log('buy attributes: ok');
