// Setup Character, Rider Details, Options, Load game (web/fe-screens.js, docs/characters.md): layout data from
// tools/export_character_select.py, menu enable rules, the Ubertrick Setup rows and the screen flow, without a browser.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { FeScreens, defaultPlayerName, menuModel, stepMenu, firstEnabled, setupDisabled, detailsDisabled, musicDisabled, optionsDisabled, uberRow, uberChoiceRows,
  keyboardPress, KEY_ROWS, FE_LUI, PROFILE_PAGES, DEFAULT_PLAYER_NAME } from './fe-screens.js';
import { humanSettings, loadCharacter, setUberChoice } from './character-roster.js';
import { UBER_ROWS, uberEntries, initialUber } from './lodge.js';

const root = new URL('./public/assets/', import.meta.url).pathname;
const read = (p) => JSON.parse(fs.readFileSync(root + p));
const data = read('UI/character-select.json');
const shop = read('CAREER/shop.json');
const initial = read('ANIMATIONS/initial.json');
const riders = read('riders.json');
const riderClips = read('ANIMATIONS/library.json').clips;

// ---- layout data ----
for (const key of Object.values(FE_LUI).concat(['155rider_details_conquer', 'Fullkeyboard', '09set_char', '08sel_char', '131cheat_char']))
  assert.ok(data.screens[key]?.elements?.length, `screen ${key} exported`);
const texts = (key, menu) => menuModel(data.screens[key], menu).texts;
assert.deepEqual(texts('09set_char'), ['Continue', 'Equip Gear', 'Rider Details', 'Music']);
assert.deepEqual(texts('154rider_details'), ['Rewards', 'Cheat Characters', 'Ubertrick Setup', 'Player Name', 'Rider Profile']);
assert.deepEqual(texts('155rider_details_conquer'), ['Rewards', 'Trophies', 'Cheat Characters', 'Ubertrick Setup', 'Career Highlights', 'Player Name', 'Rider Profile']);
assert.deepEqual(texts('140audio'), ['Radio BIG', 'BIG Mountain Ambience', 'Custom Playlist [DJ]', 'Custom Playlist [No DJ]', 'Edit Playlist']);
assert.deepEqual(texts('18options'), ['Game Options', 'Sound Options', 'Controller Settings', 'HUD Options', 'Save/Load', 'Enter Cheat', 'Credits', 'DONE']);
assert.equal(menuModel(data.screens['66ut_btnmap'], '0c5a3d9e').items.length, UBER_ROWS.length + 1);   // six categories + DONE
assert.equal(menuModel(data.screens['66ut_btnmap'], '05ab281b').items.length, 9);                   // nine uber rows
assert.equal(menuModel(data.screens['93profile_load'], '07653c55').items.length, 6);
// focus states: one per item, after the intro (frames 35 + 5i on the menu screens)
assert.deepEqual(menuModel(data.screens['09set_char']).frames, [35, 40, 45, 50]);
assert.deepEqual(menuModel(data.screens['154rider_details']).frames, [35, 40, 45, 50, 55]);
assert.deepEqual(menuModel(data.screens['18options']).frames, [35, 40, 45, 50, 55, 60, 65, 70]);
// the focus state shows item i's own help text (alpha 255) and hides the others
{
  const sc = data.screens['09set_char'], help = sc.elements.filter((e) => e.parent === '03253174').map((e) => e.name);
  assert.equal(help.length, 4);
  [35, 40, 45, 50].forEach((frame, i) => help.forEach((name, j) => {
    const ev = sc.events.find((v) => v.frame === frame && v.element === name);
    assert.equal(ev.props[13], i === j ? 255 : 0, `setup help ${j} at state ${frame}`);
  }));
  const texts = help.map((n) => sc.elements.find((e) => e.name === n).text);
  assert.deepEqual(texts, ['Continue to peak selection.', "Customize the look of your character with gear you've bought in Conquer the Mountain mode.",
    'Configure your character, view stats and info.', 'Configure the music to be played in game.']);
  const dh = data.screens['154rider_details'].elements.filter((e) => e.parent === '03253174').map((e) => e.text);
  assert.deepEqual(dh, ['View rewards earned.', 'Choose a cheat character to play with.', 'Customize ubertricks to be used in game.', 'Enter a player name.', "View a rider's bio and backstory."]);
}
// Rider Profile texts: DNA 8, Faves 12, Q&A 4, bio for the ten riders and Sam
for (const r of riders.filter((x) => x.kind === 'rider' || x.kind === 'custom')) {
  const p = data.profiles[r.id]; assert.ok(p, `profile ${r.id}`);
  assert.equal(p.dna.length, 8); assert.equal(p.faves.length, 12); assert.equal(p.qna.length, 4); assert.ok(p.bio?.length > 50);
  assert.ok([...p.dna, ...p.faves, ...p.qna].every((t) => typeof t === 'string' && t.length), `profile texts ${r.id}`);
  if (r.dna) assert.deepEqual(p.dna, r.dna, `DNA ${r.id} = riders.json`);
}
assert.equal(data.profiles.zoe.qna[1], 'The sound barrier');
assert.equal(PROFILE_PAGES.length, 4);
for (const k of ['rider_dna', 'rider_faves', 'rider_qna', 'rider_bio', 'buy_trick_ctm', 'insert_card', 'enter_cheat']) assert.ok(data.strings[k], k);

