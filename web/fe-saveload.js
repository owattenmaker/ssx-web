// Options > Save/Load: the original FE.LUI screen 25saveload (0x08CE8C54; tools/export_fe_menus.py -> UI/fe-menus.json),
// played by web/lui-player.js. docs/characters.md "Saving progress".
//
//   Save game      writes the career now (the career also autosaves after every event, purchase and lodge change;
//                  web/career-save.js, atomic, see web/save-store.js)
//   Load game      the Load game screen (93profile_load, web/fe-screens.js fe-load) with the save's row
//   Save options   / Load options: the FE options (ssx3.feOptions) written now / read back from storage
//   Load replay    greyed: replays are not recorded in the port
//   New game       "Erase all saved progress?" (the Yes/No popup, No focused), then a fresh career
//   Export save file / Import save file   browser additions below the original rows (same style): download the
//                  whole save (every ssx3.* progress/settings key, save-store.js exportSave) as JSON, or load one
//                  (validated, migrated, written atomically) and restart the page with it
import { LuiScreen } from './lui-player.js';
import { menuModel, stepMenu, firstEnabled, defaultPlayerName } from './fe-screens.js';
import { loadFeOptions, saveFeOptions } from './fe-options.js';
import { downloadSave, pickSaveFile, importSave, storageWorks } from './save-store.js';
import { migrateCareer, clearSave } from './career-save.js';
import { saveSelection } from './save-store.js';
import { pv } from './pv-flags.js';

const SY = 448 / 480, FPS = 60, FLASH = 10, DISABLED_ALPHA = 128, ROOT = '/assets/UI/';
export const SAVELOAD_SCREEN = 'fe-saveload', SAVELOAD_LUI = '25saveload';
export const EXTRA_ITEMS = ['Export save file', 'Import save file'];
const EXTRA_HELP = ['Download your progress and settings as a save file.', 'Load a save file you exported before.'];
const ROW_STEP = 22;

export function saveLoadDisabled({ career = false, loadable = false } = {}) { return [!career, !loadable, false, false, true, !career, false, false]; }

