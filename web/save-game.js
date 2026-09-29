// The lodge's Save Game (pv lodgeSave): the PS2's Save game screen and its memory-card messages, drawn for web/lodge-ui.js.
// 0x1F3A38 (the lodge's input, menu value 8) pushes cFEStateProfileLoad (0x18ECA0 with mode 3 and the pad's port mask): FE.LUI
// 93profile_load in save mode ('Save game', 'Save your progress.', Save / Previous) over the card's six rows, the card in a box
// above them (kT_MEMMemDevicePS2). Cross on a row: the name keyboard (kT_KEYUserName, the player name), then the memory-card
// manager's popups (0x1B6xxx: kT_MEMFECheckingCardPS2, kT_MEMOVOverWriteFile with Yes / No and No focused over a save,
// kT_MEMOVSaveSavingPS2NGC, kT_MEMOVSaveDone with Continue), and Continue returns to the lodge. PS2 captures: local/ps2-capture/menus
// save-s1..s8 (a card in slot 1: unformatted, formatted, with a save), lodge/35-save-game (no card).
// The browser's card is localStorage (web/career-save.js, one career save): row 1 is that save (the player name) or < E M P T Y >,
// the other rows are not drawn (as the port's Load game); no card = the storage is blocked. Delete (Square) is not in the port.
import { format } from './locale.js';
import { LuiScreen } from './lui-player.js';
import { fePopupLayout, drawFePopup } from './fe-popup.js';   // the front end's popup box as cFEPopup sizes and draws it (0x1C6B08)

const SY = 448 / 480, FPS = 60;
export const SAVE_SCREEN = 'ctm-save';
// The memory-card popups (0x1B7650 -> cFEPopup): type 1 (framed), compact (the 300 base), the options Yes / No or Continue, laid out by
// web/fe-popup.js as the popup code does (checked against the live popup objects of save-s3 / s4 / s7 / s8).
const LIST_FRAMES = { intro: 50, row1: 55 };                                    // 93profile_load: row 1's focus frame (label 0728fcb1)
const CHECK = 60, SAVING = 90;                                                  // frames the Checking / Saving messages stay up (port)

export class SaveGame {
  constructor(lodge) { this.lodge = lodge; this.phase = null; this.at = 0; this.enterAt = 0; this.ask = 1; this.ok = false; this.popupLui = null; }
  get ui() { return this.lodge.ui; }
  get fe() { return this.ui.feScreens; }
  get cs() { return this.lodge.cs; }
  t(key, fallback) { return this.cs.t(key, fallback); }
  now() { return performance.now() * FPS / 1000; }
  ready() { return !!(this.fe?.lui?.['fe-load'] && this.fe.popupLui && this.cs.career); }
  owns(s) { return s === SAVE_SCREEN && this.ready(); }
  card() { const c = this.cs.career; try { const s = c.storage ?? globalThis.localStorage; if (!s) return false; s.setItem('ssx3.probe', '1'); s.removeItem('ssx3.probe'); return true; } catch { return false; } }
  saved() { return !!this.fe?.saveInfo?.(); }
  name() { return this.fe?.playerName || ''; }
  // the memory-card manager keeps one popup up from Checking to Save complete (0x1A9BC0: shown once), so its veil fades in once
  set(phase) { const pop = (x) => ['check', 'ask', 'saving', 'done', 'failed'].includes(x); if (pop(phase) && !pop(this.phase)) this.popupAt = this.now(); this.phase = phase; this.at = this.now(); this.ui.sync(); }
  // back: where Triangle on the rows and Continue after 'Save complete.' go (default the lodge on Save Game; Options > Save/Load passes its own)
  open({ back = null } = {}) {
    if (!this.ready()) return false;
    this.backTo = back;
    this.enterAt = this.lodge.cs?.lodgeFlash?.introStart?.(this.now()) ?? this.now(); this.phase = 'list'; this.at = this.now();
    this.ui.set(SAVE_SCREEN); this.ui.index = 0; this.ui.sync();
    return true;
  }
  // back to the lodge through its state change's flash (pv lodgeFlash, web/lui-flash.js: this screen under the rising white, the lodge's
  // intro under the fall); Options > Save/Load's own return (backTo) is the front end's.
  leave() {
    const back = this.backTo; if (back) { this.phase = null; if (this.fe) this.fe.keyboard = null; this.backTo = null; back(); return; }
    const to = () => { this.phase = null; if (this.fe) this.fe.keyboard = null; this.ui.set('ctm-lodge'); this.ui.index = 6; this.ui.sync(); };
    const cs = this.lodge.cs; if (cs?.lodgeGo) cs.lodgeGo(to); else to();
  }
  // pointer targets: row 1 (menu 07653c55 at 140, 200), or the popup's options
  items() {
    if (this.phase === 'list') return this.card() ? [this.saved() ? this.name() : this.t('kT_MEMEmpty', '< E M P T Y >')] : [];
    return this.popup()?.options || [];
  }
  layoutRect(i) {
    if (this.phase === 'list') return i === 0 ? [140, 200 * SY, 460, 20 * SY] : [0, -100, 1, 1];
    const p = this.popup(); if (!p?.options?.[i]) return [0, -100, 1, 1];
    const L = this.popupLayout(p), y = L.menu[1] + 120 + 20 * i;                     // Option k at (270, 120 + 20 k) in the menu, v-centred
    return [L.menu[0] + 250, (y - 10) * SY, 140, 20 * SY];
  }

