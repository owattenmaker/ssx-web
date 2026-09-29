// The original Select Character front end (FE.LUI 08sel_char, cFEStateCharSelect vtable 0x46D000), its 3D rider
// preview and the Select Cheat Character list (131cheat_char, cFEStateCheatCharSelect). docs/characters.md.
//
// 2D: the screen's own elements and timeline (tools/export_character_select.py -> UI/character-select.json),
// played by web/lui-player.js: the intro (dashes slide in, stats fade in, frames 1..25), one state per rider
// at frame 40+5i (its name, orange silhouette and card text), the arrow flashes (hll/hlr, frames 125/130) and
// the always-on snowflakes (bg_snow_loop). Runtime fields as 0x181620/0x181BD0 fill them: names (0x14EEC8),
// the seven stats (raw attribute byte x 0.2, "%.1f", bar = raw/55) and the rider ranking ((sum - sum%5)/35).
// Sam is the eleventh entry the way the Sam PS2 build adds him (Mac's state with Sam's name/card/silhouette,
// the right arrow moved to x=556).
//
// 3D (0x181EF0 / 0x19EE88): camera eye (0,200,0) -> origin, Z up, 25 degree half-horizontal view; the rider at
// (136,-250,-82) cm turned 100 degrees about Z, board moved out of view. Clips from the front-end bank (fe.afl,
// library.json): the idle loop FE_GEAR_<PREFIX>_CYC (semantic 434) on Select and Setup, the cheer FE_CHARSEL_<NAME>
// (435, from t=0, holds its end, then a linear 0.23 s crossfade into the restarted idle) when Setup Character
// or Rider Details opens (same spot; FE_A_CYC 436 is Ubertrick Setup's). The right stick turns the rider
// 3 degrees per frame on Select Character only.
//
// Cheat characters are skins on the chosen base rider (setup slot +0x12): Rider Details > Cheat Characters lists
// the base rider's face, then every owned cheat character (unlock bits profile+char*0xF88+0xF57; career rewards in
// web/career.js, the original Enter Cheat codes, or ?unlockAll=1 / localStorage in the browser).
import { LuiScreen } from './lui-player.js';
import { FrontEndPreview, clipSample, channelValue, afb } from './fe-preview.js';
import { widescreenView } from './widescreen.js';
import { speakFrontEnd } from './rider-speech.js';
import { outfitPreviewEntry } from './wardrobe.js';
import { pv } from './pv-flags.js';

const ROOT = '/assets/UI/';
const FPS = 60;
const INTRO_END = 25;
const CROSSFADE = 0.23;                       // semantic 435 completion -> 434, linear blend (state table 0x446990)
const UNLOCK_KEY = 'ssx3.cheatCharacters';
const DEG = Math.PI / 180;
const BACK_LAYER = 9;

function loadImage(src) { const im = new Image(); im.src = src; return im.decode().then(() => im).catch(() => null); }
function storage() { try { return localStorage; } catch { return null; } }

// 0x181BD0 display: raw * 0.2 as "%.1f"; ranking = (sum - sum % 5) / 35.
export function statText(raw) { return (Math.round(raw * 2) / 10).toFixed(1); }
export function rankingText(raws) { let sum = raws.reduce((a, b) => a + b, 0); sum -= sum % 5; return (sum / 35).toFixed(1); }
export function rankingBar(raws) { let sum = raws.reduce((a, b) => a + b, 0); sum -= sum % 5; return Math.floor(sum / 7); }


export class CharacterSelect {
  constructor(ui) {
    this.ui = ui; this.data = null; this.images = {}; this.enter = 0; this.focus = 0; this.arrow = null; this.flash = null;
    this.yaw = 0; this.anim = null; this.lastScreen = null; this.cheatOpen = false; this.cheatIndex = 0; this.cheat = null;
  }
  get ready() { return !!this.data; }
  now() { return performance.now() * FPS / 1000; }

