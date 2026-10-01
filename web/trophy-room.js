// The lodge's Trophies (Rider Details > Trophies, trophyLui): the three FE.LUI screens the PS2 plays for it, drawn for
// web/lodge-ui.js with the rules of their states (PS2 captures local/ps2-capture/menus/trophy-tour, trophy-medals,
// trophy-stats, trophy-earn, trophy-locked, trophy-pop*):
//  'ctm-trophies'    125mountainroom (vtable 0x46987C: enter 0x1D2990, bind 0x1D2AD0, input 0x1D2F68, markers 0x1D3340): Peak 1..3 and
//                    Peak Pass over the mountain with a marker per event, the Peak Pass popup (0x1D32E0: the rider's pass picture).
//  'ctm-trophy-peak' 126peakroom (0x4697AC: bind 0x1D38F8, input 0x1D3C80, help 0x1D3EC8, rows 0x1D3F80): the peak's four goals, the
//                    focused goal's events with their medals, a goal's trophy thumbnail once complete.
//  'ctm-trophy-room' 127trophyroom (0x4696DC: bind 0x1D43F0, input 0x1D4698, picture 0x1D47A0, stats 0x1D4890 -> 0x1CE9E0): a complete
//                    goal's trophy and its events; the focused row shows the trophy or the event's medal picture and its best.
// Screen data: UI/character-select.json (tools/export_character_select.py: the three screens, trophy_sprites, the strings); the
// pictures are the RWRDPS2 rewards (CAREER/REWARDS, tools/export_career.py).
import { LuiScreen } from './lui-player.js';
import { menuModel, stepMenu } from './fe-screens.js';
import { MEDAL, RIDER_CHARACTER, eventKey, isTimed } from './career.js';
import { format } from './locale.js';
import { FOCUS_LAG } from './lui-flash.js';

const SY = 448 / 480;
export const TROPHY_SCREENS = { 'ctm-trophies': '125mountainroom', 'ctm-trophy-peak': '126peakroom', 'ctm-trophy-room': '127trophyroom' };
const GOALS = ['race', 'freestyle', 'freeride', 'earnings'];           // PS2 goals 0..3 (goal 2 = exploration)
const MARKER = ['race', 'slopestyle', 'superpipe', 'bigair', 'racebackcountry', 'freestylebackcountry'];   // mode 0..5 (0x1D3340)
const TROPHY_CODE = ['trc', 'tfs', 'tex', 'ter'], MEDAL_CODE = ['rac', 'fst', 'exp', 'ern'];            // RWRDPS2 +8 names: FE textures
const POPUP = [90, 114];                                               // 125mountainroom: the popup's timeline (label 0x079b4860 .. 114)
const RIVAL = ['Happiness', 'Ruthless', 'Throne'];                     // 0x1CE758: the rival events' names are literal on the PS2

export class TrophyRoom {
  constructor(lodge) { this.lodge = lodge; this.lui = {}; this.models = {}; this.peak = 1; this.goal = 0; this.popup = null; this.shown = {}; this.images = null; }
  get ui() { return this.lodge.ui; }
  get c() { return this.lodge.c; }
  get id() { return this.lodge.id; }
  get fe() { return this.ui.feScreens; }
  t(name, fallback = '') { return this.fe?.t(name, fallback) || fallback; }
  now() { return performance.now() * 60 / 1000; }