  // ---- input ----
  key(e) {
    const fe = this.fe, code = e.code;
    if (['ArrowUp', 'ArrowDown', 'Enter', 'Space', 'Escape'].includes(code)) e.preventDefault();
    if (this.phase === 'name') {
      if (code === 'Escape') { fe.keyboard = null; this.set('list'); return true; }             // Triangle: back to the rows
      fe.keyboardKey(e); if (!fe.keyboard) this.named(); return true;
    }
    if (e.repeat) return true;
    const cross = code === 'Enter' || code === 'Space', tri = code === 'Escape';
    switch (this.phase) {
      case 'list':
        if (tri) { this.leave(); return true; }
        if (cross && this.card()) { fe.openKeyboard('name'); this.phase = 'name'; this.at = this.now(); }
        return true;
      case 'ask':
        if (code === 'ArrowUp' || code === 'ArrowDown') { this.ask = 1 - this.ask; this.ui.sync(); }
        else if (cross) { if (this.ask === 0) this.write(); else this.set('list'); }
        else if (tri) this.set('list');
        return true;
      case 'done': case 'failed':
        if (cross) { if (this.phase === 'done') this.leave(); else this.set('list'); }
        return true;
    }
    return true;
  }
  choose(i) { if (this.phase === 'ask') this.ask = i; if (['list', 'ask', 'done', 'failed'].includes(this.phase)) this.key({ code: 'Enter', preventDefault() {} }); }
  back() { if (this.phase === 'name') { this.fe.keyboard = null; this.set('list'); } else if (this.phase === 'list') this.leave(); else if (this.phase === 'ask') this.set('list'); }
  named() { this.hadSave = this.saved(); this.set('check'); }                    // Done: the name is the player name (fe-screens keyboardDone)
  write() { this.set('saving'); this.ok = !!this.cs.career.persist(); }
  tick() {
    const t = this.now() - this.at;
    if (this.phase === 'name' && !this.fe.keyboard) this.named();                 // Done by pointer
    else if (this.phase === 'check' && t >= CHECK) { if (this.hadSave) { this.ask = 1; this.set('ask'); } else this.write(); }
    else if (this.phase === 'saving' && t >= SAVING) this.set(this.ok ? 'done' : 'failed');
  }