  async load() {
    try {
      const data = await (await fetch(ROOT + 'character-select.json')).json();
      const pages = [...new Set([...data.pages, 'FE_1-0', 'FE_1-1', 'FE_1-2', 'FE_1-3', 'FE_1-4'])];
      await Promise.all(pages.map(async (p) => { this.images[p] = await loadImage(ROOT + p + '.png'); }));
      if (data.sam_roster) this.images.sam = await loadImage(ROOT + data.sam_roster);
      const merge = (a, b) => ({ ...a, elements: [...a.elements, ...b.elements.map((e) => ({ ...e, index: e.index + 1000, snow: true }))], animations: { ...a.animations, ...b.animations } });
      this.select = new LuiScreen(merge(data.screens['08sel_char'], data.screens.bg_snow_loop), this.images, this.ui);
      this.snow = data.screens.bg_snow_loop.events;
      this.cheatScreen = new LuiScreen(data.screens['131cheat_char'], this.images, this.ui);
      this.flashScreen = data.screens.transition_flash;
      this.data = data;
      this.byLabel = new Map(data.screens['08sel_char'].elements.filter((e) => e.label).map((e) => [e.label, e]));
    } catch (error) { console.warn('Select Character assets unavailable (tools/export_character_select.py)', error); this.data = null; }
  }

  // ---- roster ----
  get entries() { return this.ui.riders.filter((r) => r.kind !== 'cheat').sort((a, b) => a.screen_index - b.screen_index); }
  get cheats() { return this.ui.riders.filter((r) => r.kind === 'cheat'); }
  get base() { return this.ui.rider; }
  owns(screen) { return this.ready && screen === 'character'; }
  // pv lodgeCheats: the lodge's Rider Details opens the same list (0x1F4064; PS2 local/ps2-capture/lodge/runs/l12)
  overlay(screen) { return this.ready && (screen === 'details' || (screen === 'ctm-details' && pv('lodgeCheats'))) && this.cheatOpen; }

  // Owned cheat characters of a base rider: career rewards (per rider, web/career.js), Enter Cheat codes and the
  // browser's unlock-all switch (the original has no all-characters code).
  unlocked(base) {
    if (typeof location !== 'undefined' && new URL(location.href).searchParams.get('unlockAll') === '1') return this.cheats;
    let codes = [];
    try { codes = JSON.parse(storage()?.getItem(UNLOCK_KEY) || '[]'); } catch {}
    const career = this.ui.careerUI?.career, owned = new Set();
    try { const items = career?.rewardItems('cheat_character') || []; for (const i of career?.owned(base.id, 'cheat_character') || []) if (items[i]) owned.add(items[i].character); } catch {}
    return this.cheats.filter((c) => codes.includes('all') || codes.includes(c.character) || owned.has(c.character));
  }
  // Options > Enter Cheat (0x187D38): a character code unlocks that character for every rider.
  enterCheat(text) {
    const value = String(text).toLowerCase().trim();
    const hit = this.cheats.find((c) => c.unlock?.code === value);
    if (!hit && value !== 'unlockall') return null;
    let codes = []; try { codes = JSON.parse(storage()?.getItem(UNLOCK_KEY) || '[]'); } catch {}
    codes.push(hit ? hit.character : 'all');
    try { storage()?.setItem(UNLOCK_KEY, JSON.stringify([...new Set(codes)])); } catch {}
    return hit || { name: 'All cheat characters' };
  }

  // A rider picked outside the screens (?rider=, ssxQA): a cheat entry rides on Zoe, like its own savestates.
  adopt(entry) {
    if (entry?.kind === 'cheat') { this.cheat = entry; this.ui.riderIndex = Math.max(0, this.ui.riders.findIndex((r) => r.id === (entry.base || 'zoe'))); }
    else if (entry) { this.cheat = null; this.ui.riderIndex = this.ui.riders.indexOf(entry); }
  }

  // The human the race loads: the base rider, or the cheat skin with the base rider's gameplay (web/character-roster.js).
  human() { return this.cheat ? { ...this.cheat, base: this.base.id, career: !!this.ui.careerMode } : this.base; }

