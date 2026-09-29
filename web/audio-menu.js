// The original audio menus (docs/audio-menus.md), played from the disc's own LUI layouts
// (tools/export_audio_menus.py -> /assets/UI/audio-menus.json) by web/lui-player.js:
//
//   fe-music        FE.LUI 140audio        Music: Radio BIG / BIG Mountain Ambience / Custom Playlist [DJ] / [No DJ] /
//                                          Edit Playlist (cFEStateAudioOptions 0x195FF0; Setup Character and the lodge)
//   fe-playlist     FE.LUI 16radio         Edit Playlist, the "Request Line" (cFEStateRequestLine 0x196B90)
//   fe-sound        FE.LUI 141advsettings  Sound Options (cFEStateOptionsSound 0x18A258): sound mode, Music/MC, SFX and
//                                          Character speech sliders (12 steps), DJ Speech, Arcade SFX
//   audio           OV.LUI 142audio_pda    in-game Audio (pause "Audio": the same 0x195FF0 state while in game)
//   audio-playlist  OV.LUI 143radio_pda    in-game Edit Playlist (the same 0x196B90 state while in game)
//   pda-options     OV.LUI 37beoptions     in-game Options (pause / MCOMM "Options", pv pdaOptions): HUD Options, Camera 1, Camera 2 (greyed:
//                                          one player), Music/MC / SFX / Character speech sliders, DJ Speech, Arcade SFX, Save game
//                                          (Conquer the Mountain only), then the port's "More options" in the online-only EA Talk row
//   pda-more        OV.LUI 37beoptions     the port's Widescreen / Keyboard / Display & Touch on the same page's first rows
//
// Menu rows -> radio mode (0x196540 widget "+0x18" values, SetRadioMode 0x28BF78): radio big 0, big mountain 2,
// request line 1, playlist 3; Edit Playlist is value 4. Checkbox kbox marks row k = mode MENU_MODES[k].
// Every setting goes through the audio owner's API (web/game-audio.js): getSettings / setSettings / songs /
// previewSong / endPreview / ui(ev) (0x294F78 events; FE 0..4, in game 9..13).
import { LuiScreen } from './lui-player.js';
import { menuModel, stepMenu, firstEnabled } from './fe-screens.js';
import { NowPlayingHud } from './now-playing.js';
import { pv } from './pv-flags.js';
import { WIDESCREEN_MODES } from './widescreen.js';
import { DISPLAY_ENTRY } from './fe-options.js';

const ROOT = '/assets/UI/';
const FPS = 60, FLASH = 10, SY = 448 / 480, DISABLED_ALPHA = 128, ROWS = 8;
export const MENU_MODES = Object.freeze([0, 2, 1, 3]);               // 140audio / 142audio_pda row -> radio mode
export const SONG_FREE_CREDITS = 6, SONG_PRICE = 5000;               // 0x1988D8 (6 credits), 0x198AE8 ($5,000)
export const SLIDER_STEPS = 12;                                      // 0x18B4B8: slider +0x78 = 12 (values 0..11)
export const SPEAKER_MODES = ['kT_20SpeakerStereo', 'kT_20SpeakerSurround', 'kT_20SpeakerMono', 'kT_20SpeakerDTS'];   // 0x18B3E8
// 0x294F78 events (table 0x482D60): FE listener "FEUISound" 0x1A2E58 uses 0..4, the in-game one 0x1A2F70 uses 9..13.
export const UI_EV = Object.freeze({ accept: 0, back: 1, move: 2, scroll: 3, error: 4 });
export function uiEvent(name, ingame) { return UI_EV[name] + (ingame ? 9 : 0); }

const SCREENS = {
  'fe-music': { key: '140audio', pack: 'FE' }, 'fe-playlist': { key: '16radio', pack: 'FE' }, 'fe-sound': { key: '141advsettings', pack: 'FE' },
  audio: { key: '142audio_pda', pack: 'OV', ingame: true }, 'audio-playlist': { key: '143radio_pda', pack: 'OV', ingame: true },
  'pda-options': { key: '37beoptions', pack: 'OV', ingame: true, pda: true }, 'pda-more': { key: '37beoptions', pack: 'OV', ingame: true, pda: true },
};
export const AUDIO_SCREENS = Object.keys(SCREENS);
const BOXES = ['00036968', '00037968', '00038968', '00039968', '0003a968', '0003b968', '0003c968', '0003d968'];   // 0box..7box
const SONG_ROWS = ['07a64e60', '07a64e61', '07a64e62', '07a64e63', '07a64e64', '07a64e65', '07a64e66', '07a64e67'];   // songs0..7
const FREE = ['0036d8b5', '0037d8b5', '0038d8b5', '0039d8b5', '003ad8b5', '003bd8b5'];   // 0free..5free
const N = { helptext: '0c37a134', arrUp: '068986c0', arrDown: '0985bbde', numSongs: '036acf63', songList: '0497c464', singer: '07a04dc2',
  songBy: '0a649679', cost: '0006a6a4', costAnswer: '05d17882', youHave: '0676f8c5', youHaveAnswer: '0bdab522', freeSongs: '07781583',
  select: '0bb1b4e4', preview: '04d53aa7', ps2x: '00077698', ps2sq: '007769a1', current: '0ccc3947', artist: '0d1aa924',
  ongroup: '064e9620', fegroup: '0cbe9600', lbArrows: '046fd665' };
// Sound Options rows (menu children '1'..'7'): the widget each drives and its help text element.
const SOUND_ROWS = [
  { value: '00003822', kind: 'lb', help: '0417b31b' }, { slider: '0000399c', key: 'music', help: '04e5629f' },
  { slider: '00003a9c', key: 'effects', help: '0c34d418' }, { slider: '00003b9c', key: 'speech', help: '0eeedbd8' },
  { value: '00003c22', kind: 'lb', key: 'dj', help: '0fcfb5b8' }, { value: '00003d22', kind: 'lb', key: 'arcadeAudio', help: '010bd3f8' },
  { value: '00003e22', kind: 'lb', hidden: true, help: '03546bc5' }];   // 7lb EA SPORTS BIG Talk: online only (0x18B308, gp 0x4a1a70)