  // ---- drawing ----
  draw(c, b) {
    if (!this.owns(this.ui.screen)) return false;
    this.tick();
    const fe = this.fe, lui = fe.lui['fe-load'], now = this.now(), frame = now - this.enterAt, card = this.card(), saved = card && this.saved();
    const events = [];
    for (const ev of lui.screen.events) {
      if (ev.frame <= LIST_FRAMES.intro && ev.frame <= frame) events.push({ ev, start: ev.frame });
      else if (card && ev.frame === LIST_FRAMES.row1) events.push({ ev, start: LIST_FRAMES.intro });
    }
    const snow = fe.data.screens.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) events.push({ ev, start: frame - sf + ev.frame });
    const hide = new Set(['00ff4fa5', '0224c154', '09acf751', '069acf75', '007769a1', '09d6faf4', '0d65eed4', '0006a8a5', '0007b035',
      '00000032', '00000033', '00000034', '00000035', '00000036', '0728fcb2', '0728fcb3', '0728fcb4', '0728fcb5', '0728fcb6']);
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    lui.draw(c, events, frame, (e) => {
      const n = e.name;
      if (hide.has(n)) return { hidden: true };
      if (n === '006afdc5') return { alpha: 255 };
      if (n === '02206ce5') return card ? { alpha: 255 } : { hidden: true };                                   // 'Save game', 'Save your progress.'
      if (n === '0f06f0f5') return card ? { hidden: true } : { text: fe.t('insert_card', 'Please insert a memory card (PS2) into MEMORY CARD slot 1.'), alpha: 255 };   // no card: the insert message
      if (!card && ['05458ef4', '08376202', '07653c55', '00000031', '0728fcb1', '000006ec', '05c5de78', '0e75f658', '069acf73', '00077698'].includes(n)) return { hidden: true };
      if (n === '05458ef4') return { text: format(this.t('kT_MEMMemDevicePS2', 'MEMORY CARD slot %s'), 1), alpha: 255 };
      if (n === '07653c55' || n === '00000031' || n === '069acf73' || n === '00077698') return { alpha: 255 };
      if (n === '0728fcb1') return { text: saved ? this.name() : this.t('kT_MEMEmpty', '< E M P T Y >'), alpha: 255 };
      return null;
    });
    if (this.phase === 'name' && fe.keyboard) fe.drawKeyboard(c, fe.now());
    c.restore();
    const p = this.popup(); if (p) this.drawPopup(c, p);
    return true;
  }
  popup() {
    const slot = 1;
    switch (this.phase) {
      case 'check': return { message: format(this.t('kT_MEMFECheckingCardPS2', ''), slot) };
      case 'saving': return { message: format(this.t('kT_MEMOVSaveSavingPS2NGC', ''), slot) };
      case 'ask': return { message: format(this.t('kT_MEMOVOverWriteFile', 'Would you like to overwrite %s?'), this.name()), options: [this.t('kT_CMNYes', 'Yes'), this.t('kT_CMNNo', 'No')], index: this.ask };
      case 'done': return { message: this.t('kT_MEMOVSaveDone', 'Save complete.'), options: [this.t('kT_CMNContinue', 'Continue')], index: 0 };
      case 'failed': return { message: format(this.t('kT_MEMOVSaveFailed', 'Save failed.'), slot), options: [this.t('kT_CMNContinue', 'Continue')], index: 0 };
    }
    return null;
  }
  popupLayout(p) { return fePopupLayout(this.ui.fonts?.FEFONT || {}, { message: p.message, options: p.options || [], type: 1, compact: true }); }
  // FE.LUI 'popup' as cFEPopup draws a type-1 box (web/fe-popup.js), its own LuiScreen with shapeScale; the veil fades from the first popup.
  drawPopup(c, p) {
    const fe = this.fe, screen = fe.data?.screens?.popup; if (!screen) return;
    if (!this.popupLui) { this.popupLui = new LuiScreen(screen, fe.images, this.ui); this.popupLui.shapeScale = true; }
    c.save(); c.scale(1, SY);
    drawFePopup(c, this.popupLui, this.ui.fonts?.FEFONT || {}, { message: p.message, options: p.options || [], index: p.index || 0, frame: this.now() - (this.popupAt ?? this.at) });
    c.restore();
  }
}
