// "UBER TRICK = @l1/@r1 + @square" hint (web/trick-hud.js uberHint; 0x1EBCA4 pre-pass gating, 0x1E92A8 layout, 0x1E95A0 draw at
// descriptor 73, icons from table 0x4C8980 on OV_1-2). PS2 frames: uber-chain 608/665/1557 (docs/tricks-scoring.md).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { TrickHud } from './trick-hud.js';
const ui = new URL('public/assets/UI/', import.meta.url);
const data = JSON.parse(fs.readFileSync(new URL('trick-hud.json', ui)));
const glyphs = { FEFONT: JSON.parse(fs.readFileSync(new URL('FEFONT-glyphs.json', ui))), HUDFONT: JSON.parse(fs.readFileSync(new URL('HUDFONT-glyphs.json', ui))) };
const hud = new TrickHud(data, glyphs);
const slots = () => Array(44).fill(null);
const tricky = (ratio) => ({ type: 9, maximum: -1, value: -ratio, arg: 1, field10: 0, points: 0 });
const hint = (list) => list.filter((d) => d.hint);
hud.resetUberHint(1);
let s = slots(); s[9] = tricky(0.5);
let draw = hint(hud.frame(s, { prepassSlots: s }));
assert.equal(draw.length, 4, 'two text runs and two icons');
assert.equal(draw[0].text, 'UBER TRICK = '); assert.equal(draw[2].text, ' + ');
assert.deepEqual(draw[1].sprite.uv, TrickHud.HINT_ICONS.r1.uv); assert.deepEqual(draw[3].sprite.uv, TrickHud.HINT_ICONS.square.uv);
const right = draw[3].x + 21; assert.ok(Math.abs((draw[0].x + right) / 2 - 320) < 2, 'centred on x = 320');
assert.ok(Math.abs(draw[3].y + 20 - 460) < 0.5 && Math.abs(draw[1].y - (draw[3].y + 2)) < 0.01, 'bottom at y = 460, icons centred on the text height');
// a live trick name (type 0) in the pre-pass bank hides it; the draw bank alone does not (the pre-pass is a tick behind)
const named = slots(); named[9] = tricky(0.5); named[0] = { type: 0, maximum: 3, value: 0, arg: 0, field10: 0, points: 0, text: 'FS 360' };
assert.equal(hint(hud.frame(named, { prepassSlots: named })).length, 0); assert.equal(hint(hud.frame(named, { prepassSlots: s })).length, 4);
// no Tricky slot, a full slot-9 ratio (it flips owner+0x55C: next time @l1), after the finish
assert.equal(hint(hud.frame(slots(), { prepassSlots: slots() })).length, 0);
const full = slots(); full[9] = tricky(1); assert.equal(hint(hud.frame(full, { prepassSlots: full })).length, 0);
draw = hint(hud.frame(s, { prepassSlots: s })); assert.deepEqual(draw[1].sprite.uv, TrickHud.HINT_ICONS.l1.uv);
assert.equal(hint(hud.frame(s, { prepassSlots: s, finished: true })).length, 0);
assert.equal(hint(hud.frame(s, { prepassSlots: s, flags: data.flags & ~0x1000000 })).length, 0, 'bit 24 (button prompts) required');
console.log('uber hint: layout, gating (slot 9, type 0/0x21/0xB, flags 21/24, finish) and the L1/R1 toggle checked');