// 37beoptions (PS2 menus/single/r3-options, menus/ctm/47-options): the menu's rows, their focus frames (label white, the arrows group
// 068996e3 on the row) and help lines; Save game is a plain text (focus 115: the arrows go, the Cross 00077698 shows in group 00000039);
// the EA SPORTS BIG Talk row (online only, focus 120) carries the port's "More options".
export const PDA_ROWS = Object.freeze([
  { row: '00000030', label: '06a1513c', value: '00003722', help: '0326ec30', frame: 70, kind: 'hud', y: 100 },
  { row: '00000031', label: '049fba64', value: '00003822', help: '0336ec30', frame: 75, kind: 'camera', y: 125 },
  { row: '00000032', label: '000deb24', value: '00003922', help: '0336ec30', frame: 80, kind: 'camera2', y: 150, disabled: true },
  { row: '00000034', label: '07b057f7', slider: '00003b9c', help: '0366ec30', frame: 90, key: 'music', y: 175 },
  { row: '00000035', label: '000079d8', slider: '00003c9c', help: '0376ec30', frame: 95, key: 'effects', y: 200 },
  { row: '00000036', label: '08877902', slider: '00003d9c', help: '0386ec30', frame: 100, key: 'speech', y: 225 },
  { row: '00000037', label: '000006aa', value: '00003e22', help: '0396ec30', frame: 105, key: 'dj', y: 250 },
  { row: '00000038', label: '00006883', value: '00003f22', help: '03a6ec30', frame: 110, key: 'arcadeAudio', y: 275 },
  { row: '000798c5', label: '000798c5', help: '03b6ec30', frame: 115, kind: 'save', action: true, y: 300 },
  { row: '08394d42', label: '0496a82b', slider: '06a83bbc', help: '0426ec50', frame: 120, kind: 'more', action: true, y: 325, text: 'More options',
    helpText: 'Widescreen, keyboard and display settings.' },
]);
// pda-more: the page's first three rows relabelled; Display & Touch opens the port's page (web/fe-options.js), back here.
export const PDA_MORE = Object.freeze([
  { ...PDA_ROWS[0], kind: 'widescreen', text: 'Widescreen', helpText: 'Select widescreen support for anamorphic and 16:9 displays.' },
  { ...PDA_ROWS[1], kind: 'keyboard', text: 'Keyboard' },
  { ...PDA_ROWS[2], kind: 'display', action: true, disabled: false, value: null, text: DISPLAY_ENTRY.text, helpText: DISPLAY_ENTRY.help },
]);
const PDA = { arrows: '068996e3', arrowL: '0883c100', arrowR: '08386d40', valueBar: '0b6ea470', cross: '00077698', crossGroup: '066fa3be', crossRow: '00000039',
  intro: 50 };
export const HUD_LEVELS = [0x0cdfe1ac, 0x03dd98bc, 0x0cde53c5];                         // Full / Minimal / None (21hud_opt)
export const CAMERA_WORDS = { 0x3C: 0x096bdc82, 0x3D: 0x0896ba94, 0x3E: 0x0896a5e2 };  // Near / Mid / Far
export function pdaRows(id) { return id === 'pda-more' ? PDA_MORE : PDA_ROWS; }

// ---- pure rules (web/test-audio-menu.mjs) ----

const count = (bits) => bits.reduce((n, b) => n + (b ? 1 : 0), 0);
// 0x1587B8 > 0: a custom playlist exists. The playlist mask only ever holds owned songs (0x197500 toggles owned rows).
export function playlistBits(settings, owned) { return owned.map((o, i) => !!(o && settings?.playlist?.[i])); }
// 0x196B08 (query 6): rows 2/3 need a playlist; Edit Playlist needs the career (CTM) flag or a playlist.
export function musicDisabled({ has, ctm }) { return [false, false, !has, !has, !ctm && !has]; }
// 0x196960 help table by the row's value (radio mode or 4 = edit).
export function musicHelp(row, { has, ctm, ingame }) {
  const create = ingame ? 'kT_HELPAUDIOCreateInLodge' : ctm ? 'kT_HELPAUDIOCreate' : 'kT_HELPAUDIOCreateInCTM';
  switch (row) {
    case 0: return 'kT_HELPAUDIORadioBig';
    case 1: return 'kT_HELPAUDIOBMA';
    case 2: return has ? 'kT_FEHELPCustPlayDJ' : create;
    case 3: return has ? 'kT_FEHELPCustPlayNoDJ' : create;
    default: return ctm || has ? 'kT_HELPAUDIOEditReq' : ingame ? 'kT_HELPAUDIOCreateInLodge' : 'kT_HELPAUDIOCreateInCTM';
  }
}
// 0x197AD8: the Request Line lists owned songs first, then the rest, each in song order.
export function songOrder(owned) { const idx = owned.map((_, i) => i); return [...idx.filter((i) => owned[i]), ...idx.filter((i) => !owned[i])]; }
// 0x1977D0 query 6 (Cross allowed?) for a song. The last song in the playlist cannot be removed.
export function songState(song, { owned, playlist, ctm, cash = 0 }) {
  const mine = !!owned[song], listed = !!playlist[song], last = listed && count(playlist) === 1;
  const credits = SONG_FREE_CREDITS - count(owned), affordable = credits > 0 || cash >= SONG_PRICE;
  return { mine, listed, last, credits, affordable, selectable: mine ? !last : ctm && affordable, buy: !mine && ctm && affordable };
}
// 0x1981F8 help / 0x198340 Cross label for the focused song.
export function songHelp(st, { ctm }) {
  if (st.mine) return st.last ? 'kT_HELPAudio1SongMin' : 'kT_HELPAUDIOAddRem';
  if (ctm) return st.affordable ? 'kT_HELPAUDIOAvailBuy' : 'kT_16SaveMoreCarsh';
  return 'kT_FAQRADIOBA';            // (0x535C11 == 0; kT_CMNHELPBuySongCTM otherwise)
}
export function songButton(st) { if (st.mine) return st.last ? null : st.listed ? 'kT_BTNRemSong' : 'kT_BTNAddSong'; return st.buy ? 'kT_16BuyMusicTracks' : null; }
// 0x197500 event 5 on an owned song: toggle, except that the playlist never becomes empty.
export function toggleSong(playlist, song) {
  const next = playlist.slice(); if (next[song] && count(next) === 1) return next; next[song] = !next[song]; return next;
}
// 0x39E130: knob x = track.x + value * (trackW - knobW) / (steps - 1).
export function knobX(value, trackW, knobW, steps = SLIDER_STEPS) { return Math.max(0, Math.min(steps - 1, value)) * (trackW - knobW) / (steps - 1); }
// 8-row list window (the LUI menu's +0x98 top): keep the focus visible.
export function scrollTop(top, sel, total, rows = ROWS) { if (sel < top) top = sel; if (sel >= top + rows) top = sel - rows + 1; return Math.max(0, Math.min(top, Math.max(0, total - rows))); }
function fmt(template, value) { return String(template || '').replace(/%[dS]/, String(value)); }

function loadImage(src) { return new Promise((resolve, reject) => { const im = new Image(); im.onload = () => resolve(im); im.onerror = reject; im.src = src; }); }
// pv riderMusic: a rider's radio mode and custom playlist live in its profile record (R+0xF80 / R+0xF78 of 0x4A6CA8 + profile *
// 0x9B50 + char * 0xF88): the Music screen writes the mode (0x196770 -> 1587F8), Triangle on the Request Line the masks (0x19768C
// -> 158848 / 158820), and every world load applies the human's (2867E8: 158700 -> SetRadioMode, 158750 -> 28C2D0). PS2
// local/ps2-capture/lodge/runs: l6 buys two songs (owned 0x3, playlist 0x3 on exit), l7 picks Custom Playlist [DJ] (R+0xF80 = 1), l10
// rides out of the station with audio+0x608C = 1 and the custom mask audio+0x6098 = 0x3. A record from before keeps the global
// mode it was played with; a custom mode needs a playlist (0x196B08), else Radio BIG.
export function riderMusic(record, global = {}) {
  const playlist = Array.from({ length: 35 }, (_, i) => !!record?.playlist?.includes(i));
  let radioMode = record?.radioMode ?? global.radioMode ?? 0;
  if ((radioMode === 1 || radioMode === 3) && !playlist.some(Boolean)) radioMode = 0;
  return { radioMode, playlist };
}
function unlockAll() { try { return new URL(location.href).searchParams.has('unlockAll'); } catch { return false; } }

