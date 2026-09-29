// Now-playing popup (web/now-playing.js): layout and timeline against the popup state stored in the user's
// PCSX2 snapshots (HUD hud+0x19C line records, +0x190/+0x194/+0x198, +0x3AC/+0x3B0).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { popupPhase, advanceTime, wrapLine, layoutNowPlaying, nowPlayingDrawList, NowPlayingHud, NOW_PLAYING } from './now-playing.js';
const ui = new URL('public/assets/UI/', import.meta.url);
const fonts = { FEFONT: JSON.parse(fs.readFileSync(new URL('FEFONT-glyphs.json', ui))), HUDFONT: JSON.parse(fs.readFileSync(new URL('HUDFONT-glyphs.json', ui))) };
const near = (a, b, tol = 2e-4, what = '') => assert.ok(Math.abs(a - b) <= tol, `${what}: ${a} vs ${b}`);

// Snapshot line records (text, width +0x1FC, height +0x200) and box size.
const cases = [
  { song: { title: 'Silver Screen Shower Scene', artist: 'Felix Da Housecat', album: 'Kittenz and Thee Glitz' }, // the-junction-*.p2s
    lines: [['EA RADIO BIG', 149.59996032714844, 20.99884033203125], ['Silver Screen Shower Scene', 259.8058776855469, 20.400266647338867],
      ['Felix Da Housecat', 169.80386352539062, 17.4003963470459], ['Kittenz and Thee Glitz', 201.00454711914062, 17.4003963470459]], W: 299.8058776855469, H: 104.20459747314453 },
  { song: { title: 'Avalanche', artist: 'Powerplant', album: '' }, // snow-jam-countdown-1.p2s
    lines: [['EA RADIO BIG', 149.59996032714844, 20.99884033203125], ['Avalanche', 101.4023208618164, 20.400266647338867],
      ['Powerplant', 106.80244445800781, 17.4003963470459], ['', 0, 17.4003963470459]], W: 189.59996032714844, H: 104.20459747314453 },
  { song: { title: 'Screw Up', artist: 'Overseer', album: 'Wreakage' }, // metro-city-countdown-anchor.p2s
    lines: [['EA RADIO BIG', 149.59996032714844, 20.99884033203125], ['Screw Up', 85.80196380615234, 20.400266647338867],
      ['Overseer', 85.80196380615234, 17.4003963470459], ['Wreakage', 94.80216979980469, 17.4003963470459]], W: 189.59996032714844, H: 104.20459747314453 },
];
for (const c of cases) {
  const l = layoutNowPlaying(fonts, c.song);
  assert.equal(l.lines.length, c.lines.length); assert.equal(l.lastTitle, 1);
  l.lines.forEach((x, i) => { assert.equal(x.text, c.lines[i][0]); near(x.w, c.lines[i][1], 2e-4, `${x.text} width`); near(x.h, c.lines[i][2], 1e-5, `${x.text} height`); });
  near(l.width, c.W, 2e-4, 'box width'); near(l.height, c.H, 1e-4, 'box height');
}

// Wrapping (0x392430): a long title splits at the last space that still fits 300 px, later lines skip spaces.
const fe = Math.fround(0.8 * NOW_PLAYING.feScale);
const long = 'A Very Long Song Title That Needs More Than One Line To Fit';
const w1 = wrapLine(fonts.FEFONT, long, fe);
assert.ok(w1.rest && long.startsWith(w1.line + ' ') && !w1.rest.startsWith(' '), JSON.stringify(w1));
const lw = layoutNowPlaying(fonts, { title: long, artist: 'X', album: 'Y' });
assert.ok(lw.lastTitle >= 2 && lw.lines.slice(1, lw.lastTitle + 1).every((x) => x.w < 300));
assert.equal(lw.lines.slice(1, lw.lastTitle + 1).map((x) => x.text).join(' '), long);
assert.equal(wrapLine(fonts.FEFONT, '', fe).rest, null);

// Timeline: phases and the snapshot states (time, slide, alpha).
near(popupPhase(0.3666665554046631).slide, 0.9166663885116577, 1e-6, 'slide in'); assert.equal(popupPhase(0.3666665554046631).alpha, 0);
near(popupPhase(7.299946308135986).alpha, 0.750133752822876, 1e-6, 'fade out'); near(popupPhase(7.283279895782471).alpha, 0.791799783706665, 1e-6, 'fade out');
assert.deepEqual(popupPhase(5.699970722198486), { slide: 1, alpha: 1, done: false });
near(popupPhase(0.6).alpha, 0.5, 1e-6, 'fade in'); near(popupPhase(7.8).slide, 0.5, 1e-5, 'slide out'); assert.equal(popupPhase(8).done, true);
// Frame count: accumulated 1/60 steps (float32) until done.
let t = 0, frames = 0, firstFull = -1; while (!popupPhase(t = advanceTime(t)).done) { frames++; if (firstFull < 0 && popupPhase(t).alpha === 1) firstFull = frames; }
assert.ok(frames >= 478 && frames <= 480, `visible frames ${frames}`); assert.ok(firstFull >= 47 && firstFull <= 49, `full alpha at ${firstFull}`);

// Draw list (0x2204A0): box at (20, 460 - H), slides from x = -30 - W, text at +20 / +14.0024.
const l0 = layoutNowPlaying(fonts, cases[0].song);
const full = nowPlayingDrawList(l0, 1, 1);
near(full.box.x, 20, 0, 'box x'); near(full.box.y, 460 - cases[0].H, 1e-4, 'box y');
assert.deepEqual(full.draws.slice(0, 3).map((d) => d.w), [42, Math.fround(cases[0].W - 56), 14]);
const texts = full.draws.filter((d) => d.kind === 'text');
near(texts[0].x, 40, 0, 'text x'); near(texts[0].y, full.box.y + 14.00235652923584, 1e-4, 'text y');
near(texts[1].y, texts[0].y + 20.99884033203125, 1e-4, 'title y'); assert.deepEqual(texts.map((d) => d.argb[1]), [0, 1, 0, 0]);
near(nowPlayingDrawList(l0, 0, 0).box.x, -30 - cases[0].W, 1e-3, 'slide start');

// State machine: command 5 semantics.
const hud = new NowPlayingHud({ fonts, images: {} });
hud.show(cases[1].song); assert.equal(hud.visible, false); hud.tick(); assert.equal(hud.visible, false); hud.tick(); assert.equal(hud.visible, true);
for (let k = 0; k < 100; k++) hud.tick(); const before = hud.time;
hud.show(cases[1].song); assert.equal(hud.time, before, 'same title keeps running');
hud.show(cases[2].song); assert.equal(hud.time, -1, 'new title restarts');
hud.hidden = 1; hud.tick(); hud.tick(); assert.equal(hud.visible, false); hud.hidden = 0; assert.equal(hud.visible, true);
for (let k = 0; k < 600; k++) hud.tick(); assert.equal(hud.active, false);
hud.show(cases[2].song); assert.equal(hud.time, -1, 'same song shows again after the popup ended');
hud.show({ title: '' }); assert.equal(hud.active, false);
hud.show(cases[0].song); hud.update(2 / 60 + 1e-6); assert.equal(hud.visible, true);
console.log(`now-playing popup ok (${cases.length} snapshot layouts, ${frames} visible frames)`);