// ---- enable rules ----
assert.deepEqual(setupDisabled({ wardrobe: true }), [false, false, false, false]);
assert.deepEqual(setupDisabled({ wardrobe: false }), [false, true, false, false]);
assert.deepEqual(detailsDisabled({ cheats: 0 }), [false, true, false, false, false]);     // fresh profile (PS2 frame)
assert.deepEqual(detailsDisabled({ cheats: 3 }), [false, false, false, false, false]);
assert.deepEqual(musicDisabled({ playlist: 0, edit: true }), [false, false, true, true, false]);
assert.equal(optionsDisabled().length, 8); assert.equal(optionsDisabled()[5], false); assert.equal(optionsDisabled()[7], false);
// wrap and skip (PS2: Setup Down from Music -> Continue; Rider Details Down from Rewards skips Cheat Characters, Up wraps)
assert.equal(stepMenu(3, 1, setupDisabled({ wardrobe: true })), 0);
assert.equal(stepMenu(0, -1, setupDisabled({ wardrobe: true })), 3);
const det = detailsDisabled({ cheats: 0 });
assert.equal(stepMenu(0, 1, det), 2); assert.equal(stepMenu(2, -1, det), 0); assert.equal(stepMenu(4, 1, det), 0); assert.equal(stepMenu(0, -1, det), 4);
assert.equal(firstEnabled([true, false]), 1);

// ---- Ubertrick Setup -> grab profile rows ----
const points = data.uber_points?.table;
assert.ok(points?.length >= 85, 'trick score table 0x530600');
const zoe = shop.uber_tricks.per_rider_defaults[4];
for (const [name, category] of UBER_ROWS) {
  const def = zoe.rows[name].owned_default.entry, entry = uberEntries(shop, category)[def];
  assert.deepEqual(uberRow(entry, points), initial.original_grab_control.profile.uber[1][category], `Zoe's ${name} uber = initial.json set 1`);
  const base = uberEntries(shop, category)[zoe.rows[name].hidden_base.entry];
  assert.deepEqual(uberRow(base, points), initial.original_grab_control.profile.uber[0][category], `Zoe's hidden ${name} uber = set 0`);
}
const sel = Object.fromEntries(Object.entries(initialUber(shop, 4)).map(([c, st]) => [c, st.selected]));
assert.deepEqual(uberChoiceRows(shop, points, 4, sel), [], 'defaults add no rows');
const chosen = uberChoiceRows(shop, points, 4, { ...sel, 0: 2 });                               // Method: jib O
assert.deepEqual(chosen, [[1, 0, { semantic: 120, upper_semantic: 170, score_id: 49, begin_points: 2000, hold_points: 3000 }]]);
setUberChoice('zoe', chosen);
const zoeDoc = await loadCharacter({ id: 'zoe', package: 'RIDER_ZOE', settings: false });
assert.equal(humanSettings(initial, zoeDoc).original_grab_control.profile.uber[1][0].semantic, 120);
assert.equal(humanSettings(initial, zoeDoc).original_grab_control.profile.uber[1][1].semantic, initial.original_grab_control.profile.uber[1][1].semantic);
setUberChoice('zoe', []);
assert.equal(await loadCharacter({ id: 'zoe', package: 'RIDER_ZOE', settings: false }), null);