  // The screens load with the FE data (the Select Character export); until then (or an older export) the lodge keeps its list.
  ready() {
    if (this.lui['ctm-trophies']) return true;
    const data = this.fe?.data, screens = data?.screens;
    if (!screens || !data.trophy_sprites || !Object.values(TROPHY_SCREENS).every((k) => screens[k])) return false;
    const snow = screens.bg_snow_loop;
    const merge = (a) => (snow ? { ...a, elements: [...a.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))], animations: { ...a.animations, ...snow.animations } } : a);
    this.images = Object.create(this.fe.images || {});                   // + the reward pictures ('R:' keys)
    for (const [id, key] of Object.entries(TROPHY_SCREENS)) {
      this.lui[id] = new LuiScreen(merge(screens[key]), this.images, this.ui);
      this.lui[id].byLabel = new Map(screens[key].elements.filter((e) => e.label).map((e) => [e.label, e.name]));
      this.models[id] = menuModel(screens[key]);
    }
    // The pass popup's frame grows from 20 % (its shapes' scale props, as 139buy_popup: docs/career-events.md); every other shape of the
    // three screens is at 100 %.
    this.lui['ctm-trophies'].shapeScale = true;
    this.sprites = data.trophy_sprites;
    return true;
  }
  owns(s) { return !!TROPHY_SCREENS[s] && this.ready(); }

  // ---- state ----
  rider() { return this.c.rider(this.id); }
  passes() { const p = this.rider().peaks || []; return p[2] ? 2 : p[1] ? 1 : 0; }         // +0xB4: the highest peak without its lock bit (0x145F90)
  complete(peak, goal) { return this.c.goalComplete(this.id, peak, GOALS[goal]); }       // 0x157BF0
  // The goal's rows (0x1CE6F0 / 0x1CE758 / 0x1CED90): its table 0x45AAD8 entries; exploration = collectibles, Big Challenges, earnings = one.
  rows(peak, goal) {
    const c = this.c;
    if (goal === 2) return [{ text: this.t('collectibles', 'Collectibles'), medal: c.collectMedal(this.id, peak), stat: 'collect' },
      { text: this.t('big_challenges', 'Big Challenges'), medal: c.challengeMedal(this.id, peak), stat: 'challenge' }];
    if (goal === 3) return [{ text: this.t('earnings', 'Earnings'), medal: this.complete(peak, 3) ? MEDAL.GOLD : MEDAL.NONE, stat: 'earned' }];   // 0x158AF0
    return c.goalEvents(this.id, peak, GOALS[goal]).map((e) => ({ ...e, text: this.eventText(e, peak), stat: isTimed(e.mode) ? 'time' : 'score' }));
  }
  eventText(e, peak) {
    if (e.mode >= 6) return this.t(['pk1_race', 'pk2_race', 'all_peak_race', 'pk1_jam', 'pk2_jam', 'all_peak_jam'][e.mode - 6], this.c.eventName(e.mode, e.course));
    if (e.mode === 4) return format(`${RIVAL[peak - 1]} %s`, this.t('race_caps', 'Race'));
    if (e.mode === 5) return `${RIVAL[peak - 1]} Jam`;
    return this.c.courses[e.course]?.name ?? '';                                         // 0x144C60
  }
  // 0x1CE9E0: the row's line under the picture (best time as mm:ss of the record's whole seconds, best score, cash earned, counts).
  stat(row, peak) {
    const c = this.c, r = this.rider();
    if (row.stat === 'earned') return format(this.t('you_earned', "You've earned: %s"), `$ ${(r.earned || 0).toLocaleString('en-US')}`);
    if (row.stat === 'collect') return format(this.t('collect_num', 'Collectibles: %d/%d'), c.peakCollected(this.id, peak), c.peakCollectTotal(peak));
    if (row.stat === 'challenge') return format(this.t('chal_num', 'Challenges Complete: %d/%d'), c.challengesDone(this.id, peak), c.peakChallengeTotal(peak));
    const best = r.best?.[eventKey(row.mode, row.course)];
    if (row.stat === 'time') {
      const s = best == null ? 0 : Math.floor(best / 60),
        two = (n) => String(n).padStart(2, '0');
      return format(this.t('best_time', 'Your best time: %S'), `${two(Math.floor(s / 60))}:${two(s % 60)}`);
    }
    return format(this.t('best_score', 'Your best score: %d'), best ?? 0);
  }
  reward(section, index) { return this.c.rewardItems(section)[index] || null; }
  // A reward picture as a LUI sprite (drawn over the element's box, as the PS2's loaded texture +0x78).
  picture(pic) {
    if (!pic) return null;
    const key = 'R:' + pic;
    if (!Object.prototype.hasOwnProperty.call(this.images, key)) { const im = this.lodge.cs.picture('REWARDS/' + pic); if (!im) return null; this.images[key] = im; }
    const im = this.images[key];
    return { page: key, sx: 0, sy: 0, sw: im.naturalWidth || 256, sh: im.naturalHeight || 256 };
  }

  // ---- menus ----
  items(s) {
    if (s === 'ctm-trophy-room') return [this.reward('trophy', this.goal * 3 + this.peak - 1)?.name || '', ...this.rows(this.peak, this.goal).map((r) => r.text)];
    return this.models[s]?.texts || [];
  }
  layout(s, i) {
    const lui = this.lui[s], model = this.models[s], name = model?.items[i], e = name && lui?.byName.get(name);
    if (!e || i >= this.items(s).length) return [0, -100, 1, 1];
    const menu = lui.byName.get(model.menu), mp = menu?.props || {}, p = e.props || {}, w = p[6] || 200, h = p[7] || 20, [ox, oy] = lui.anchor(p, w, h);
    return [(mp[0] || 0) + (p[0] || 0) + ox, ((mp[1] || 0) + (p[1] || 0) + oy) * SY, w, h * SY];
  }
  key(e) {
    const s = this.ui.screen;
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') {
      e.preventDefault();
      // the LUI menus wrap (PS2 trophy-locked, trophy-stats)
      if (!e.repeat && !this.popup) { const n = this.items(s).length; this.ui.index = stepMenu(this.ui.index, e.code === 'ArrowUp' ? -1 : 1, Array(n).fill(false)); this.ui.sync(); }
      return true;
    }
    return false;
  }
  choose(i) {
    const ui = this.ui, s = ui.screen;
    if (s === 'ctm-trophies') {
      // 0x1D2F68: Peak Pass opens the pass popup (0x1D32E0); a peak opens its room, locked or not (PS2 trophy-locked: Peak 2 / 3 rooms)
      if (this.popup) return;                                            // the popup keeps Peak Pass focused (Cross opens it again: no change)
      if (i === 3) { this.popup = { at: this.now() }; return; }
      this.go(() => { this.peak = i + 1; ui.set('ctm-trophy-peak'); }); return;
    }
    if (s === 'ctm-trophy-peak') { if (!this.complete(this.peak, i)) return; this.go(() => { this.goal = i; ui.set('ctm-trophy-room'); }); return; }   // 0x1D3C80: complete goals only
  }
  back() {
    const ui = this.ui, s = ui.screen;
    if (s === 'ctm-trophies' && this.popup) { this.popup = null; return; }                // 0x1D2EA0: Triangle hides the popup group first
    if (s === 'ctm-trophies') { this.go(() => { ui.set('ctm-details'); ui.index = 1; ui.sync(); }); return; }
    if (s === 'ctm-trophy-peak') { this.go(() => { ui.set('ctm-trophies'); ui.index = this.peak - 1; ui.sync(); }); return; }
    if (s === 'ctm-trophy-room') this.go(() => { ui.set('ctm-trophy-peak'); ui.index = this.goal; ui.sync(); });
  }
  // each room change is a state change (0x39F400; 125mountainroom's Triangle 0x1D4698): the TransitionOut flash (web/lui-flash.js)
  go(to) { const cs = this.lodge.cs; if (cs?.lodgeGo) cs.lodgeGo(to); else to(); }

  // ---- drawing ----
  timeline(s, index) {
    const lui = this.lui[s], model = this.models[s], now = this.now(), st = this.shown;
    // a screen entered afresh (another screen, or this one again after the page drew something else) replays its intro
    if (st.screen !== s || !(now - st.drawn <= 15)) { st.screen = s; st.enter = this.lodge.cs?.lodgeFlash?.introStart?.(now) ?? now; st.index = null; this.popup = null; }
    st.drawn = now;
    if (st.index !== index) { st.index = index; st.focus = now; }
    // the mountain room's focus plays from its intro's 0x42 label (frame 25), when its activation (0x1D2A90 -> 0x186518)
    // sets the cursor; the peak / trophy rooms' activations (0x1D3C60 / 0x1D43D0) set none
    const frame = now - st.enter, out = [], intro = s === 'ctm-trophies' ? (lui.screen.labels?.find((l) => l.control?.some((c) => c.startsWith('42')))?.frame ?? 0) + FOCUS_LAG : 0;
    const focusStart = Math.max(st.focus - st.enter, intro);
    for (const ev of lui.screen.events) {
      if (ev.frame <= model.intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (ev.frame === model.frames[index]) { if (frame >= focusStart) out.push({ ev, start: focusStart }); }
      else if (this.popup && s === 'ctm-trophies' && ev.frame >= POPUP[0] && ev.frame <= POPUP[1]) {
        const start = this.popup.at - st.enter + ev.frame - POPUP[0];
        if (frame >= start) out.push({ ev, start });
      }
    }
    const snow = this.fe.data.screens.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    return { events: out, frame };
  }
  draw(c, b) {
    const s = this.ui.screen; if (!this.owns(s)) return false;
    const n = this.items(s).length; if (this.ui.index >= n) this.ui.index = 0;
    const lui = this.lui[s], { events, frame } = this.timeline(s, this.ui.index);
    const override = s === 'ctm-trophies' ? this.mountain() : s === 'ctm-trophy-peak' ? this.peakRoom() : this.trophyRoom();
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    // a shape draws at its vertex alpha only (the GS applies it once: the popups' veils and boxes, docs/career-events.md 139buy_popup)
    lui.draw(c, events, frame, (e) => { const o = override(e); return e.kind === 'shape' && !o?.hidden ? { alpha: 255, ...(o || {}) } : o; });
    c.restore();
    return true;
  }
  // 125mountainroom: title kT_TITLETrophyRoom, help by focus (0x1D2F68 focus: pass / peak 1 / peak 2..3 with their lock), a marker per
  // standard event of the race and freestyle goals (0x1D3340: 'dot_visit arrow' with a medal, else 'dot_loc arrow').
  mountain() {
    const passes = this.passes(), i = this.ui.index, marks = new Map(), count = {};
    for (let peak = 1; peak <= 3; peak++) for (const goal of [0, 1]) for (const e of this.c.goalEvents(this.id, peak, GOALS[goal])) {
      if (e.mode > 5) continue;
      const kind = MARKER[e.mode], k = count[kind] = (count[kind] ?? -1) + 1;
      marks.set(`mrk_${kind}_${k}`, e.medal !== MEDAL.NONE);
    }
    const help = [
      this.t('help_view_trophies'),
      passes >= 1 ? this.t('help_view_trophies') : this.t('locked_peak2'),
      passes >= 2 ? this.t('help_view_trophies') : this.t('locked_peak3'),
      this.t('help_view_pass')
    ][i];
    const character = RIDER_CHARACTER[this.id] ?? 3, pass = this.reward('peak_pass', character * 3 + passes), passSprite = pass && this.picture(pass.picture);
    return (e) => {
      const label = e.label;
      if (label === 'screentitle') return { text: this.t('trophy_room', 'Trophies') };
      if (label === 'helptext') return { text: help };
      if (label && marks.has(label)) return { sprite: this.sprites[marks.get(label) ? 'dot_visit arrow' : 'dot_loc arrow'] };
      if (label === 'Group peak pass' && !this.popup) return { hidden: true };
      if (label === 'Front' && !passSprite) return { hidden: true };                 // 0x1D32E0: Front only once the pass texture loaded (+0xC4)
      if (label === 'peak pass') return passSprite ? { sprite: passSprite } : { hidden: true };
      return null;
    };
  }
  // 126peakroom: title kT_PeakNname; each goal's trophy thumbnail (FE texture trc1..ter3) once complete; the focused goal's rows
  // (0x1D3F80): name, the medal icon (FE texture racp..ernb) and the check, or the cross.
  peakRoom() {
    const peak = this.peak, goal = this.ui.index, rows = this.rows(peak, goal), done = [0, 1, 2, 3].map((g) => this.complete(peak, g));
    const thumbs = { trph_race: 0, trph_freestyle: 1, trph_explore: 2, trph_earn: 3 };
    return (e) => {
      const label = e.label; if (!label) return null;
      if (label === 'screentitle') return { text: this.t(`peak${peak}_name`, `Peak ${peak}`) };
      if (label === 'helptext') return { text: done[goal] ? this.t('help_select_goal') : this.t('help_locked_goal') };
      if (label in thumbs) { const g = thumbs[label]; return done[g] ? { sprite: this.sprites[TROPHY_CODE[g] + peak] } : { hidden: true }; }
      let m = label.match(/^txtmdl_(\d)$|^(\d)text blocks$|^mdl_back(\d)$/);
      if (m) { const k = +(m[1] ?? m[2] ?? m[3]); return k < rows.length ? (m[1] != null ? { text: rows[k].text } : null) : { hidden: true }; }
      if ((m = label.match(/^mdl_(\d)$/))) { const r = rows[+m[1]]; return r && r.medal !== MEDAL.NONE ? { sprite: this.sprites[MEDAL_CODE[goal] + 'pgsb'[r.medal]] } : { hidden: true }; }
      if ((m = label.match(/^(\d)(no)?check$/))) { const r = rows[+m[1] - 1]; return r && (r.medal !== MEDAL.NONE) === !m[2] ? null : { hidden: true }; }
      return null;
    };
  }
  // 127trophyroom: title by goal; row 0 the trophy (0x156A90 name), rows 1..n the goal's events; the picture: the trophy on row 0, the
  // event's medal picture (0x156AE0 goal*4 + medal) on its row or none; the stats line on the event rows only.
  trophyRoom() {
    const peak = this.peak, goal = this.goal, rows = this.rows(peak, goal), i = this.ui.index, row = i > 0 ? rows[i - 1] : null;
    const trophy = this.reward('trophy', goal * 3 + peak - 1);
    const pic = i === 0 ? (this.complete(peak, goal) ? trophy?.picture : null) : row && row.medal !== MEDAL.NONE ? this.reward('medal', goal * 4 + row.medal)?.picture : null;
    const bitmap = this.picture(pic);
    return (e) => {
      const label = e.label; if (!label) return null;
      if (label === 'screentitle') return { text: this.t(['race_trophy', 'fs_trophy', 'explore_trophy', 'earnings_trophy'][goal]) };
      if (label === 'trophy') return { text: trophy?.name || '' };
      const m = label.match(/^event_(\d)$/);
      if (m) { const r = rows[+m[1]]; return r ? { text: r.text } : { hidden: true }; }
      if (label === 'bitmap') return bitmap ? { sprite: bitmap } : { hidden: true };
      if (label === 'stats') return row ? { text: this.stat(row, peak) } : { hidden: true };
      return null;
    };
  }
}