export class AudioMenus {
  constructor(ui) {
    this.ui = ui; this.data = null; this.images = {}; this.lui = {}; this.models = {}; this.enterAt = {}; this.focusAt = 0;
    this.flash = null; this.ctx = {}; this.pendingIndex = null; this.fallback = null; this.previewed = false; this.top = 0; this.list = [];
    try { if (new URL(location.href).searchParams.has('perf')) globalThis.__audioMenus = this; } catch {}   // QA: ?perf=1
    this.soundMode = 0; try { this.soundMode = +(localStorage.getItem('ssx3.soundMode') || 0) & 3; } catch {}
  }
  get ready() { return !!this.data; }
  get audio() { const a = this.ui.gameAudio || null; if (a && !a.riderMusic) a.riderMusic = () => this.worldMusic(); return a; }
  // the human's radio mode / playlist for a world load (web/game-audio.js worldLoaded), null without pv riderMusic or a record
  worldMusic() { const r = this.riderRecord(); return r ? riderMusic(r, this.ui.gameAudio?.getSettings?.() || {}) : null; }
  riderRecord() { const c = this.career; if (!pv('riderMusic') || !c?.songState || unlockAll()) return null; try { return c.songState(this.riderId); } catch { return null; } }
  now() { return performance.now() * FPS / 1000; }
  async load() {
    try {
      const data = await (await fetch(ROOT + 'audio-menus.json')).json();
      await Promise.all(data.pages.map(async (p) => { this.images[p] = await loadImage(ROOT + p + '.png'); }));
      const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop;
      for (const [id, s] of Object.entries(SCREENS)) {
        let screen = data.screens[s.key]; if (!screen) continue;
        if (s.pack === 'FE' && snow) screen = { ...screen, elements: [...screen.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))], animations: { ...screen.animations, ...snow.animations } };
        this.lui[id] = new LuiScreen(screen, this.images, this.ui);
        this.models[id] = s.pda ? { items: pdaRows(id).map((r) => r.row), frames: pdaRows(id).map((r) => r.frame), texts: pdaRows(id).map((r) => r.text || (screen.elements.find((e) => e.name === r.label)?.text || '').replace('%d', r.kind === 'camera2' ? '2' : '1')), intro: PDA.intro }
          : id === 'fe-sound' ? this.rowModel(screen) : menuModel(screen);
      }
      Object.assign(this.images, Object.fromEntries(Object.entries(this.ui.characterSelect?.images || {}).filter(([k]) => !this.images[k])));
      this.data = data;
      // FE Options "Sound" (web/fe-screens.js enables it when this hook exists): Sound Options, back to Options.
      this.ui.cb.audioOptions = () => { const fe = this.ui.feScreens; if (fe?.go) fe.go('fe-sound'); else this.ui.set('fe-sound'); };
    } catch (error) { console.warn('Audio menus unavailable', error); this.data = null; }
    this.nowPlaying = new NowPlayingHud(this.ui);
    this.subscribe();
    this.watchSounds();
  }
  // "EA RADIO BIG" popup (web/now-playing.js; 0x28F478 -> HUD command 5) when a playlist song starts in game.
  subscribe() {
    if (this.unsubscribe || !this.audio?.onNowPlaying) return;
    this.unsubscribe = this.audio.onNowPlaying((song) => { if (this.audio?.raceMusic === false) return; this.nowPlaying?.show(song); });
  }
  // From ui.js on the 'game' screen: advance the 60 Hz popup (not while paused: the HUD update is skipped) and draw
  // it after the rest of the HUD (a microtask, so the other HUD returns in ui.draw do not skip it).
  drawHud(c) {
    const np = this.nowPlaying; if (!np) return; this.subscribe();
    const now = performance.now(), dt = this.hudAt ? (now - this.hudAt) / 1000 : 0; this.hudAt = now;
    np.update(dt > 0.25 ? 1 / 60 : dt);
    if (np.visible) queueMicrotask(() => np.draw(c));
  }
  // 141advsettings: the menu's rows (kind 0x15 groups); focus frame = the frame that turns the row's label white.
  rowModel(screen) {
    const by = new Map(screen.elements.map((e) => [e.name, e])), menu = screen.elements.find((e) => e.kind === 'menu');
    const rows = menu.children.filter((n) => by.get(n)?.widget === 'row');
    const frames = rows.map((n) => { const label = by.get(n).children[0]; return screen.events.find((ev) => ev.element === label && ev.props?.[14] === 255 && ev.props?.[13] !== 0)?.frame; });
    return { menu: menu.name, items: rows, frames, texts: rows.map((n) => by.get(by.get(n).children[0])?.text || ''), intro: Math.max(0, ...screen.events.filter((ev) => ev.frame < Math.min(...frames.filter((f) => f != null))).map((ev) => ev.frame)) };
  }
  owns(screen) { return this.ready && !!SCREENS[screen] && !!this.lui[screen]; }
  // Another screen of audio-menus.json drawn by its owner (38session: web/career-ui.js drawSessionMap), made once on these images.
  screenLui(key) { const s = this.data?.screens?.[key]; if (!s) return null; const m = (this.extraLui ??= {}); return (m[key] ??= new LuiScreen(s, this.images, this.ui)); }
  t(key) { return this.data?.strings?.[key] ?? ''; }
  ingame(screen = this.ui.screen) { return !!SCREENS[screen]?.ingame || (screen === 'fe-sound' && !!this.ctx.sound?.ingame); }

  // ---- settings (web/game-audio.js; a local copy when the API is missing) ----
  settings() {
    const s = this.audio?.getSettings?.() || (this.fallback ??= { music: 10, effects: 10, speech: 10, radioMode: 0, dj: true, arcadeAudio: true, playlist: Array(35).fill(true) });
    const r = this.riderRecord(); return r ? { ...s, ...riderMusic(r, s) } : s;
  }
  set(partial) {
    const r = this.riderRecord();
    if (r && (partial.radioMode !== undefined || partial.playlist)) {   // pv riderMusic: the rider's record (1587F8 / 158848)
      if (partial.radioMode !== undefined) r.radioMode = partial.radioMode;
      if (partial.playlist) r.playlist = partial.playlist.flatMap((b, i) => (b ? [i] : []));
      try { this.career.persist?.(); } catch {}
    }
    if (this.audio?.setSettings) this.audio.setSettings(partial); else Object.assign(this.fallback ??= this.settings(), partial);
  }
  sfx(name, screen = this.ui.screen) { try { this.audio?.ui?.(uiEvent(name, this.ingame(screen))); } catch {} }
  songs() {
    const list = this.audio?.songs?.(); if (list?.length) return list;
    return (this.ui.careerUI?.career?.songs?.() || []).map((s, index) => ({ index, title: s.title, artist: s.artist, album: s.album }));
  }
  get career() { return this.ui.careerUI?.career || null; }
  get riderId() { return this.ui.rider?.id || 'sam'; }
  // 0x160 owned mask: the rider's bought songs (career save, 6 free credits then $5,000); ?unlockAll=1 or no career
  // data (the lodge tables are optional) owns everything, like the song cheat 0x187D38.
  owned() {
    const n = this.songs().length || 35, c = this.career;
    if (unlockAll() || !c?.songState) return Array(n).fill(true);
    let mine = []; try { mine = c.songState(this.riderId).owned || []; } catch { return Array(n).fill(true); }
    return Array.from({ length: n }, (_, i) => mine.includes(i));
  }
  cash() { try { return this.career?.rider(this.riderId)?.cash ?? 0; } catch { return 0; } }
  hasPlaylist() { return count(playlistBits(this.settings(), this.owned())) > 0; }

  // ---- navigation ----
  // ctx: {ctm, back()} for fe-music (the lodge passes ctm: true); in game {back()} returns to the pause menu.
  open(screen, ctx = {}) {
    if (screen === 'fe-music' || screen === 'audio') this.ctx = { ...ctx };
    if (screen === 'fe-sound') this.ctx.sound = { ...ctx };
    this.ui.set(screen);
  }
  enter(screen, from) {
    // Pause opened from the race (0x2306A8 / 0x244880 -> 0x294F48: bank-0 sound 0); results / pre-race / top-times
    // panels appearing play ev 14 (0x1E6594, 0x1E78F4, 0x1E805C, 0x1F7E30, 0x1FBB90 ...).
    if (from === 'game' && (screen === 'pause' || screen === 'ctm-pause')) { try { this.audio?.uiSound?.(0); } catch {} }
    if (PANELS.has(screen) && from !== screen) { try { this.audio?.ui?.(14); } catch {} }
    if (!this.owns(screen)) return;
    const now = this.now();
    if (from !== screen) this.enterAt[screen] = this.ui.careerUI?.lodgeFlash?.introStart?.(now) ?? now;   // pv introLead: opened by the lodge's state change (web/lui-flash.js)
    this.focusAt = now;
    // Setup Character -> Music (web/fe-screens.js go('fe-music')): the FE state, CTM flag from the career flow (0x1A181C).
    if (screen === 'fe-music' && from === 'setup') this.ctx = { ctm: !!this.ui.careerMode, back: () => this.ui.set('setup') };
    if (screen === 'fe-sound' && from === 'fe-options') this.ctx.sound = { back: () => this.ui.set('fe-options') };
    if (screen === 'fe-playlist' || screen === 'audio-playlist') {
      if (from !== screen) {
        const owned = this.owned(); this.list = songOrder(owned).slice(0, this.songs().length || owned.length);
        this.playlist = playlistBits(this.settings(), owned); this.previewed = false; this.top = 0; this.pendingIndex = 0;
      }
    }
    if (screen === 'fe-music' || screen === 'audio') { const m = MENU_MODES.indexOf(this.settings().radioMode ?? 0); if (from !== 'fe-playlist' && from !== 'audio-playlist') this.pendingIndex = Math.max(0, m); }
    if (screen === 'fe-sound' && from !== screen) this.pendingIndex = 0;
    if (screen === 'pda-options' && from !== screen) this.pendingIndex = from === 'pda-more' ? PDA_ROWS.length - 1 : 0;
    if (screen === 'pda-more' && from !== screen) this.pendingIndex = from === 'fe-display' ? 2 : 0;
  }
  focus() {
    const s = this.ui.screen, dis = this.disabledList(s);
    if (this.pendingIndex != null) { this.ui.index = this.pendingIndex; this.pendingIndex = null; }
    if (this.ui.index >= dis.length) this.ui.index = 0;
    if (dis[this.ui.index]) this.ui.index = firstEnabled(dis, this.ui.index);
    return this.ui.index;
  }
  disabledList(screen = this.ui.screen) {
    if (screen === 'fe-music' || screen === 'audio') return musicDisabled({ has: this.hasPlaylist(), ctm: !!this.ctx.ctm && screen === 'fe-music' });
    if (screen === 'fe-sound') return SOUND_ROWS.map((r) => !!r.hidden);
    if (SCREENS[screen]?.pda) return pdaRows(screen).map((r) => !!r.disabled || (r.kind === 'save' && !this.canSave()));
    if (screen === 'fe-playlist' || screen === 'audio-playlist') return this.list.map(() => false);
    return [];
  }
  items() {
    const s = this.ui.screen;
    if (s === 'fe-playlist' || s === 'audio-playlist') { const songs = this.songs(); return this.list.map((i) => songs[i]?.title || ''); }
    return this.models[s]?.texts || [];
  }
  disabled(i) { return !this.ui.ready || !!this.disabledList()[i]; }
  layout(i) {
    const s = this.ui.screen, lui = this.lui[s], model = this.models[s];
    if (s === 'fe-playlist' || s === 'audio-playlist') { const r = i - this.top; if (r < 0 || r >= ROWS) return [0, -100, 1, 1]; return this.rowRect(SONG_ROWS[r]); }
    const name = model?.items[i]; if (!name || !lui) return [0, -100, 1, 1];
    if (SCREENS[s]?.pda) return this.rowRect(name);
    if (s === 'fe-sound') { const row = lui.byName.get(name), menu = lui.byName.get(model.menu), p = row.props || {}, mp = menu.props || {}; return [(mp[0] || 0) + (p[0] || 0) + 150, ((mp[1] || 0) + (p[1] || 0)) * SY, 400, 20 * SY]; }
    return this.rowRect(name);
  }
  rowRect(name) {
    const lui = this.lui[this.ui.screen]; const e = lui?.byName.get(name); if (!e) return [0, -100, 1, 1];
    let x = 0, y = 0; for (let n = e; n; n = lui.byName.get(n.parent || n.menu)) { const p = n.props || {}; x += p[0] || 0; y += p[1] || 0; if (!(n.parent || n.menu)) break; }
    const p = e.props || {}, w = p[6] || 250, h = p[7] || 20;
    return [x - ((p[12] ?? 9) & 32 ? w : 0), y * SY, w, h * SY];
  }

  go(fn) { if (this.ingame()) { fn(); return; } this.flash = { at: this.now(), to: fn }; }   // FE: whitefade (10 frames) first
  choose(i) {
    const ui = this.ui, s = ui.screen; if (!ui.ready || this.flash) return;
    if (i !== ui.index) { ui.index = i; this.onFocus(); }
    if (s === 'fe-music' || s === 'audio') return this.chooseMusic(i);
    if (s === 'fe-playlist' || s === 'audio-playlist') return this.chooseSong(i);
    if (SCREENS[s]?.pda) return this.choosePda(i);
    // fe-sound: Cross does nothing and is silent (query 0x18A890 returns 0 for Cross)
  }
  chooseMusic(i) {
    const s = this.ui.screen;
    if (this.disabled(i)) { this.sfx('error'); return; }
    this.sfx('accept');
    if (i < 4) {                                                       // 0x196770: move the check, save, SetRadioMode
      const radioMode = MENU_MODES[i], partial = { radioMode };
      if (radioMode === 1 || radioMode === 3) partial.playlist = playlistBits(this.settings(), this.owned());
      this.set(partial); return;
    }
    const to = s === 'audio' ? 'audio-playlist' : 'fe-playlist';       // Edit Playlist: cFEStateRequestLine
    this.go(() => this.ui.set(to));
  }
  songAt(i = this.ui.index) { return this.list[i]; }
  songInfo(song = this.songAt()) {
    return songState(song, { owned: this.owned(), playlist: this.playlist || [], ctm: !!this.ctx.ctm && this.ui.screen === 'fe-playlist', cash: this.cash() });
  }
  chooseSong(i) {
    const song = this.songAt(i); if (song == null) return;
    const st = this.songInfo(song);
    if (!st.selectable) { this.sfx('error'); return; }
    this.sfx('accept');
    if (st.mine) { this.playlist = toggleSong(this.playlist, song); return; }
    this.confirm = { song, credit: st.credits > 0, index: 1 };          // cUIStateBuyPopup (0x198988), defaults to No
  }
  buy() {                                                               // 0x1988D8 -> 0x158558: own it, add it to the playlist
    const c = this.career, { song } = this.confirm; this.confirm = null;
    try { c?.buySong?.(this.riderId, song); } catch (error) { console.warn(error); }
    const owned = this.owned(); if (!owned[song]) return;
    this.playlist = this.playlist.slice(); this.playlist[song] = true;
  }
  // Square: 0x1977D0 query 8. Front end: the song's PREVIEW section on charsel (0x2B4978); in game (owned songs only):
  // audio+0x508 = song, PlayMusic(0, 0, -1, 1) and the Now Playing labels (0x197E70).
  preview() {
    const song = this.songAt(); if (song == null) return;
    if (this.ui.screen === 'audio-playlist' && !this.owned()[song]) { this.sfx('error'); return; }
    this.sfx('accept'); this.previewed = true;
    try { this.audio?.previewSong?.(song); } catch (error) { console.warn(error); }
  }
  // Triangle (event 6): 0x19768C saves both masks (0x158848 / 0x158820), 0x28C2D0 custom playlist, then back to Music.
  savePlaylist() {
    const owned = this.owned(), bits = playlistBits({ playlist: this.playlist }, owned);
    const previous = this.settings().playlist || [], merged = Array.from({ length: Math.max(previous.length, bits.length) }, (_, i) => owned[i] ? bits[i] : false);
    this.set({ playlist: merged });
    const c = this.career;
    if (c?.songState && !unlockAll()) { try { c.songState(this.riderId).playlist = merged.flatMap((b, i) => (b ? [i] : [])); c.persist?.(); } catch {} }
    if (this.previewed) { try { this.audio?.endPreview?.({ career: !!this.ui.careerMode }); } catch {} this.previewed = false; }
  }
  back() {
    const ui = this.ui, s = ui.screen; if (this.flash) return;
    if (this.confirm) { this.confirm = null; return; }                // buy popup: Triangle is silent (0x1CAED8)
    this.sfx('accept');                                                 // Triangle: the engine's default query 0x101 -> kind 6 (snd 3)
    if (s === 'fe-playlist' || s === 'audio-playlist') {
      this.savePlaylist();
      const to = s === 'audio-playlist' ? 'audio' : 'fe-music';
      this.go(() => { ui.set(to); ui.index = 4; ui.sync(); });
      return;
    }
    if (s === 'fe-sound') { const back = this.ctx.sound?.back || (() => ui.set('fe-options')); this.go(back); return; }
    if (s === 'pda-more') { ui.set('pda-options'); ui.sync(); return; }
    if (s === 'pda-options') {   // back to the menu that opened Options, on its Options row
      const to = ui.optionsReturn || 'pause', single = !!ui.careerUI?.singlePause?.(); ui.optionsReturn = null; ui.set(to);
      ui.index = to === 'ctm-pause' ? (single ? 3 : 4) : to === 'ctm-mcomm' ? 5 : to === 'ctm-bcpause' ? 4 : 3; ui.sync(); return;
    }
    const back = this.ctx.back || (() => ui.set(s === 'audio' ? 'pause' : 'setup'));
    this.go(back);
  }
  // Sound Options: Left/Right change the focused row (0x39DFE8 sliders step by 1; list boxes cycle).
  change(direction) {
    if (SCREENS[this.ui.screen]?.pda) return this.changePda(direction);
    const row = SOUND_ROWS[this.ui.index]; if (!row || row.hidden) return;
    const st = this.settings();
    if (row.slider) {
      const v = Math.max(0, Math.min(SLIDER_STEPS - 1, (st[row.key] ?? 10) + direction));
      if (v === st[row.key]) return;
      this.set({ [row.key]: v }); this.sfx('move'); return;
    }
    // list boxes wrap (0x399904) and play kind 1 (move) on every Left/Right (UIListBox 0x399970)
    if (row.key) this.set({ [row.key]: !st[row.key] });
    else { this.soundMode = (this.soundMode + direction + 4) % 4; try { localStorage.setItem('ssx3.soundMode', String(this.soundMode)); } catch {} }
    this.sfx('move');
  }
  reset() { this.set({ music: 10, effects: 10, speech: 10, dj: true, arcadeAudio: true }); /* the profile defaults 0x14F458 */ this.soundMode = 0; try { localStorage.setItem('ssx3.soundMode', '0'); } catch {} this.sfx('accept'); }
  move(direction) {
    const s = this.ui.screen, before = this.ui.index;
    this.ui.index = stepMenu(this.focus(), direction, this.disabledList(s));
    if (s === 'fe-playlist' || s === 'audio-playlist') { const top = this.top; this.top = scrollTop(this.top, this.ui.index, this.list.length); if (top !== this.top && this.ui.index !== before) { this.onFocus(); this.sfx('move'); return; } }
    if (this.ui.index !== before) { this.onFocus(); this.sfx('move'); }
  }
  onFocus() { this.focusAt = this.now(); }
  key(e) {
    const s = this.ui.screen; if (!this.owns(s)) return false;
    const codes = ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Enter', 'Space', 'Escape', 'ShiftLeft', 'ShiftRight', 'KeyO'];
    if (codes.includes(e.code)) e.preventDefault();
    if (this.flash) return true;
    if (e.repeat && !['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.code)) return true;
    if (this.confirm) {
      if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { this.confirm.index ^= 1; this.sfx('move'); }
      else if (e.code === 'Enter' || e.code === 'Space') { const yes = this.confirm.index === 0; this.sfx('accept'); if (yes) this.buy(); else this.confirm = null; }
      else if (e.code === 'Escape') this.back();
      this.ui.sync(); return true;
    }
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') this.move(e.code === 'ArrowUp' ? -1 : 1);
    else if ((e.code === 'ArrowLeft' || e.code === 'ArrowRight') && (s === 'fe-sound' || SCREENS[s]?.pda)) this.change(e.code === 'ArrowLeft' ? -1 : 1);
    else if (e.code === 'Enter' || e.code === 'Space') this.choose(this.ui.index);
    else if (e.code === 'Escape') this.back();
    else if ((e.code === 'ShiftLeft' || e.code === 'ShiftRight' || e.code === 'KeyO') && (s === 'fe-playlist' || s === 'audio-playlist')) this.preview();
    else if ((e.code === 'ShiftLeft' || e.code === 'ShiftRight' || e.code === 'KeyO') && s === 'fe-sound') this.reset();
    this.ui.sync(); return true;
  }

  // ---- UI sounds for the rest of the port (0x294F78 moments; docs/audio-menus.md "UI sounds") ----
  // One capture-phase observer instead of edits in every screen file, following the LUI engine (0x39B000):
  // Up/Down that moves the cursor -> kind 1 move; Left/Right that changes a list box / rider / value -> kind 1 move;
  // Cross/Enter on an enabled item -> kind 6 accept (snd 3), on a greyed one -> kind 4 error; Triangle/Escape that
  // leaves -> the default query's kind 6 too (snd 3: the engine never plays kind 3 "back"). Front end uses ev 0..4,
  // the overlay (pause, MCOMM, lodge, results) 9..13. Screens owned here play their own sounds.
  watchSounds() {
    if (this.watching || typeof addEventListener !== 'function') return; this.watching = true;
    const ui = this.ui;
    // Left/Right values the screens cycle (options, rider, profile page, uber list, gear mode...).
    const sig = () => { const cs = ui.characterSelect, fe = ui.feScreens, cu = ui.careerUI; try { return JSON.stringify([ui.cameraView, ui.widescreen, ui.keyboardMode, ui.riderIndex, ui.rider?.id, cs?.cheat?.id, cs?.index, fe?.page, fe?.uberIndex, fe?.keyboard?.col, fe?.keyboard?.row, cu?.lodge?.gearMode, cu?.lodge?.category, cu?.goal]); } catch { return ''; } };
    const snap = () => ({ screen: ui.screen, index: ui.index, flash: !!(ui.feScreens?.flash || ui.characterSelect?.flash || this.flash), sig: sig() });
    addEventListener('keydown', (e) => {
      const s = ui.screen;
      if (!ui.ready || e.repeat && !['ArrowUp', 'ArrowDown'].includes(e.code) || this.owns(s) || s === 'game' || s === 'loading' || s === 'title' && e.code !== 'Enter') return;
      if (!['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Enter', 'Space', 'Escape'].includes(e.code)) return;
      const before = snap(), ingame = PAUSE_SCREENS.has(s) || (s === 'options' && ui.optionsReturn !== 'fe-options' && !!ui.optionsReturn) || s === 'pause';
      const off = !!ui.nav?.children?.[ui.index]?.disabled, keyboard = !!ui.feScreens?.keyboard;
      setTimeout(() => {
        const after = snap(), changed = after.screen !== before.screen || after.flash !== before.flash;
        const play = (name) => { try { this.audio?.ui?.(uiEvent(name, ingame)); } catch {} };
        if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { if (after.index !== before.index && !changed) play('move'); }
        else if (e.code === 'ArrowLeft' || e.code === 'ArrowRight') { if (changed || after.index !== before.index || keyboard || after.sig !== before.sig) play('move'); }
        else if (e.code === 'Enter' || e.code === 'Space') { if (keyboard) play('move'); else play(off ? 'error' : 'accept'); }   // keyboard popup 0x1CE254: typing a key = kind 1
        else if (e.code === 'Escape') { if (changed && !PANELS.has(before.screen)) play('accept'); }
      }, 0);
    }, true);
  }

  // ---- drawing ----
  events(id, frame) {
    const lui = this.lui[id], screen = lui.screen, model = this.models[id], out = [], focus = [];
    const intro = model?.intro ?? 25;
    let row = this.ui.index; if (id === 'fe-playlist' || id === 'audio-playlist') row -= this.top;
    if (model?.frames?.[row] != null) focus.push(model.frames[row]);
    const start = this.focusAt - this.enterAt[id], first = model?.frames?.[0], items = new Set(model?.items || []);
    for (const ev of screen.events) {
      if (ev.frame <= intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      // the first focus state also sets the non-menu widgets (help text colour and position): keep them for every row
      else if (ev.frame === first && !focus.includes(first) && !items.has(ev.element) && frame >= intro) out.push({ ev, start: ev.frame });
    }
    for (const ev of screen.events) if (focus.includes(ev.frame)) out.push({ ev, start });
    const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop?.events || [], sf = frame % 600;
    if (SCREENS[id].pack === 'FE') for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    return out;
  }
  draw(c, b) {
    const ui = this.ui, id = ui.screen, now = this.now();
    if (this.flash && now - this.flash.at >= FLASH) { const f = this.flash; this.flash = null; f.to(); return; }
    if (this.enterAt[id] == null) this.enterAt[id] = now;
    this.focus();
    if (id === 'fe-playlist' || id === 'audio-playlist') this.top = scrollTop(this.top, ui.index, this.list.length);
    const frame = now - this.enterAt[id], lui = this.lui[id], events = this.events(id, frame);
    if (SCREENS[id].ingame) { if (ui.careerUI?.mcommFrame) ui.careerUI.mcommFrame(c, b); else { b.fillStyle = '#7198b0'; b.fillRect(0, 0, 640, 448); } }
    else { b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448); }
    c.save(); c.scale(1, SY);
    lui.draw(c, events, frame, this.override(id, events, frame));
    if (this.confirm) this.drawConfirm(c);
    if (this.flash) { c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash.at) / FLASH)})`; c.fillRect(0, 0, 640, 480); }
    c.restore();
    this.ui.careerUI?.lodgeFlash?.draw(c);   // pv lodgeFlash (web/lui-flash.js): the fall of the lodge's flash when the lodge opened Music
  }
  override(id, events, frame) {
    const ui = this.ui, lui = this.lui[id], dis = this.disabledList(id), model = this.models[id], pack = SCREENS[id].pack;
    const spr = this.data.sprites[pack], items = new Map((model?.items || []).map((n, i) => [n, i]));
    if (id === 'fe-music' || id === 'audio') {
      const mode = this.settings().radioMode ?? 0, has = !dis[2];
      const ctx = { has, ctm: !!this.ctx.ctm && id === 'fe-music', ingame: id === 'audio' };
      return (e) => {
        if (items.has(e.name) && dis[items.get(e.name)]) return { alpha: DISABLED_ALPHA };
        const box = BOXES.indexOf(e.name);
        if (box >= 0 && box < 4) return { sprite: MENU_MODES[box] === mode ? spr.checkbox : spr['empty box'], ...(dis[box] ? { props: { 14: 128, 15: 128, 16: 128 } } : {}) };
        if (e.name === N.helptext) return { text: this.t(musicHelp(ui.index, ctx)) };
        return null;
      };
    }
    if (id === 'fe-playlist' || id === 'audio-playlist') return this.songOverride(id, spr);
    if (id === 'fe-sound') return this.soundOverride(lui, events, frame);
    if (SCREENS[id]?.pda) return this.pdaOverride(id, lui, events, frame);
    return () => null;
  }
  songOverride(id, spr) {
    const ui = this.ui, songs = this.songs(), owned = this.owned(), playlist = this.playlist || [], ingame = id === 'audio-playlist';
    const ctm = !!this.ctx.ctm && !ingame, song = this.songAt(), st = song != null ? this.songInfo(song) : null, cash = this.cash();
    const now = this.audio?.songTitle || null, button = st && songButton(st);
    return (e) => {
      const r = SONG_ROWS.indexOf(e.name);
      if (r >= 0) { const s = this.list[this.top + r]; return s == null ? { hidden: true } : { text: songs[s]?.title || '' }; }
      const b = BOXES.indexOf(e.name);
      if (b >= 0) { const s = this.list[this.top + b]; if (s == null) return { hidden: true }; return { sprite: playlist[s] ? spr.checkbox : owned[s] ? spr['empty box'] : spr['yell_dollar sign'], props: { 13: 255 } }; }
      switch (e.name) {
        case N.arrUp: return this.top > 0 ? null : { hidden: true };
        case N.arrDown: return this.top + ROWS < this.list.length ? null : { hidden: true };
        case N.songList: return { text: fmt(this.t('kT_OVRCMNSongsInList'), count(playlist)) };
        case N.numSongs: case N.singer: return { hidden: true };
        case N.songBy: return st ? { text: fmt(this.t('kT_OVRCMNSongBy'), songs[song]?.artist || '') } : { hidden: true };
        case N.helptext: return st ? { text: this.t(songHelp(st, { ctm })) } : null;
        case N.current: return { text: fmt(this.t('kT_16NowPlaying'), now?.title || '') };
        case N.artist: return { text: fmt(this.t('kT_16Artist'), now?.artist || '') };
        case N.cost: return ctm && st && !st.mine ? null : { hidden: true };
        case N.costAnswer: return ctm && st && !st.mine ? { text: st.credits > 0 ? this.t('kT_OVRCMNFree') : `$ ${SONG_PRICE.toLocaleString('en-US')}` } : { hidden: true };
        case N.youHave: return ctm ? null : { hidden: true };
        case N.youHaveAnswer: return ctm ? { text: `$ ${cash.toLocaleString('en-US')}` } : { hidden: true };
        case N.freeSongs: return ctm && st?.credits > 0 ? null : { hidden: true };
        case N.select: return button ? { text: this.t(button), props: { 6: 300 } } : { hidden: true };   // one line (the code sets it; no wrap)
        case N.ps2x: return button ? null : { hidden: true };
        case N.preview: case N.ps2sq: return !ingame || (st && st.mine) ? null : { hidden: true };
      }
      const f = FREE.indexOf(e.name); if (f >= 0) return ctm && st && f < st.credits ? null : { hidden: true };
      return null;
    };
  }
  soundOverride(lui, events, frame) {
    const st = this.settings(), row = this.ui.index, by = lui.byName;
    const value = { '00003822': this.t(SPEAKER_MODES[this.soundMode]), '00003c22': this.t(st.dj ? 'kT_CMNOn' : 'kT_CMNOff'), '00003d22': this.t(st.arcadeAudio ? 'kT_CMNOn' : 'kT_CMNOff') };
    const knobs = new Map(SOUND_ROWS.filter((r) => r.slider).map((r) => { const sl = by.get(r.slider); return [sl.knob, { track: by.get(sl.track), knob: by.get(sl.knob), key: r.key }]; }));
    const helps = new Map(SOUND_ROWS.map((r, i) => [r.help, i]));
    return (e) => {
      if (e.name === N.ongroup) return { hidden: true };                 // online footer ('Online' tag, Accept): offline -> fegroup
      if (e.name === N.fegroup) return { alpha: 255 };
      if (e.name === '00000037' || e.name === '000003e4' || e.name === '00003e22') return { hidden: true };
      if (value[e.name] != null) return { text: value[e.name] };
      if (e.widget === 'slider') return { alpha: 255 };                   // the slider group itself is A=0; its track/knob show
      const k = knobs.get(e.name);
      if (k) { const tp = lui.props(k.track, events, frame), kp = lui.props(k.knob, events, frame); return { props: { 0: (tp[0] || 0) + knobX(st[k.key] ?? 11, tp[6] || 160, kp[6] || 18) } }; }
      if (helps.has(e.name)) return { alpha: helps.get(e.name) === row ? 255 : 0 };
      if (e.name === N.lbArrows) return SOUND_ROWS[row]?.kind === 'lb' ? { alpha: 255, props: { 1: 5 + 20 * row } } : { hidden: true };
      return null;
    };
  }
  // ---- in-game Options (pv pdaOptions) ----
  // Save game: Conquer the Mountain only (a Single Event has no profile to save: greyed, PS2 r3-options).
  canSave() { const cu = this.ui.careerUI; return !!cu?.career && (!!cu.active?.career || !!cu.freeRide); }
  word(hash, fallback) { return this.ui.careerUI?.t?.(hash, fallback) || fallback; }
  hudLevel() { return this.ui.feScreens?.hudLevel?.() ?? 0; }
  setHud(v) { const x = this.ui.feScreens?.extra; if (!x) return; x.options = { ...x.options, hud: v }; x.commit?.(); }
  pdaValue(r) {
    const ui = this.ui, st = this.settings();
    switch (r.kind) {
      case 'hud': return this.word(HUD_LEVELS[this.hudLevel()] ?? HUD_LEVELS[0], ['Full', 'Minimal', 'None'][this.hudLevel()] || 'Full');
      case 'camera': return this.word(CAMERA_WORDS[ui.cameraView] ?? CAMERA_WORDS[0x3D], { 0x3C: 'Near', 0x3D: 'Mid', 0x3E: 'Far' }[ui.cameraView] || 'Mid');
      case 'camera2': return this.word(CAMERA_WORDS[0x3C], 'Near');   // player 2's view (one player: greyed)
      case 'widescreen': return WIDESCREEN_MODES[ui.widescreen]?.label ?? '';
      case 'keyboard': return ui.keyboardMode || '';
    }
    if (r.key === 'dj' || r.key === 'arcadeAudio') return this.t(st[r.key] ? 'kT_CMNOn' : 'kT_CMNOff');
    return null;
  }
  pdaHelp(r) {
    if (r.kind === 'keyboard') return this.ui.keyboardMode === 'Simple' ? 'Arrows/WASD steer on the ground and spin/flip in the air.' : 'Arrows/WASD steer; IJKL spin/flip in the air.';
    return r.helpText ?? null;
  }
  // Left/Right: list boxes wrap, sliders step (0x39DFE8), as Sound Options; greyed and action rows do nothing.
  changePda(direction) {
    const id = this.ui.screen, r = pdaRows(id)[this.ui.index], ui = this.ui; if (!r || r.action || this.disabledList(id)[this.ui.index]) return;
    const st = this.settings();
    if (r.slider) { const v = Math.max(0, Math.min(SLIDER_STEPS - 1, (st[r.key] ?? 10) + direction)); if (v === st[r.key]) return; this.set({ [r.key]: v }); this.sfx('move'); return; }
    if (r.key) this.set({ [r.key]: !st[r.key] });
    else if (r.kind === 'hud') this.setHud((this.hudLevel() + direction + 3) % 3);
    else if (r.kind === 'camera') ui.cycleCamera?.(direction);
    else if (r.kind === 'widescreen') ui.cycleWidescreen?.(direction);
    else if (r.kind === 'keyboard') ui.cycleKeyboard?.();
    this.sfx('move');
  }
  choosePda(i) {
    const id = this.ui.screen, r = pdaRows(id)[i], ui = this.ui; if (!r?.action) return;   // Cross on a value row: nothing (silent)
    if (this.disabledList(id)[i]) { this.sfx('error'); return; }
    this.sfx('accept');
    if (r.kind === 'more') { ui.set('pda-more'); ui.sync(); return; }
    if (r.kind === 'display') { if (!ui.feScreens?.openDisplay?.('pda-more')) this.sfx('error'); return; }
    if (r.kind === 'save') { try { const cu = ui.careerUI; cu.saved = cu.career.persist(); } catch (error) { console.warn(error); } }
  }
  pdaOverride(id, lui, events, frame) {
    const rows = pdaRows(id), more = id === 'pda-more', dis = this.disabledList(id), at = rows[this.ui.index] || rows[0], st = this.settings();
    const byEl = new Map(); rows.forEach((r, i) => { byEl.set(r.label, { r, i, part: 'label' }); if (r.value) byEl.set(r.value, { r, i, part: 'value' }); if (r.slider) byEl.set(r.slider, { r, i, part: 'slider' }); });
    const helps = new Map(rows.map((r, i) => [r.help, i]));
    const knobs = new Map(rows.filter((r) => r.slider && r.key).map((r) => { const sl = lui.byName.get(r.slider); return [sl.knob, { track: lui.byName.get(sl.track), knob: lui.byName.get(sl.knob), key: r.key }]; }));
    const shown = new Set(); for (const r of rows) for (const n of [r.row, r.label, r.value, r.slider, r.help]) if (n) shown.add(n);
    const pageRows = new Set(PDA_ROWS.flatMap((r) => [r.row, r.label, r.value, r.slider].filter(Boolean)));
    const helpNames = new Set(PDA_ROWS.map((r) => r.help).concat(['0346ec30']));
    return (e) => {
      const n = e.name;
      if (!more && n === '06a83bbc') return { hidden: true };                     // the EA Talk slider (the More row has none)
      if (more && pageRows.has(n) && !shown.has(n)) return { hidden: true };   // the rows the More page does not use
      if (more && n === PDA_ROWS[2].value) return { hidden: true };           // Display & Touch: no value
      if (helpNames.has(n) && !helps.has(n)) return { alpha: 0 };
      if (helps.has(n)) { const i = helps.get(n), r = rows[i]; if (rows[this.ui.index]?.help !== n) return { alpha: 0 }; const t = this.pdaHelp(rows[this.ui.index]); return t != null ? { alpha: 255, text: t } : { alpha: 255 }; }
      const k = knobs.get(n);
      if (k && !more) { const tp = lui.props(k.track, events, frame), kp = lui.props(k.knob, events, frame); return { props: { 0: (tp[0] || 0) + knobX(st[k.key] ?? 11, tp[6] || 120, kp[6] || 18) } }; }
      const hit = byEl.get(n);
      if (hit) {
        const o = {};
        if (hit.part === 'label') { if (hit.r.text) o.text = hit.r.text; else if (/%d/.test(e.text || '')) o.text = e.text.replace('%d', hit.r.kind === 'camera2' ? '2' : '1'); }
        if (hit.part === 'value') { const v = this.pdaValue(hit.r); if (v != null) o.text = v; }
        if (dis[hit.i]) o.alpha = DISABLED_ALPHA;
        if (hit.part === 'slider') o.alpha = dis[hit.i] ? DISABLED_ALPHA : 255;
        return o;
      }
      if (n === PDA_ROWS[9].row && !more) return { alpha: 255 };               // the EA Talk row group (online only on the PS2)
      // an action row: no arrows, the Cross on the row (the Save game focus state 115, moved to the row)
      if (at.action) {
        if (n === PDA.arrowL || n === PDA.arrowR || (n === PDA.valueBar && at.kind !== 'save')) return { alpha: 0 };   // Save keeps the authored focus state
        if (n === PDA.crossRow) return { props: { 1: at.y } };
        if (n === PDA.crossGroup) return { alpha: 255, props: { 0: 282, 1: 0 } };
        if (n === PDA.cross) return { alpha: 255, props: { 0: 0, 1: 2 } };
      } else if (n === PDA.cross || n === PDA.crossGroup) return { alpha: 0 };
      if (n === PDA.arrows) return { alpha: 255 };   // the group's A 100 is its bar's; the arrows draw white (PS2 r3-options, as Sound Options' lbArrows)
      return null;
    };
  }
  // cUIStateBuyPopup (0x198988): "Buy song using free song credit?" / "Buy song?" with Yes / No.
  drawConfirm(c) {
    const ui = this.ui, q = this.t(this.confirm.credit ? 'kT_OVRCMNBuySongCredit' : 'kT_16BuyMusicTrack');
    c.fillStyle = 'rgba(58,108,148,.96)'; c.fillRect(110, 170, 420, 150); c.strokeStyle = '#8fb6cf'; c.lineWidth = 3; c.strokeRect(118, 178, 404, 134);
    ui.text(c, q, 320, 200, 18, '#0c1a26', 'FEFONT', 'center');
    [this.t('kT_CMNYes'), this.t('kT_CMNNo')].forEach((t, i) => { if (this.confirm.index === i) ui.sprite('OV_1-2', 55, 122, 24, 24, 272, 245 + i * 26, 16, 16); ui.text(c, t, 296, 245 + i * 26, 18, this.confirm.index === i ? '#eef4f7' : '#0c1a26'); });
  }
}
const PANELS = new Set(['results', 'ctm-results', 'ctm-objectives', 'ctm-records']);   // ev 14 panels; Triangle silent there
// In-game menus (overlay listener 0x1A2F70, events 9..13): pause family, MCOMM and its transport map, results, and the
// lodge (cFEStateLodge / BuyAttrib / CareerStats / LodgeRiderDetail are overlay states).
const PAUSE_SCREENS = new Set(['pause', 'ctm-pause', 'ctm-giveup', 'ctm-restart', 'ctm-mcomm', 'ctm-quit', 'results', 'ctm-results', 'ctm-records',
  'ctm-peaks', 'ctm-goals', 'ctm-events', 'ctm-confirm', 'ctm-lodge', 'ctm-attributes', 'ctm-details', 'ctm-saveprompt', 'ctm-saved']);