// ---- keyboard ----
let k = { text: 'PLAYER1', caret: 7, caps: false, shift: false, max: 15 };
k = keyboardPress(k, 'Back'); assert.equal(k.text, 'PLAYER');
k = keyboardPress(k, 'z'); assert.equal(k.text, 'PLAYERz');
k = keyboardPress(k, 'Caps'); k = keyboardPress(k, 'q'); assert.equal(k.text, 'PLAYERzQ');
k = keyboardPress(k, 'Shift'); k = keyboardPress(k, 'q'); assert.equal(k.text, 'PLAYERzQq');       // Shift inverts Caps, once
k = keyboardPress(k, 'Left'); k = keyboardPress(k, 'Space'); assert.equal(k.text, 'PLAYERzQ q');
k = keyboardPress(k, 'Clear'); assert.equal(k.text, ''); for (let i = 0; i < 20; i++) k = keyboardPress(k, '1'); assert.equal(k.text.length, 15);
assert.ok(keyboardPress(k, 'Done').done);
assert.deepEqual(KEY_ROWS.map((r) => r.length), [14, 14, 13, 12, 5]);                              // the LUI row groups
{ const kb = data.screens.Fullkeyboard, by = new Map(kb.elements.map((e) => [e.name, e]));
  ['039115e3', '03e115e3', '03f115e3', '03c115e3', '03d115e3'].forEach((g, i) => assert.equal(by.get(g).children.length, KEY_ROWS[i].length, `keyboard row ${i}`)); }