  // ---- screen flow ----
  onEnter() { this.enter = this.now(); this.focus = this.enter; this.yaw = 0; this.flash = null; this.arrow = null; }
  items() { return [this.base?.name || '']; }
  layout() { return [270, 355, 180, 30]; }
  disabled() { return !this.ui.ready; }
  async cycle(direction) {
    if (!this.ui.ready || this.flash) return;
    const list = this.entries, at = Math.max(0, list.indexOf(this.base)), next = list[(at + direction + list.length) % list.length];
    this.arrow = { side: direction < 0 ? 'hll' : 'hlr', start: this.now() };
    try { await this.ui.cb.rider(next); this.ui.riderIndex = this.ui.riders.indexOf(next); this.cheat = null; this.focus = this.now(); this.anim = null; this.ui.sync(); }
    catch (error) { console.error(error); }
  }
  choose() {
    if (!this.ui.ready || this.flash) return;
    this.flash = this.now();   // Cross: 0x181844 -> transition_flash, then Setup Character (09set_char)
  }
  // The white flash is over (0x181844): Conquer the Mountain has no Setup Character. The FE controller's CharSelect case
  // (0x1A0A58, game type 0x535C11 = 0 -> 0x1A0B00) sets free ride (144DF0(4), 1451E8(12)), the start course (14 for a new
  // career 145C38, else the last lodge 146D98) and goes straight to the game load (cPreGameLoadScreen); PS2 ctm/caps
  // new-career: Cross -> Basic Controls load. Single Event and online (types 1 / 2) open Setup Character (0x1A0AD0 / 0x1A0B80).
  selectDone() {
    if (this.ui.careerMode && this.ui.careerUI?.ready) { this.ui.rememberRider?.(); this.ui.careerUI.enter(); }
    else this.ui.set('setup');
    speakFrontEnd(this.ui, 'select', this.human())?.catch?.(() => {});   // 1A0358 +0xC20: Post_Selection with the cheer
  }
  back() { this.ui.set('main'); }
  key(e) {
    const screen = this.ui.screen;
    if (this.overlay(screen)) {
      if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Enter', 'Space', 'Escape'].includes(e.code)) e.preventDefault();
      if (e.repeat) return true;
      const list = this.cheatList();
      if (e.code === 'ArrowLeft') this.cheatIndex = (this.cheatIndex + list.length - 1) % list.length;
      if (e.code === 'ArrowRight') this.cheatIndex = (this.cheatIndex + 1) % list.length;
      if (e.code === 'Enter' || e.code === 'Space') this.pickCheat(list[this.cheatIndex]);
      if (e.code === 'Escape') this.cheatOpen = false;
      this.ui.sync(); return true;
    }
    if (!this.owns(screen)) return false;
    if (['ArrowLeft', 'ArrowRight'].includes(e.code)) { e.preventDefault(); if (!e.repeat) this.cycle(e.code === 'ArrowLeft' ? -1 : 1); return true; }
    if (['KeyJ', 'KeyL'].includes(e.code)) this.stick = e.code === 'KeyJ' ? -1 : 1;   // right stick X (Classic IJKL)
    // Typing an original Enter Cheat code (Options > Enter Cheat, 0x187D38) on this screen unlocks its character.
    if (/^(Key[A-Z]|Digit[0-9])$/.test(e.code) && !e.repeat) {
      e.preventDefault();   // typing a code must not reach main.js (R restarts a run)
      this.typed = ((this.typed || '') + e.code.slice(-1).toLowerCase()).slice(-24);
      const codes = [...this.cheats.map((c) => c.unlock?.code).filter(Boolean), 'unlockall'];
      const code = codes.find((c) => this.typed.endsWith(c));
      if (code) { const hit = this.enterCheat(code); this.notice = { text: `${hit.name} unlocked`, until: this.now() + 150 }; this.typed = ''; }
      return true;
    }
    return false;
  }