export class FeSaveLoad {
  constructor(ui) { this.ui = ui; this.model = null; this.lui = null; this.images = {}; this.enterAt = 0; this.focusAt = 0; this.flash = null; this.notice = null; this.busy = false; }
  get ready() { return !!this.model; }
  now() { return performance.now() * FPS / 1000; }
  get fe() { return this.ui.feScreens; }
  get career() { return this.ui.careerUI?.career || null; }
  async load() {
    try {
      const data = await (await fetch(ROOT + 'fe-menus.json')).json(), screen = data?.screens?.[SAVELOAD_LUI], model = screen && menuModel(screen);
      if (!model || model.items.length !== 6) return false;
      const have = this.ui.characterSelect?.images || {};
      await Promise.all((data.pages || []).filter((p) => !have[p]).map(async (p) => { try { const im = new Image(); im.src = ROOT + p + '.png'; await im.decode(); this.images[p] = im; } catch {} }));
      const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop;
      const merged = snow ? { ...screen, elements: [...screen.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))], animations: { ...screen.animations, ...snow.animations } } : screen;
      this.lui = new LuiScreen(merged, { ...have, ...this.images }, this.ui); this.model = model;
      return true;
    } catch (e) { console.warn('Save/Load screen unavailable', e); return false; }
  }
  owns(screen) { return this.ready && screen === SAVELOAD_SCREEN; }
  items() { return [...this.model.texts, ...EXTRA_ITEMS]; }
  disabledList() { return saveLoadDisabled({ career: !!this.career, loadable: !!this.fe?.saveInfo?.() }); }
  disabled(i) { return !this.ui.ready || !!this.disabledList()[i]; }
  layout(i) { return [292, (157 + ROW_STEP * i) * SY, 250, 20 * SY]; }   // left-aligned rows at x 298
  focus() { const dis = this.disabledList(); if (!(this.ui.index >= 0 && this.ui.index < dis.length)) this.ui.index = 0; if (dis[this.ui.index]) this.ui.index = firstEnabled(dis, this.ui.index); return this.ui.index; }
  enter(screen, from) {
    if (screen !== SAVELOAD_SCREEN) return;
    const now = this.now(); if (from !== screen && from !== 'fe-load') this.enterAt = now; this.focusAt = now;
    if (from === 'fe-options') this.from = from;
    if (this.returnIndex != null) { this.pending = this.returnIndex; this.returnIndex = null; }
  }
  say(text, seconds = 3) { this.notice = { text, until: this.now() + seconds * FPS }; }
  go(screen, index = null) { this.flash = { at: this.now(), to: () => { this.ui.set(screen); if (index != null) { this.ui.index = index; this.ui.sync(); } } }; }

  key(e) {
    if (!this.owns(this.ui.screen)) return false;
    if (this.fe?.prompt) return this.fe.promptKey(e);   // New game? Yes / No (the FE.LUI popup)
    if (['ArrowUp', 'ArrowDown', 'Enter', 'Space', 'Escape'].includes(e.code)) e.preventDefault();
    if (this.flash || this.busy) return true;
    if (e.repeat && e.code !== 'ArrowUp' && e.code !== 'ArrowDown') return true;
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { this.ui.index = stepMenu(this.focus(), e.code === 'ArrowUp' ? -1 : 1, this.disabledList()); this.focusAt = this.now(); this.ui.sync(); return true; }
    if (e.code === 'Enter' || e.code === 'Space') { this.choose(this.ui.index); return true; }
    if (e.code === 'Escape') { this.back(); return true; }
    return true;
  }
  choose(i) {
    const ui = this.ui; if (!ui.ready || this.flash || this.busy || this.fe?.prompt || this.disabled(i)) return;
    ui.index = i; this.focusAt = this.now();
    // pv lodgeSave: Save game opens the Save game screen (cFEStateProfileLoad in save mode, web/save-game.js) as the lodge's does;
    // Continue after 'Save complete.' (or Triangle) comes back here on Save game
    if (i === 0 && pv('lodgeSave') && this.ui.careerUI?.lodge?.saveGame?.ready()) { const g = this.ui.careerUI.lodge.saveGame; this.flash = { at: this.now(), to: () => g.open({ back: () => { this.ui.set(SAVELOAD_SCREEN); this.ui.index = 0; this.ui.sync(); } }) }; return; }
    if (i === 0) { const ok = this.career?.persist(); this.say(ok ? 'Save complete.' : 'Save failed: browser storage is full or blocked.'); }
    else if (i === 1) { this.returnIndex = 1; this.fe.loadFrom = SAVELOAD_SCREEN; this.fe.returnTo[SAVELOAD_SCREEN] = { index: 1 }; this.go('fe-load'); }
    else if (i === 2) { const x = this.fe?.extra; if (x) x.commit(); else saveFeOptions(loadFeOptions()); this.say(storageWorks() ? 'Options saved.' : 'Save failed: browser storage is full or blocked.'); }
    else if (i === 3) { const x = this.fe?.extra; if (x) { x.options = loadFeOptions(); x.dirty = false; x.applyScreenPosition?.(); ui.cb?.feOptions?.(x.options); } this.say('Options loaded.'); }
    else if (i === 5) this.askNewGame();
    else if (i === 6) { try { downloadSave(); this.say('Save file downloaded.'); } catch (e) { console.warn(e); this.say('Export failed.'); } }
    else if (i === 7) this.importFile();
  }
  askNewGame() {
    const fe = this.fe; if (!fe) return;
    const done = (yes) => { fe.prompt = null; if (yes) this.newGame(); this.ui.sync(); };
    fe.prompt = { at: fe.now(), index: 1, message: 'Erase all saved progress?', yes: () => done(true), no: () => done(false) };
    this.ui.sync();
  }
  // A fresh Conquer the Mountain career (every rider's record, cash, medals, purchases); options and the free-play
  // outfits are settings and stay. The old save is gone for good, hence the question (and Export before it).
  // pv newGameReset (docs/ctm-parity.md "New game"): the PS2's Yes (0x18D460 event 0x16 -> 0x18D4F4..0x18D5E4) resets profile 0 only:
  // 0x147138 clears the player name, 0x149A88(0) sets the slot's rider to Zoe (+0x11 = 4) without a cheat skin (+0x12 = 0), names it
  // kT_MEMPlayerName 1 and runs 0x1567B8(0x4A6CA8, 1) = 0x151600 for the ten riders: each career block as at boot, including the
  // relationships (+0xBC1, 0x1519E0), the outfit records (0x151A88 / 0x1513B8) and the reward / unlock block (+0xF30..+0xF7F,
  // 0x151988: the cheat characters owned, Enter Cheat's too: 0x187D38 grants them with 158618). The records (0x535C18) live in the
  // options file (0x152758) and are kept, as are the options. Off: the records were reset, the rest kept.
  newGame() {
    const c = this.career; if (!c) return;
    const reset = pv('newGameReset'), records = reset && Array.isArray(c.save?.records) ? c.save.records : null;
    clearSave(c.storage);
    c.save = { version: 2, seed: 0x13572468, records: records ?? c.rules.records.map((slot) => slot.map((r) => ({ ...r }))), riders: {} };
    if (reset) {
      const s = (() => { try { return c.storage ?? globalThis.localStorage; } catch { return null; } })();
      for (const k of ['ssx3.relationships.v1', 'ssx3.outfit.free.v1', 'ssx3.outfit.v1', 'ssx3.cheatCharacters']) { try { s?.removeItem(k); } catch {} }
      import('./ai-race.js').then((m) => m.relationshipsReset?.()).catch(() => {});   // a loaded race's tables are reloaded at its next start
      const name = defaultPlayerName(); try { s?.setItem('ssx3.playerName', name); } catch {}
      if (this.fe) this.fe.playerName = name; this.ui.playerName = name;
      saveSelection({ rider: 'zoe', base: null });
      const zoe = (this.ui.riders || []).findIndex((r) => r.id === 'zoe'); if (zoe >= 0) this.ui.riderIndex = zoe;
    }
    this.say(c.persist() ? 'New game started.' : 'Save failed: browser storage is full or blocked.');
  }
  async importFile() {
    this.busy = true;
    try {
      const file = await pickSaveFile();
      if (!file) { this.busy = false; return; }
      const r = file.invalid ? { ok: false, error: 'That file is not a save file.' } : importSave(file, undefined, { migrateCareer });
      if (!r.ok) { this.say(r.error, 5); this.busy = false; return; }
      this.say('Save file loaded. Restarting...', 5);
      setTimeout(() => location.reload(), 1200);   // every module re-reads its part of the save at start-up
    } catch (e) { console.warn(e); this.say('Import failed.'); this.busy = false; }
  }
  back() {
    if (this.flash || this.fe?.prompt) return;
    this.go('fe-options', 4);
  }
  draw(c, b) {
    const now = this.now();
    if (this.flash && now - this.flash.at >= FLASH) { const f = this.flash; this.flash = null; f.to(); return; }
    if (this.pending != null) { this.ui.index = this.pending; this.pending = null; }
    this.focus();
    const i = this.ui.index, frame = now - this.enterAt, model = this.model, dis = this.disabledList();
    const events = [], start = this.focusAt - this.enterAt, focus = model.frames[Math.min(i, 5)];
    for (const ev of this.lui.screen.events) {
      if (ev.frame <= model.intro && ev.frame <= frame) events.push({ ev, start: ev.frame });
      else if (ev.frame === focus) events.push({ ev, start });
    }
    const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) events.push({ ev, start: frame - sf + ev.frame });
    const rows = new Map(model.items.map((n, k) => [n, k])), extra = i >= 6, notice = this.notice && now < this.notice.until ? this.notice.text : null;
    const helpNames = new Set(['08ae1902', '0a349062', '0f0d8642', '051f1842', '014485c2', '02df57c2']);
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    this.lui.draw(c, events, frame, (e) => {
      if (rows.has(e.name)) { const k = rows.get(e.name); return extra ? { props: { 14: 37, 15: 7, 16: 5 }, alpha: dis[k] ? DISABLED_ALPHA : 255 } : dis[k] ? { alpha: DISABLED_ALPHA } : null; }
      if (e.label === 'hl' && extra) return { props: { 1: 157 + ROW_STEP * i - 1 } };
      if (helpNames.has(e.name)) {
        if (notice || extra) return e.name === '02df57c2' ? { text: notice || EXTRA_HELP[i - 6], alpha: 255, props: { 14: 255, 15: 255, 16: 255 } } : { hidden: true };
      }
      return null;
    });
    // the browser rows, in the style of 'New game' (298, 265 + 22 k, 60%, right-aligned in the menu)
    const nw = this.lui.byName.get('0c96a2b5'), p = this.lui.props(nw, events, frame);
    EXTRA_ITEMS.forEach((t, k) => { const on = i === 6 + k; this.lui.text(c, t, p[0] ?? 298, 157 + ROW_STEP * (6 + k), { ...p, 14: on ? 255 : 37, 15: on ? 255 : 7, 16: on ? 255 : 5 }, 1); });
    if (this.flash) { c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash.at) / FLASH)})`; c.fillRect(0, 0, 640, 480); }
    c.restore();
    if (this.fe?.prompt) this.fe.drawPrompt(c, this.fe.now());   // scales itself to the 480-line space
  }
}
