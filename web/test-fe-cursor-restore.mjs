// pv feCursorRestore (web/fe-event-select.js restoring): a Select Event map screen reads no input until its first drawn frame has
// restored its cursor (focus). The PS2 runs phase 3 (0x39ECB0: vtable +0x34, show / cursor restore) before phase 5, the only phase
// that reads input (vtable +0x64; docs/pause-contexts.md 2). Off: a Cross in the frame between the screen change and that draw
// chooses the previous screen's row (test-online-records: Leaderboards row 5 -> Select Peak chose Peak 3, the cursor on Peak 1).
//   node test-fe-cursor-restore.mjs
import assert from 'node:assert/strict';
import { setPv } from './pv-flags.js';

globalThis.performance ??= { now: () => 0 };
const { FeEventSelect } = await import('./fe-event-select.js');

// a page UI with just what the selector reads: the screen, the cursor, ready, set() entering the screen as web/ui.js set does
function selector() {
  const ui = { screen: 'main', index: 5, ready: true, courses: [], previousScreen: null };
  const es = new FeEventSelect(ui);
  // ready / owns: the Map screen loaded
  es.map = {};
  es.peak = 1;
  es.boards = true;
  ui.eventSelect = es;
  ui.set = (screen) => {
    ui.previousScreen = ui.screen;
    ui.screen = screen;
    es.enter(screen, ui.previousScreen);
  };
  ui.sync = () => {};
  return { ui, es };
}
const cross = { code: 'Enter', repeat: false, preventDefault() {} };

for (const on of [false, true]) {
  setPv('feCursorRestore', on);
  // Main > Leaderboards: Select Peak opens with its cursor to restore on Peak 1 (row 2); ui.index is still the main menu's 5
  const { ui, es } = selector();
  es.openBoards();
  assert.equal(ui.screen, 'fe-peak');
  assert.equal(es.pendingIndex, 2, 'the entry row: Peak 1');
  // a Cross before the first drawn frame
  assert.equal(es.key(cross), true, 'the selector owns the key');
  if (on) {
    assert.equal(es.flash, null, 'on: no input before the cursor restore');
    assert.equal(es.peak, 1);
    // the first drawn frame's cursor restore (draw -> focus), then the Cross takes the restored row
    es.focus();
    assert.equal(ui.index, 2);
    es.key(cross);
    assert.ok(es.flash, 'on: the Cross after the restore takes');
    assert.equal(es.peak, 1, 'on: Peak 1, the focused row');
  } else {
    // off: row 5 is no peak (3 - 5), so nothing happens; from a screen whose row maps to a peak it picks that row (below)
    assert.equal(es.flash, null);
    ui.index = 0;
    es.enter('fe-peak', 'main');
    es.key(cross);
    assert.ok(es.flash, 'off: the Cross takes before the restore');
    assert.equal(es.peak, 3, 'off: the previous screen\'s row 0 chose Peak 3 with the cursor on Peak 1');
  }
}
setPv('feCursorRestore', null);
console.log('fe cursor restore: OK');
