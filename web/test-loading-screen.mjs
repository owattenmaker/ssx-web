// Loading screen (web/loading-screen.js): hint rotation 0x245950, LUI track playback, the load percentage, the
// keyboard panel rows, and the exported GL.LUI 110ctrl_load layout (tools/export_loading_screen.py).
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { nextHint, trackValue, animationProps, loadingPercent, keyboardRows, HINT_COUNT, HINT_SKIP } from './loading-screen.js';

// Hint order from a fresh counter: 1..11, 13, 14, 15, then back to 1 (12 is never shown).
let counter = 0; const shown = [];
for (let i = 0; i < 2 * HINT_COUNT; i++) { const r = nextHint(counter); shown.push(r.hint); counter = r.counter; }
assert.deepEqual(shown.slice(0, 14), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15]);
assert.equal(shown[14], 1);
assert.ok(!shown.includes(HINT_SKIP));
assert.equal(nextHint(-3).hint, 1); assert.equal(nextHint(NaN).hint, 1);

// Tracks: linear between keys over the frame counts, held after the last key.
const keys = [[153, 299], [-152, 0]];
assert.equal(trackValue(keys, 0), 153);
assert.equal(trackValue(keys, 299), -152);
assert.equal(trackValue(keys, 1000), -152);
assert.ok(Math.abs(trackValue(keys, 149.5) - 0.5) < 1e-9);
assert.equal(trackValue([[0, 10], [10, 10], [0, 0]], 15), 5);
const anim = { frames: 300, tracks: { 0: [[0, 299], [-304, 0]] } };
assert.equal(animationProps(anim, 300, 9)[0], 0);            // mode 8 bit: loops over the 300 frames
assert.equal(animationProps(anim, 1000, 1)[0], -304);        // mode 1: holds the end

// Percentage: rises to at most 98 while loading, 100 only when done, monotonic.
const curve = [[0, 0], [20, 2], [120, 17], [240, 27], [400, 52], [520, 97], [540, 98], [800, 98], [810, 100]];
let prev = 0;
for (let f = 0; f <= 600; f += 5) { const p = loadingPercent(curve, f, 420, 815, false); assert.ok(p >= prev && p <= 98); prev = p; }
assert.equal(loadingPercent(curve, 420, 420, 815, false), 98);
assert.equal(loadingPercent(curve, 30, 420, 815, true), 100);

// Keyboard rows follow pad-input.js: Simple = arrows/WASD turn + spin/flip; Classic adds IJKL.
const s = { turn: 'Turn', turn_spin_flip: 'Turn/spin/flip', jump: 'Jump', boost_tweak: 'Boost/Tweak', grab_board: 'Grab board', hand_plant: 'Hand plant', board_press: 'Board press', reset: 'Reset', pause: 'Pause' };
const simple = keyboardRows('Simple', s), classic = keyboardRows('Classic', s);
assert.equal(simple[0].label, 'Turn/spin/flip'); assert.equal(classic[0].label, 'Turn');
assert.ok(classic.some((r) => r.keys.join('') === 'IJKL')); assert.ok(!simple.some((r) => r.keys.join('') === 'IJKL'));
for (const [keysLabel, label] of [['Space', 'Jump'], ['Shift', 'Boost/Tweak'], ['QZEX', 'Grab board'], ['C', 'Hand plant'], ['TFGH', 'Board press'], ['Backspace', 'Reset'], ['Esc', 'Pause']])
  assert.ok(simple.some((r) => r.keys.join('') === keysLabel && r.label === label), keysLabel);

// Exported layout: the original screen's strings, sprites and timeline.
const data = JSON.parse(readFileSync(new URL('./public/assets/LOADING/loading.json', import.meta.url)));
const byName = new Map(data.elements.map((e) => [e.name, e]));
const text = (name) => byName.get(name)?.text;
assert.equal(text('07db5562'), 'Basic Controls');
assert.equal(text('0e303d74'), 'Default');
assert.equal(text('0efd5af4'), 'Loading...');
assert.equal(byName.get('0a6b545c').sprite.page, 'GL_1-4');           // DualShock 2 picture
assert.deepEqual([byName.get('0a6b545c').props[0], byName.get('0a6b545c').props[1]], [188, 125]);
for (const e of data.elements) if (e.parent) assert.ok(byName.has(e.parent), e.name);
for (const ev of data.events) if (ev.anim) assert.ok(data.animations[ev.anim], ev.anim);
assert.equal(data.hints.length, 15);
assert.equal(data.hints[0].title, 'Aggression and Tolerance levels');
console.log('loading screen: hint rotation, LUI tracks, percentage, keyboard rows and 110ctrl_load layout OK');