  // Rider Details > Cheat Characters (enabled when the base rider owns at least one, 0x1577A0 > 0).
  cheatList() { return [this.base, ...this.unlocked(this.base)]; }
  canOpenCheats() { return this.unlocked(this.base).length > 0; }
  openCheats() { if (!this.canOpenCheats()) return; this.cheatOpen = true; this.cheatStart = this.now(); const list = this.cheatList(); this.cheatIndex = Math.max(0, list.indexOf(this.cheat || this.base)); this.ui.sync(); }
  async pickCheat(entry) {
    if (!entry) return;
    const cheat = entry.kind === 'cheat' ? entry : null;
    try { await this.ui.cb.rider(cheat ? { ...cheat, base: this.base.id, career: !!this.ui.careerMode } : this.base); this.cheat = cheat; this.anim = null; }
    catch (error) { console.error(error); }
    this.cheatOpen = false; this.ui.sync();
  }

  // ---- 2D ----
  stats(rider) {
    const career = this.ui.careerUI?.career, id = rider?.kind === 'cheat' ? this.base.id : rider?.id;
    const rows = career?.save?.riders?.[id]?.attributes;   // Buy Attributes rows, NumAttr order
    return Array.isArray(rows) && rows.length === 7 ? rows.slice() : (rider?.stats?.raw || [5, 5, 5, 5, 5, 5, 5]).slice();
  }