// ---- flow (stub UI) ----
const mem = new Map();
globalThis.localStorage = { getItem: (k) => (mem.has(k) ? mem.get(k) : null), setItem: (k, v) => mem.set(k, String(v)), removeItem: (k) => mem.delete(k) };
function stubUi() {
  const ui = { screen: 'character', index: 0, ready: true, cb: {}, careerMode: false, onlineMode: false, log: [], synced: 0,
    set(s) { this.previousScreen = this.screen; this.screen = s; this.index = 0; this.fe?.enter(s, this.previousScreen); this.log.push(s); }, sync() { this.synced++; },
    rider: { id: 'zoe', name: 'Zoe' }, riders,
    characterSelect: { base: { id: 'zoe', name: 'Zoe' }, owned: [], unlocked() { return this.owned; }, openCheats() { ui.log.push('cheats'); }, overlay() { return false; },
      enterCheat(t) { ui.log.push('cheat:' + t); return t === 'worm' ? { name: 'Eddie' } : null; }, human() { return this.base; }, hidePreviewFor(f) { ui.log.push('hide:' + f); } },
    careerUI: null };
  const fe = new FeScreens(ui); ui.fe = fe; assert.ok(fe.init(data, {}));
  return { ui, fe };
}
const finish = (fe) => { const f = fe.flash; assert.ok(f, 'whitefade before the screen change'); fe.flash = null; f.to(); };
{
  const { ui, fe } = stubUi();
  assert.equal(DEFAULT_PLAYER_NAME, 'PLAYER1'); assert.equal(fe.playerName, defaultPlayerName(), 'pv playerName: PLAYER 1 (0x147170), else PLAYER1');
  ui.set('setup'); assert.ok(fe.owns('setup'));
  assert.equal(ui.log.at(-2), 'hide:26', 'preview hidden after the Select Character flash'); ui.set('details'); assert.equal(ui.log.at(-2), 'hide:30'); ui.set('setup'); assert.equal(ui.log.at(-2), 'hide:30');
  assert.deepEqual(fe.items(), ['Continue', 'Equip Gear', 'Rider Details', 'Music']);
  assert.equal(fe.disabled(1), true, 'Equip Gear needs web/wardrobe.js or cb.equipGear');
  ui.cb.equipGear = (r) => ui.log.push('equip:' + r.id); assert.equal(fe.disabled(1), false);
  fe.choose(1); assert.equal(ui.log.at(-1), 'equip:zoe');
  fe.choose(0); finish(fe); assert.equal(ui.screen, 'event', 'Continue -> the event flow');
  ui.set('setup'); ui.careerMode = true; ui.careerUI = { ready: true, enter() { ui.log.push('career'); } }; fe.choose(0); finish(fe); assert.equal(ui.log.at(-1), 'career');
  ui.careerMode = false; ui.careerUI = null; ui.onlineMode = true; ui.mpUI = { enter() { ui.log.push('online'); } }; ui.set('setup'); fe.choose(0); finish(fe); assert.equal(ui.log.at(-1), 'online');
  ui.onlineMode = false;
  // Music (140audio): Radio BIG checked by default, the custom playlists disabled without a playlist, the audio hook
  ui.set('setup'); fe.choose(3); finish(fe); assert.equal(ui.screen, 'fe-music');
  assert.deepEqual(fe.disabledList(), [false, false, true, true, true]);
  ui.cb.music = (m) => ui.log.push('music:' + m); fe.choose(1); assert.equal(ui.log.at(-1), 'music:ambience'); assert.equal(mem.get('ssx3.musicMode'), 'ambience');
  fe.back(); finish(fe); assert.equal(ui.screen, 'setup'); fe.focus(); assert.equal(ui.index, 3, 'back to Music');
  // Rider Details
  fe.choose(2); finish(fe); assert.equal(ui.screen, 'details');
  assert.deepEqual(fe.disabledList(), [true, true, true, false, false], 'no career: no Rewards/Ubertrick; no cheat owned');
  fe.focus(); assert.equal(ui.index, 3, 'first enabled item');
  ui.characterSelect.owned = [{ id: 'brodi' }]; assert.equal(fe.disabled(1), false);
  fe.choose(1); assert.equal(ui.log.at(-1), 'cheats');
  fe.choose(4); finish(fe); assert.equal(ui.screen, 'fe-profile');
  assert.equal(fe.page, 0); fe.turnPage(1); fe.turnPage(1); fe.turnPage(1); assert.equal(fe.page, 3); fe.turnPage(1); assert.equal(fe.page, 0); fe.turnPage(-1); assert.equal(fe.page, 3);
  assert.deepEqual(fe.items(), [data.strings.rider_bio]);
  fe.back(); finish(fe); assert.equal(ui.screen, 'details'); fe.focus(); assert.equal(ui.index, 4, 'back to Rider Profile');
  // Player Name (cKeyboardPopup): starts on Done with the current name
  fe.choose(3); assert.ok(fe.keyboard); assert.equal(fe.keyLabel(), 'Done'); assert.equal(fe.keyboard.text, defaultPlayerName());
  fe.keyPress('Clear'); for (const ch of 'sam') fe.keyPress(ch); fe.keyPress('~'); assert.equal(fe.keyboard.text, 'sam', 'punctuation is disabled for names');
  fe.keyPress('Done'); assert.equal(fe.keyboard, null); assert.equal(fe.playerName, 'sam'); assert.equal(mem.get('ssx3.playerName'), 'sam');
  fe.back(); finish(fe); assert.equal(ui.screen, 'setup');
  // Square: Options; Game Options -> the browser options screen and back; Enter Cheat -> CharacterSelect.enterCheat
  fe.openOptions('setup'); finish(fe); assert.equal(ui.screen, 'fe-options');
  fe.choose(0); finish(fe); assert.equal(ui.screen, 'fe-game', 'Game Options = 19game_opt');
  assert.deepEqual(fe.items(), ['Speed units', 'Widescreen', 'Screen position', 'Video calibration']);
  assert.deepEqual(fe.disabledList(), [false, false, false, true], 'Screen position on, Video calibration off');
  { // Screen position (cFEPopupScreenPos): +-1 per press, clamped to +-20, Cross keeps, Triangle restores; 1/320 width, 1/240 height
    const { screenTranslate, SCREEN_POS } = await import('./fe-options.js'); const x = fe.extra, k = (code) => x.key({ code }, 'fe-game');
    const stage = { style: {} }; ui.stage = stage; x.screenPosLui ??= { screen: { events: [] }, draw() {} };
    x.choose('fe-game', 2); assert.ok(x.screenPos, 'popup open');
    for (let i = 0; i < 25; i++) k('ArrowRight'); k('ArrowUp'); assert.deepEqual([x.screenPos.x, x.screenPos.y], [20, 1]); assert.equal(x.screenPos.arrow.frame, SCREEN_POS.arrows.up);
    assert.equal(stage.style.translate, '6.25% -0.4167%', 'live: 20/320 right, 1/240 up');
    k('Escape'); assert.equal(x.screenPos, null); assert.equal(stage.style.translate, '', 'Triangle restores'); assert.equal(x.options.screenX, 0);
    x.choose('fe-game', 2); k('ArrowLeft'); k('ArrowDown'); k('ArrowDown'); k('Enter'); assert.deepEqual([x.options.screenX, x.options.screenY], [-1, -2]); assert.ok(x.dirty);
    assert.equal(stage.style.translate, screenTranslate(-1, -2)); assert.equal(screenTranslate(-1, -2), '-0.3125% 0.8333%');
    x.reset('fe-game'); assert.equal(stage.style.translate, '', 'Square resets the position'); delete ui.stage; }
  { // an owned Video plays full screen (MoviePlayer 0x1D23E0) instead of the poster view
    const x = fe.extra, played = [], keep = { section: x.section, status: x.status, playMovie: x.playMovie };
    x.section = () => 'video'; x.status = () => ({ state: 'owned', item: { image: 'data/movies/intro.mpc' } }); x.playMovie = (item) => played.push(item.image);
    x.choose('fe-gallery'); assert.deepEqual(played, ['data/movies/intro.mpc']);
    x.movie = { item: {} }; x.stopMovie = function () { this.movie = null; played.push('stop'); };
    assert.ok(x.key({ code: 'ArrowLeft' }, 'fe-gallery'), 'modal while playing'); assert.ok(x.movie); x.key({ code: 'Enter' }, 'fe-gallery'); assert.equal(played.at(-1), 'stop');
    Object.assign(x, keep); delete x.stopMovie; }
  ui.widescreen = 0; ui.cycleWidescreen = (d) => { ui.widescreen = (ui.widescreen + d + 3) % 3; };
  fe.extra.change('fe-game', 1, 1); assert.equal(ui.widescreen, 1, 'Widescreen 16:9');
  fe.extra.change('fe-game', 0, 1); assert.equal(fe.extra.options.speedUnits, 1); assert.equal(mem.get('ssx3.feOptions'), undefined, 'kept only on Yes'); assert.ok(fe.extra.dirty);
  assert.ok(fe.extra.reset('fe-game')); assert.equal(fe.extra.options.speedUnits, 0); assert.equal(ui.widescreen, 0);
  fe.back(); finish(fe); assert.equal(ui.screen, 'fe-options'); fe.focus(); assert.equal(ui.index, 0, 'returns on Game Options');
  for (const [i, screen] of [[2, 'fe-control'], [3, 'fe-hud'], [6, 'fe-credits']]) { ui.index = i; fe.choose(i); finish(fe); assert.equal(ui.screen, screen); fe.back(); finish(fe); fe.focus(); assert.equal(ui.index, i); }
  ui.set('fe-hud'); fe.focus(); assert.equal(ui.index, 0); fe.extra.change('fe-hud', 0, 1); assert.equal(fe.extra.options.hud, 1); assert.deepEqual(fe.items(), ['Minimal']);
  // Controller 1P = Default / Pro (INPUT.MAP / INPUT2.MAP) for pad and keyboard; the race reads fe.inputMap()
  assert.deepEqual(fe.extra.value('fe-control', 2), { vals: ['kT_20PresetDefault', 'kT_OVRCMNPro'], index: 0 }); assert.equal(fe.inputMap(), 0);
  fe.extra.change('fe-control', 2, 1); assert.equal(fe.inputMap(), 1); fe.extra.change('fe-control', 2, 1); assert.equal(fe.inputMap(), 0);
  assert.equal(fe.vibration(), true, 'Vibration 1P On by default'); fe.extra.change('fe-control', 0, 1); assert.equal(fe.vibration(), false); fe.extra.change('fe-control', 0, 1); assert.equal(fe.vibration(), true);
  { const { keyboardProRows } = await import('./fe-options.js'); const { keyboardRows } = await import('./loading-screen.js');
    const s = { turn: 'Turn', turn_spin_flip: 'Turn/spin/flip', jump: 'Jump', hand_plant: 'Hand plant', boost_tweak: 'B', grab_board: 'G', board_press: 'P', reset: 'R', pause: 'Pa' };
    const pro = keyboardProRows(keyboardRows('Classic', s), s); assert.ok(!pro.some((r) => r.keys.includes('I')), 'no IJKL spins in Pro'); assert.deepEqual(pro.find((r) => r.label === 'Hand plant').keys, ['Y']); }
  ui.set('fe-options');
  fe.choose(5); assert.equal(fe.keyLabel(), 'q', 'Enter Cheat keyboard starts on q (PS2 frame)'); fe.keyboard.text = 'worm'; fe.keyPress('Done');
  assert.equal(ui.log.at(-1), 'cheat:worm');
  // leaving Options after a change: "Would you like to save your Options?" (PS2 frame); No keeps them for the session
  // Display & Touch (web/fe-options.js displayScreen / optionsWithDisplay): above DONE, built from Game Options' rows
  assert.deepEqual(fe.items(), ['Game Options', 'Sound Options', 'Controller Settings', 'HUD Options', 'Save/Load', 'Enter Cheat', 'Credits', 'Display & Touch', 'DONE']);
  assert.equal(fe.displayIndex(), 7); assert.equal(fe.disabled(7), false);
  { const { DISPLAY_ROWS, TEXTURE_SET_ROW } = await import('./fe-options.js'); const { quality, defaultRiderTextures, device } = await import('./quality.js'); const { pv } = await import('./pv-flags.js');
    const dirty = fe.extra.dirty; fe.extra.dirty = false;
    // rows by what they set (the Texture set row sits after PS2 softness only with pv xboxRiders)
    const row = (f) => DISPLAY_ROWS.findIndex(f), iUpscale = row((r) => r.q === 'upscale'), iSoft = row((r) => r.q === 'ps2Output'), iTier = row((r) => r.q === 'tier');
    const iShow = row((r) => r.t === 'show'), iLayout = row((r) => r.t === 'layout'), iTextures = row((r) => r.q === 'riderTextures');
    fe.choose(7); finish(fe); assert.equal(ui.screen, 'fe-display');
    assert.deepEqual(fe.items(), DISPLAY_ROWS.map((r) => r.label));
    assert.deepEqual(fe.disabledList(), DISPLAY_ROWS.map((r) => !!r.deck), 'no touch controller: layout / stick / vibration greyed');
    // Texture set (docs/xbox-textures.md section 8): with pv xboxRiders right after PS2 softness, enabled, PS2 / Xbox HD, the device's
    // default first, a change saved with the other quality settings; without the switch no row
    if (pv('xboxRiders')) {
      assert.equal(iTextures, iSoft + 1); assert.equal(DISPLAY_ROWS[iTextures], TEXTURE_SET_ROW); assert.equal(fe.disabled(iTextures), false);
      assert.deepEqual(TEXTURE_SET_ROW.values.map(([v]) => v), ['ps2', 'xbox']);
      const was = quality.riderTextures; assert.equal(was, defaultRiderTextures(device, quality.tier));
      fe.extra.change('fe-display', iTextures, 1); assert.notEqual(quality.riderTextures, was); assert.equal(JSON.parse(mem.get('ssx3.quality')).riderTextures, quality.riderTextures);
      fe.extra.change('fe-display', iTextures, 1); assert.equal(quality.riderTextures, was, 'PS2 / Xbox HD wraps');
    } else assert.equal(iTextures, -1);
    const touch = { shown: true, settings: { layout: 'split', stick: 'floating', haptics: true, show: 'auto' }, log: [], setSetting(k, v) { this.settings[k] = v; this.log.push(`${k}=${v}`); } };
    ui.touchControls = touch; assert.equal(fe.disabled(iLayout), false);
    fe.extra.change('fe-display', iLayout, 1); assert.deepEqual(touch.log, ['layout=below']);
    assert.equal(fe.extra.value('fe-display', iLayout).index, 1);
    const tier = quality.tier, next = ['low', 'medium', 'high'][(['low', 'medium', 'high'].indexOf(tier) + 1) % 3];
    fe.extra.change('fe-display', iTier, 1); assert.ok(fe.prompt, 'Quality asks first (Yes/No)'); assert.equal(quality.tier, tier);
    fe.prompt.no(); assert.equal(quality.tier, tier, 'No keeps the tier');
    fe.extra.change('fe-display', iTier, 1); fe.prompt.yes(); assert.equal(quality.tier, next, 'Yes applies it (no reload)'); assert.equal(JSON.parse(mem.get('ssx3.quality')).tier, next);
    fe.extra.change('fe-display', iShow, -1); assert.ok(fe.prompt, 'hiding the controller asks'); assert.equal(fe.prompt.index, 1, 'No focused'); fe.prompt.yes(); assert.deepEqual(touch.log.at(-1), 'show=off');
    fe.extra.change('fe-display', iUpscale, 1); assert.equal(quality.upscale, 'pixelated');
    // PS2 softness: Off by default, On saved with the other quality settings (web/fog-renderer.js setSoftness)
    assert.equal(DISPLAY_ROWS[iSoft].label, 'PS2 softness'); assert.equal(iSoft, 2); assert.equal(quality.ps2Output, false); assert.equal(fe.extra.value('fe-display', iSoft).index, 0);
    fe.extra.change('fe-display', iSoft, 1); assert.equal(quality.ps2Output, true); assert.equal(JSON.parse(mem.get('ssx3.quality')).ps2Output, true);
    fe.extra.change('fe-display', iSoft, 1); assert.equal(quality.ps2Output, false, 'Off / On wraps');
    assert.equal(fe.extra.dirty, false, 'saved in their own keys, not part of the save question');
    fe.back(); finish(fe); assert.equal(ui.screen, 'fe-options'); fe.focus(); assert.equal(ui.index, 7);
    // the deck's ≡ during a paused race: Display & Touch over the pause menu, back returns there
    ui.set('pause'); ui.index = 3; assert.ok(fe.openDisplay('pause')); finish(fe); assert.equal(ui.screen, 'fe-display');
    fe.back(); finish(fe); assert.equal(ui.screen, 'pause'); assert.equal(ui.index, 3);
    ui.touchControls = null; ui.set('fe-options'); fe.extra.dirty = dirty; }
  fe.choose(8); assert.ok(fe.prompt, 'save prompt'); assert.equal(fe.prompt.message, 'Would you like to save your Options?');
  fe.prompt.index = 1; fe.prompt.no(); finish(fe); assert.equal(ui.screen, 'setup', 'DONE returns'); assert.equal(mem.get('ssx3.feOptions'), undefined); assert.equal(fe.hudLevel(), 1);
  ui.set('fe-options'); fe.extra.change('fe-hud', 0, 1); fe.back(); assert.ok(fe.prompt); fe.prompt.yes(); finish(fe);
  assert.equal(JSON.parse(mem.get('ssx3.feOptions')).hud, 2, 'Yes saves'); assert.equal(fe.minimalHudFlags(0x1530c047), 0x10200007);
  // Load game (Circle on Select Character): the browser save; Loading..., then back
  ui.set('character'); fe.openLoad('character'); finish(fe); assert.equal(ui.screen, 'fe-load');
  assert.equal(fe.saveInfo(), null); assert.deepEqual(fe.disabledList(), [true, true, true, true, true, true]);
  const storage = { getItem: () => JSON.stringify({ version: 1, riders: { zoe: { cash: 5 } } }) };
  ui.careerUI = { career: { storage, save: null, rules: { records: [] } } };
  assert.equal(fe.saveInfo().name, 'sam'); assert.equal(fe.disabled(0), false);
  fe.choose(0); assert.ok(fe.loadState); assert.equal(ui.careerUI.career.save.riders.zoe.cash, 5);
  fe.back(); finish(fe); assert.equal(ui.screen, 'character');
}
// Ubertrick Setup with a career: owned ubers only (buying stays in the lodge), the choice reaches the rider settings
{
  const { ui, fe } = stubUi();
  const save = { riders: {} }, career = { shop, save, rider: (id) => (save.riders[id] ??= { character: 4, cash: 0 }), uber(id) { return this.rider(id).uber ??= initialUber(shop, 4); },
    uberStatus(id, cat, entry) { const u = this.uber(id)[cat], e = uberEntries(shop, cat)[entry]; if (!u || !e || e.hidden || !(u.visible >> entry & 1)) return null; if (!(u.lock >> entry & 1)) return { entry: e, state: u.selected === entry ? 'selected' : 'owned' }; return { entry: e, state: 'buy' }; },
    selectUber(id, cat, entry) { this.uber(id)[cat].selected = entry; ui.log.push(`uber:${cat}:${entry}`); return true; } };
  ui.careerUI = { career }; let reloaded = null; ui.cb.rider = async (r) => { reloaded = r; };
  ui.set('details'); fe.choose(2); finish(fe); assert.equal(ui.screen, 'fe-uber');
  assert.deepEqual(fe.items(), ['Mute', 'Indy', 'Stalefish', 'Method', 'Nose Grab', 'Tail Grab', 'DONE']);
  ui.index = 3; fe.choose(3); assert.ok(fe.uberList);
  const list = fe.uberEntries(); assert.deepEqual(list.map((x) => x.entry.name), ['jib O', 'Indian', 'Hand in Hand', 'Kort Martial']);
  assert.equal(fe.uberIndex, 0, 'the list opens on its first row (PS2)');
  assert.equal(fe.uberClipName(riderClips, list[1].entry), 'UBER_METHOD_2_L2', 'Indian (trick id 123) previews fe:30');
  fe.uberIndex = 1; fe.previewTrick(); assert.equal(fe.uberAnim.entry.name, 'Indian');
  fe.uberIndex = 0; fe.choose(3); assert.equal(ui.log.filter((l) => l.startsWith('uber:')).length, 0, 'a locked uber cannot be picked here');
  career.uber('zoe')[0].lock &= ~(1 << 2);                                                           // bought in the lodge
  fe.choose(3); assert.equal(ui.log.at(-1), 'uber:0:2'); assert.equal(reloaded?.id, 'zoe', 'rider reloads with the new uber');
  const doc = await loadCharacter({ id: 'zoe', package: 'RIDER_ZOE', settings: false });
  assert.equal(humanSettings(initial, doc).original_grab_control.profile.uber[1][0].semantic, 120);
  setUberChoice('zoe', []);
  fe.back(); assert.equal(fe.uberList, false); ui.index = 6; fe.choose(6); finish(fe); assert.equal(ui.screen, 'details');
}
// ---- extra screens data ----
{
  const { rowModel, creditLines, saveDateTime, EXTRA_SCREENS } = await import('./fe-options.js');
  for (const k of ['19game_opt', '22control', '21hud_opt', '26credits', '128rewardsroom', '129rewardgallery', '130rewardposter', '141advsettings']) assert.ok(data.screens[k], k);
  assert.deepEqual(rowModel(data.screens['21hud_opt']).frames, [30, 40, 50]);
  assert.deepEqual(rowModel(data.screens['22control']).texts, ['Vibration 1P', 'Vibration 2P', 'Controller 1P', 'Controller 2P']);
  assert.deepEqual(rowModel(data.screens['128rewardsroom']).texts, ['Art', 'Posters', 'Toys', 'Trading Cards', 'Cheat Characters', 'Videos']);
  assert.equal(data.credits.length, 160, 'kT_CREDITS lines (CRAMER.LOC)');
  const cr = creditLines(data.credits); assert.equal(cr.lines[0].text, 'Developed at EA CANADA - WWW.EACANADA.COM'); assert.equal(cr.lines.find((l) => l.text === 'Development Team').heading, true);
  // PS2 frame (sample 570): 'Development Team' 40 px below the first line, 'Executive Producer' 40 more, names 20 px
  const at = (t) => cr.lines.find((l) => l.text === t).y;
  assert.equal(at('Development Team') - at('Developed at EA CANADA - WWW.EACANADA.COM'), 40); assert.equal(at('Steven Rechtschaffner') - at('Executive Producer'), 40);
  assert.equal(at('Co-Producers') - at('Larry LaPierre'), 40); assert.equal(at('J. David Elton') - at('Co-Producers'), 40); assert.equal(at('Production Team') - at('Conor Lumpkin'), 40);
  const dt = saveDateTime(new Date(2026, 8, 23, 14, 5, 9).getTime()); assert.deepEqual(dt, { date: '09:23:2026', time: '14:05:09' });
  assert.equal(saveDateTime(undefined), null, 'old saves without savedAt');
  for (const k of ['kT_CMNMPH', 'kT_CMNKPH', 'kT_CMNHUDFull', 'kT_20PresetDefault', 'kT_OVRCMNPro']) assert.ok(data.strings[k], k);
  assert.equal(EXTRA_SCREENS.length, 7);
}
// save timestamp (web/career-save.js): written on every save, old saves still load
{
  const { writeSave, loadSave } = await import('./career-save.js');
  const store = new Map(), st = { getItem: (k) => store.get(k) ?? null, setItem: (k, v) => store.set(k, v) };
  store.set('ssx3.career.v1', JSON.stringify({ version: 1, riders: {} })); assert.ok(loadSave(st));
  writeSave({ version: 1, riders: {} }, st); assert.ok(Number.isFinite(loadSave(st).savedAt));
}
{ // Vibration (web/rumble.js): the option gates the motors; the model itself is checked against the PS2 in web/test-rumble.mjs
  const { Rumble, AE_RUMBLE_IMPACT } = await import('./rumble.js');
  const calls = [], act = { playEffect: (type, p) => { calls.push([type, p]); return Promise.resolve(); }, reset: () => { calls.push(['reset']); return Promise.resolve(); } };
  const q = new Float32Array(1 + 64 * 5); q[0] = 1; q.set([AE_RUMBLE_IMPACT, 4989.197, 0, 0, 0], 1);
  const core = { HEAPF32: q, _audio_events: () => 0 }, r = new Rumble(() => ({ vibrationActuator: act }));
  r.tick(core, true); assert.equal(calls.at(-1)[0], 'dual-rumble'); assert.equal(calls.at(-1)[1].weakMagnitude, 1);
  r.tick(core, false); assert.equal(calls.at(-1)[0], 'reset', 'Vibration Off stops the motors'); const n = calls.length; r.tick(core, false); assert.equal(calls.length, n);
}
console.log('fe-screens ok');