  draw(c, b) {
    const now = this.now(), frame = now - this.enter;
    if (this.flash && now - this.flash >= 10) { this.flash = null; this.selectDone(); return; }
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    b.save(); b.scale(1, 448 / 480); c.save(); c.scale(1, 448 / 480);
    const entries = this.entries, base = this.base, index = Math.max(0, entries.indexOf(base));
    const sam = base?.id === 'sam', label = sam ? 75 : 40 + 5 * Math.min(index, 9);
    const events = [];
    const sel = this.data.screens['08sel_char'];
    for (const ev of sel.events) {
      if (ev.frame <= Math.min(frame, INTRO_END)) events.push({ ev, start: this.enter - this.enter + ev.frame });
      else if (ev.frame === label) events.push({ ev, start: this.focus - this.enter });
      else if (this.arrow && ev.frame === (this.arrow.side === 'hll' ? 125 : 130)) events.push({ ev, start: this.arrow.start - this.enter });
    }
    const snowFrame = frame % 600;
    for (const ev of this.snow) if (ev.frame <= snowFrame) events.push({ ev, start: frame - snowFrame + ev.frame });
    const raws = this.stats(base), names = new Map(entries.map((r) => [r.screen_index, r]));
    const bar = (e) => this.byLabel.get(e.label);
    const macName = '00000037', macCard = '00069027', macSilhouette = '03757854';
    const override = (e) => {
      if (e.name === '0ee727a9') return { hidden: true };                          // 2-player title (0x181620)
      if (e.label?.startsWith('NumAttr') && e.label !== 'NumAttrOver') return { text: statText(raws[+e.label.slice(7)]) };
      if (e.label === 'NumAttrOver') return { text: rankingText(raws) };
      if (/^\dpb$/.test(e.label || '')) return { fill: Math.max(0, raws[+e.label[0]]) / 55 };
      if (e.label === 'overall') return { fill: rankingBar(raws) / 55 };
      if (sam && e.name === macName) return { text: base.name };
      if (/^[0-9]$/.test(e.label || '')) { const r = names.get(+e.label); if (r) return { text: r.name }; }
      if (this.notice && this.now() < this.notice.until && /^0006902/.test(e.name) && (sam ? e.name === macCard : true)) return { text: this.notice.text };
      if (sam && e.name === macCard) return { text: base.card.join(' ') };
      if (sam && e.name === macSilhouette) return { hidden: true };
      if (e.name === '06e5eed4' && names.has(10)) {                                 // right arrow (Sam build: x 512 -> 556)
        const p = this.select.props(e, events, frame); return { props: { 0: this.data.sam.right_arrow_x, 1: p[1] } };
      }
      return null;
    };
    // The PS2 draws the preview rider over the sky, mountains, snow, the white ramp, the frame and the big "3"
    // (layers 0..9) and under the menu (10+): the UI background canvas sits behind the 3D canvas.
    this.select.draw(b, events, frame, override, (layer) => layer <= BACK_LAYER);
    this.select.draw(c, events, frame, override, (layer) => layer > BACK_LAYER);
    // Sam, the eleventh figure of the row (docs/characters.md "Sam in the roster row"): his white silhouette belongs to
    // the white strip (layer 9, the ui-bg canvas, the strip's own alpha), his orange one to the highlights (layer 12,
    // shown by his rider state like the others').
    if (names.has(10) && this.images.sam) {
      const strip = this.select.byName.get('00a79fcc'), a = strip ? (this.select.props(strip, events, frame)[13] ?? 255) / 255 : 1;
      this.drawSam(b, 'white', a); if (sam) this.drawSam(c, 'orange', 1);
    }
    if (this.flash) { c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash) / 10)})`; c.fillRect(0, 0, 640, 480); }
    c.restore(); b.restore();
  }

  // The Sam build's silhouettes (group 0648bf23 offset (-4,12), 130%/110%): white always, orange when focused.
  drawSam(c, which, alpha) {
    const [x, y, w, h] = this.data.sam.silhouette, W = w * 1.3, H = h * 1.1, X = x - 4, Y = y + 12;   // group 0648bf23 (-4,12), 130%/110%
    const [u0, v0, u1, v1] = this.data.sam[which === 'white' ? 'white_uv' : 'orange_uv'];
    c.save(); c.globalAlpha = alpha; c.drawImage(this.images.sam, u0, v0, u1 - u0, v1 - v0, X, Y, W, H); c.restore();
  }

  // Select Cheat Character (131cheat_char) over Rider Details: six face slots, the focused one enlarged, its name.
  drawOverlay(c) {
    if (!this.overlay(this.ui.screen)) return;
    const frame = this.now() - this.cheatStart, list = this.cheatList();
    const screen = this.data.screens['131cheat_char'];
    const events = screen.events.filter((ev) => ev.frame <= Math.min(frame, 30)).map((ev) => ({ ev, start: ev.frame }));
    const first = Math.max(0, Math.min(this.cheatIndex - 2, list.length - 6));
    const slots = ['00076f61', '00076f62', '00076f63', '00076f64', '00076f65', '00076f66'];
    c.save(); c.scale(1, 448 / 480);
    this.cheatScreen.draw(c, events, frame, (e) => {
      const slot = slots.indexOf(e.name);
      if (slot >= 0) {
        const entry = list[first + slot]; if (!entry) return { hidden: true };
        const face = this.data.faces[entry.kind === 'cheat' ? entry.face : `${{ sam: 'mac' }[entry.id] || entry.id}face`];
        return { sprite: face, props: first + slot === this.cheatIndex ? { 9: 78, 10: 78 } : {} };
      }
      if (e.name === '083ca274') return { text: list[this.cheatIndex]?.name || '' };
      if ((e.name === '096efaf4' || e.name === '06e5eed4') && pv('lodgeCheats') && list.length <= slots.length) return { hidden: true };   // the arrows only when the faces scroll (PS2 lodge/runs/l12: Zoe + Brodi, none)
      return null;
    });
    c.restore();
  }

  // ---- 3D preview (called by web/main.js on the front-end rider screens) ----
  // The original FE preview model (web/fe-preview.js, tools/export_fe_preview.py): the NIS head, eyes and hands with
  // the FE clips' morph channels and the rider's own IRR.DAT lighting. Without its package the race rider stands in.
  get preview3d() { return this._preview ??= new FrontEndPreview(); }
  // What the preview shows: the base rider (the original keeps the base rider's preview for a cheat skin,
  // characters/<cheat>/setup.p2s), or the cheat skin's own FE package with ?feSkin=1.
  previewEntry() {
    const skin = this.cheat && typeof location !== 'undefined' && new URL(location.href).searchParams.get('feSkin') === '1';
    // a changed Equip Gear outfit (web/wardrobe.js): its own FE package (the NIS head/eyes/hands of that outfit); with a
    // cheat skin too the base rider in its outfit (brodi/gear/zoe-gear-dangerous-trouble-setup-after-equip.p2s)
    return skin ? this.cheat : outfitPreviewEntry(this.base);
  }
  // main.js, every frame: true when the FE preview replaces the race rider (drawn, or loading like the original,
  // which shows nothing until the model is loaded). Hides the preview everywhere else.
  showPreview(screen, playing) {
    const on = !playing && this.ready && ['character', 'setup', 'details'].includes(screen) && !!this.T && this.preview3d.want(this.T, this.previewEntry());
    const hidden = !playing && this.previewHidden(screen);
    this.preview3d.show(on && !hidden);
    // Warm the rest of the roster once the shown rider is up (neighbours first): prepared off the main thread and
    // compiled before use, so later switches are a swap (web/fe-preview.js cache).
    if (on && screen === 'character' && this.preview3d.ready && !this.prefetched) {
      this.prefetched = true;
      const list = this.entries, at = Math.max(0, list.indexOf(this.base));
      const order = list.map((r, i) => [r, Math.min((i - at + list.length) % list.length, (at - i + list.length) % list.length)]).sort((a, b) => a[1] - b[1]).map(([r]) => outfitPreviewEntry(r));
      // pv feCompileSpread: only while a rider screen is up (not on under the load screen); a stopped prefetch starts again on the next visit
      const onScreen = pv('feCompileSpread') ? () => ['character', 'setup', 'details'].includes(this.ui.screen) : null;
      this.preview3d.prefetch(this.T, order.slice(1), { active: onScreen }).then((done) => { if (done === false) this.prefetched = false; }).catch(() => {});
    }
    return on || hidden;                                        // hidden: neither the preview nor the race rider shows
  }
  // ---- preview hide window (web/fe-screens.js) ----
  // Screen changes hide the FE model (slot +0xCC8 = 0) for a while: ~30 frames on Setup Character <-> Rider Details,
  // ~26 after the Select Character flash; the cheer 435 is already set but its clock starts when the model shows.
  hidePreviewFor(frames) { this.previewHiddenUntil = performance.now() + frames * 1000 / FPS; this.previewHiddenScreen = this.ui.screen; }
  previewHidden(screen = this.ui.screen) { return !!this.previewHiddenUntil && performance.now() < this.previewHiddenUntil && screen === this.previewHiddenScreen; }

  // Returns true when it placed the rider and camera.
  place(T, model, camera) {
    if (!this.ready || !['character', 'setup', 'details'].includes(this.ui.screen)) return false;
    this.T = T; this.camera = camera;
    if (this.ui.screen === 'character' && this.stick) { this.yaw = (this.yaw + 3 * this.stick + 360) % 360; this.stick = 0; }
    // 25 degrees half-horizontal on the 4:3 frame: vertical tangent tan25 x 3/4 (square 640x480 pixels, the spec's
    // 686.24 px/unit); main.js's setViewOffset(640,448) leaves aspect 640/448, which stretched the preview by 7%.
    camera.clearViewOffset();
    camera.aspect = widescreenView(this.ui.widescreen, 0).cameraAspect;
    camera.fov = 2 * Math.atan(Math.tan(25 * DEG) * 0.75) / DEG;
    camera.updateProjectionMatrix();
    camera.up.set(0, 1, 0);
    camera.position.set(0, 0, -2); camera.lookAt(0, 0, 0);   // Rider Details shows the same spot (ARMSX2 frames)
    const yaw = this.ui.screen === 'character' ? this.yaw : 0;
    if (this.preview3d.want(T, this.previewEntry()) && model.parent && this.preview3d.place(T, model.parent, yaw)) this.preview3d.light(T, this.core);
    model.position.set(1.36, -0.82, 2.5);
    model.scale.setScalar(1 / (model.userData.riderScale || 1));   // the preview geometry is unscaled (geometry+0x140 = 1.0)
    model.quaternion.setFromAxisAngle(new T.Vector3(0, 1, 0), (100 + yaw) * DEG);
    return true;
  }

  // FE clips: idle 434 on Select Character, cheer 435 (then the idle) on entering Setup Character or Rider Details.
  clipsFor() {
    const base = this.base?.kind === 'custom' || !this.base?.fe ? this.ui.riders.find((r) => r.id === 'mac') : this.base;
    const fe = this.base?.fe || base?.fe || { idle: 'FE_GEAR_MAC_CYC', cheer: 'FE_CHARSEL_MAC' };
    return fe;
  }

  pose(T, bones, rig, dt, { clips, samples, scale, core }) {
    if (!this.ready || !['character', 'setup', 'details'].includes(this.ui.screen) || !clips || !samples) return false;
    this.T = T; if (core) this.core = core;
    const preview = this.preview3d.want(T, this.previewEntry());
    if (preview && !this.preview3d.ready) return true;                            // loading: the original draws nothing yet
    if (this.previewHidden()) { if (this.anim) this.anim.t = 0; return true; }       // hide window: the clock waits (hidePreviewFor)
    const target = preview ? this.preview3d.model : rig;
    const screen = this.ui.screen, fe = this.clipsFor();
    if (screen !== this.lastScreen || !this.anim || this.anim.rig !== target) {   // a new model restarts its animator (0x311A50)
      // Setup Character (0x183178) and Rider Details both reset the animator and play the cheer 435 whenever they are
      // entered (also from each other; ARMSX2 snaps, docs/characters.md); Select Character plays the idle 434.
      // FE_A_CYC (436, 0x184C60) belongs to Ubertrick Setup (66ut_btnmap), not Rider Details.
      const cheer = screen === 'setup' || screen === 'details';
      this.anim = { rig: target, clip: cheer ? fe.cheer : fe.idle, t: 0, fade: null };
      this.lastScreen = screen;
    }
    const a = this.anim;
    a.t += dt;
    const find = (name) => clips.find((c) => c.name === name);
    let main = find(a.clip);
    if (!main) return false;
    const len = (clip) => (clip.frame_count - 1) / clip.fps;
    if (a.clip === fe.cheer && a.t >= len(main) && !a.fade) a.fade = { t: 0 };        // cheer done: crossfade to the idle
    let weight = 0, idle = null, idleT = 0;
    if (a.fade) { a.fade.t += dt; idle = find(fe.idle); idleT = a.fade.t; weight = Math.min(1, a.fade.t / CROSSFADE); if (weight >= 1) { a.clip = fe.idle; a.t = a.fade.t; a.fade = null; main = idle; weight = 0; idle = null; } }
    const A = clipSample(main, a.t, a.clip !== fe.cheer), B = idle ? clipSample(idle, idleT, true) : null;
    if (preview) return this.preview3d.apply(T, A, B, weight, samples);
    const value = (s, stream, x) => channelValue(samples, s, stream, x);
    const v = new T.Vector3(), q = new T.Quaternion();
    rig.bones.forEach((b, k) => {
      const bone = bones[k]; if (!bone) return;
      const rest = () => { bone.position.fromArray(b.translation).multiplyScalar(scale); bone.quaternion.fromArray(b.rotation).normalize(); };
      const local = (s) => {
        const stream = s.clip.streams?.[String(b.file)]; if (!stream) return null;
        const tc = b.animation_translation_channel, rc = b.animation_rotation_channel;
        return { p: tc >= 0 ? new T.Vector3(value(s, stream, tc) / 100, value(s, stream, tc + 2) / 100, -value(s, stream, tc + 1) / 100).multiplyScalar(scale) : null,
                 r: rc >= 0 ? afb(T, value(s, stream, rc), value(s, stream, rc + 1), value(s, stream, rc + 2)) : null };
      };
      rest();
      const pa = local(A), pb = B ? local(B) : null;
      if (pa?.p) bone.position.copy(pa.p); if (pa?.r) bone.quaternion.copy(pa.r);
      if (pb) { if (pb.p) bone.position.lerp(v.copy(pb.p), weight); if (pb.r) bone.quaternion.slerp(q.copy(pb.r), weight); }
      if (b.name === 'board_rootg') bone.position.set(0, 100, 0);                  // 0x19F548: the board is moved past the far plane
    });
    return true;
  }
}
