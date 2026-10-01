// Lodge sub-screens (Rider Details -> Rewards / Trophies, Buy/Equip Gear, Ubertrick Setup) for web/career-ui.js. Catalogs come from the
// original data (RWRDPS2.DAT rewards, BOLTPS2.DAT gear, the 0x14FF90 uber trick table) via tools/export_career.py; the purchase rules are
// in web/career.js. Layout follows PS2 frames in
// local/ps2-capture/menus/lodge (Rider Details, Rewards, Buy Gear, Ubertrick Setup).
import { format } from './locale.js';
import { UBER_ROWS, uberEntries, SONG_FREE_CREDITS, SONG_PRICE } from './lodge.js';
import { openEquipGear, preloadEquipGear } from './wardrobe.js';
// Career Highlights = the monster tricks (0x1F5DA0)
import { MONSTER_LIST, monsterById } from './monster-tricks.js';
// the original 35car_stat LUI page
import { CareerHighlights, HIGHLIGHT_PAGES } from './career-highlights.js';
import { stepMenu } from './fe-screens.js';
import { TrophyRoom } from './trophy-room.js';
// Save Game = cFEStateProfileLoad mode 3 on FE.LUI 93profile_load + the memory-card popups // trophyLui: Trophies = FE.LUI
// 125mountainroom / 126peakroom / 127trophyroom
import { SaveGame, SAVE_SCREEN } from './save-game.js';

const Y = (y) => Math.round((y * 448) / 480);
// Rewards categories in the PS2 order (lodge/22-rewards.png) -> catalog section and locale id.
export const REWARD_CATEGORIES = [
  ['art', 'kT_OVRCMNArt', 'Art'],
  ['poster', 'kT_OVRCMNPosters', 'Posters'],
  ['toy', 'kT_OVRCMNToys', 'Toys'],
  ['trading_card', 'kT_OVRCMNTradeCard', 'Trading Cards'],
  ['cheat_character', 'kT_TITLECheatCharacter', 'Cheat Characters'],
  ['video', 'kT_OVRCMNVideos', 'Videos']
];
const ROWS = 10;
export const LODGE_SCREENS = [
  'ctm-details',
  'ctm-rewards',
  'ctm-reward-list',
  'ctm-reward-view',
  'ctm-buy',
  'ctm-trophies',
  'ctm-trophy-peak',
  'ctm-trophy-room',
  SAVE_SCREEN,
  'ctm-gear',
  'ctm-gear-buy',
  'ctm-uber',
  'ctm-uber-list',
  'ctm-uber-buy',
  'ctm-music',
  'ctm-songs',
  'ctm-song-buy',
  'ctm-highlights'
];

export class LodgeScreens {
  constructor(career) {
    this.cs = career;
    this.category = 0;
    this.top = 0;
    this.gearMode = 'buy';
    this.folders = [];
    this.uberRow = 0;
    this.trophies = new TrophyRoom(this);
    this.saveGame = new SaveGame(this);
  }
  saveLui(s) {
    return this.saveGame.owns(s);
  }
  trophyLui(s) {
    return this.trophies.owns(s);
  }
  get ui() {
    return this.cs.ui;
  }
  get c() {
    return this.cs.career;
  }
  get id() {
    return this.cs.riderId;
  }
  t(k, f) {
    return this.cs.t(k, f);
  }
  get peak() {
    return this.cs.lodgePeak || 1;
  }
  owns(s) {
    return LODGE_SCREENS.includes(s);
  }
  section() {
    return REWARD_CATEGORIES[this.category][0];
  }
  items(s) {
    if (this.trophyLui(s)) return this.trophies.items(s);
    if (this.saveLui(s)) return this.saveGame.items();
    switch (s) {
      case 'ctm-details':
        return [
          this.t(0x01d16b73, 'Rewards'),
          this.t('kT_TITLETrophyRoom', 'Trophies'),
          this.t('kT_TITLECheatCharacter', 'Cheat Characters'),
          'Ubertrick Setup',
          'Career Highlights',
          'Player Name',
          'Rider Profile'
        ];
      case 'ctm-rewards':
        return REWARD_CATEGORIES.map(([, k, f]) => this.t(k, f));
      case 'ctm-reward-list':
        return this.c.rewardItems(this.section()).map((i) => i.name);
      case 'ctm-reward-view':
        return [this.t('kT_BTNPrevious', 'Previous')];
      case 'ctm-buy':
        return [this.t('kT_CMNYes', 'Yes'), this.t('kT_CMNNo', 'No')];
      case 'ctm-trophies':
        return ['Peak 1', 'Peak 2', 'Peak 3'];
      case 'ctm-gear':
        return this.gearList().map((e) => e.name);
      case 'ctm-uber':
        return [...UBER_ROWS.map(([n]) => n), 'DONE'];
      case 'ctm-highlights':
        return this.highlights().map((h) => h.title);
      case 'ctm-uber-list':
        return this.uberList().map((x) => x.entry.name);
      case 'ctm-music':
        return [
          this.t(0x066204d7, 'Radio BIG'),
          this.t(0x0001add2, 'BIG Mountain Ambience'),
          this.t(0x07ece8aa, 'Custom Playlist [DJ]'),
          this.t(0x0ce97c4a, 'Custom Playlist [No DJ]'),
          this.t(0x063cab64, 'Edit Playlist')
        ];
      case 'ctm-songs':
        return this.c.songs().map((x) => x.title);
      case 'ctm-gear-buy':
      case 'ctm-uber-buy':
      case 'ctm-song-buy':
        return [this.t('kT_CMNYes', 'Yes'), this.t('kT_CMNNo', 'No')];
    }
    return [];
  }
  disabled(s, i) {
    // as on the PS2 (lodge/runs/l1, menus/lodge/20-rider-details) every item but Cheat Characters (no cheat owned) is live
    // Cheat Characters is live once the rider owns one (0x1577A0 > 0; PS2 lodge/runs/l12 after buying Brodi in Rewards)
    if (s === 'ctm-details') {
      const fe = this.ui.feScreens;
      return (
        (i === 2 && !this.ui.characterSelect?.canOpenCheats?.()) ||
        (i === 3 && !this.c.shop) ||
        (i === 5 && !fe?.kbLui) ||
        (i === 6 && !fe?.owns('fe-profile'))
      );
    }
    if (s === 'ctm-details') return ![0, 1, 3].includes(i) || (i === 3 && !this.c.shop);
    // radio modes: no in-game music playback in the browser port
    if (s === 'ctm-music') return i < 4;
    return false;
  }
  layout(s, i) {
    if (this.trophyLui(s)) return this.trophies.layout(s, i);
    if (this.saveLui(s)) return this.saveGame.layoutRect(i);
    if (s === 'ctm-details') return [190, Y(140) + i * Y(20), 320, Y(20)];
    if (s === 'ctm-rewards') return [210, Y(112) + i * Y(43), 300, Y(34)];
    if (s === 'ctm-reward-list') {
      const r = i - this.top;
      return r < 0 || r >= ROWS ? [0, -100, 1, 1] : [20, Y(120) + r * Y(24), 240, Y(24)];
    }
    if (['ctm-buy', 'ctm-gear-buy', 'ctm-uber-buy', 'ctm-song-buy'].includes(s)) return [260, Y(245) + i * Y(24), 120, Y(24)];
    if (s === 'ctm-gear' || s === 'ctm-songs') {
      const r = i - this.top;
      return r < 0 || r >= ROWS ? [0, -100, 1, 1] : [20, Y(120) + r * Y(24), 240, Y(24)];
    }
    if (s === 'ctm-uber') return [20, Y(140) + i * Y(20), 150, Y(20)];
    if (s === 'ctm-highlights') {
      const r = i - this.top;
      return r < 0 || r >= 3 ? null : [40, Y(120) + r * Y(70), 560, Y(66)];
    }
    if (s === 'ctm-uber-list') return [200, Y(104) + i * Y(20), 200, Y(20)];
    if (s === 'ctm-music') return [260, Y(140) + i * Y(20), 300, Y(20)];
    if (s === 'ctm-trophies') return [40, Y(150) + i * Y(30), 200, Y(28)];
    return [420, Y(366), 170, Y(24)];
  }
  choose(i) {
    const ui = this.ui,
      s = ui.screen;
    if (this.trophyLui(s)) return this.trophies.choose(i);
    if (this.saveLui(s)) return this.saveGame.choose(i);
    if (s === 'ctm-details') {
      const fe = ui.feScreens,
        back = (k) => () => {
          ui.set('ctm-details');
          ui.index = k;
          ui.sync();
        };
      // 0x1F4064: Ubertrick Setup = cFEStateUberTrick with buying, Player Name = the keyboard over this screen (0x1F45C4), Rider Profile =
      // 14rid_prof
      // the rewards room (128rewardsroom / 129rewardgallery) with buying (PS2 lodge/runs/l11)
      if (i === 0 && fe?.owns('fe-rewards')) {
        fe.openFromLodge('fe-rewards', back(0));
        return;
      }
      // 131cheat_char over this screen: the pick is the career rider's skin (setup slot +0x12)
      if (i === 2 && ui.characterSelect?.canOpenCheats?.()) {
        ui.characterSelect.openCheats();
        return;
      }
      if (i === 3 && fe?.owns('fe-uber')) {
        fe.openFromLodge('fe-uber', back(3));
        return;
      }
      if (i === 5 && fe?.kbLui) {
        fe.openKeyboard('name');
        return;
      }
      if (i === 6 && fe?.owns('fe-profile')) {
        fe.openFromLodge('fe-profile', back(6));
        return;
      }
    }
    // the LUI screens' changes (web/lui-flash.js)
    if (s === 'ctm-details') {
      if (i === 0) ui.set('ctm-rewards');
      else if (i === 1) this.cs.lodgeGo(() => ui.set('ctm-trophies'));
      else if (i === 3) {
        this.uberRow = 0;
        ui.set('ctm-uber');
      } else if (i === 4)
        this.cs.lodgeGo(() => {
          this.top = 0;
          ui.set('ctm-highlights');
        });
      return;
    }
    if (s === 'ctm-gear') {
      const e = this.gearList()[i];
      if (!e) return;
      if (e.flags & 0x20) {
        this.folders.push({ item: e.item, name: e.name, index: i });
        this.top = 0;
        ui.set('ctm-gear');
        return;
      }
      this.selected = e.item;
      this.selectedIndex = i;
      if (this.gearMode === 'buy') {
        if (this.c.gearStatus(this.id, e.item, this.peak)?.state === 'buy') this.cs.go('ctm-gear-buy', 1);
      } else {
        this.c.equipGear(this.id, e.item, !this.c.gear(this.id).equipped(e.item));
        ui.sync();
      }
      return;
    }
    if (s === 'ctm-gear-buy') {
      if (i === 0) this.c.buyGear(this.id, this.selected, this.peak);
      this.returnTo('ctm-gear', this.selectedIndex);
      return;
    }
    if (s === 'ctm-uber') {
      if (i === UBER_ROWS.length) {
        this.back();
        return;
      }
      this.uberRow = i;
      ui.set('ctm-uber-list');
      return;
    }
    if (s === 'ctm-uber-list') {
      const x = this.uberList()[i];
      if (!x) return;
      this.selected = x.index;
      this.selectedIndex = i;
      if (x.state === 'buy') this.cs.go('ctm-uber-buy', 1);
      else if (x.state === 'owned') {
        this.c.selectUber(this.id, this.uberCategory(), x.index);
        ui.sync();
      }
      return;
    }
    if (s === 'ctm-uber-buy') {
      if (i === 0) this.c.buyUber(this.id, this.uberCategory(), this.selected);
      this.returnTo('ctm-uber-list', this.selectedIndex);
      return;
    }
    if (s === 'ctm-music') {
      if (i === 4) {
        this.top = 0;
        ui.set('ctm-songs');
      }
      return;
    }
    if (s === 'ctm-songs') {
      this.selected = i;
      if (!this.c.songState(this.id).owned.includes(i) && this.songAffordable()) this.cs.go('ctm-song-buy', 1);
      return;
    }
    if (s === 'ctm-song-buy') {
      if (i === 0) this.c.buySong(this.id, this.selected);
      this.returnTo('ctm-songs', this.selected);
      return;
    }
    if (s === 'ctm-rewards') {
      this.category = i;
      this.top = 0;
      ui.set('ctm-reward-list');
      return;
    }
    // feReturn: opened from the FE Rider Details (web/fe-screens.js), view only
    if (s === 'ctm-reward-list') {
      const st = this.c.rewardStatus(this.id, this.section(), i, this.peak);
      this.selected = i;
      if (st.state === 'owned') ui.set('ctm-reward-view');
      else if (st.state === 'buy' && !this.feReturn) this.cs.go('ctm-buy', 1);
      return;
    }
    if (s === 'ctm-buy') {
      if (i === 0) this.c.buyReward(this.id, this.section(), this.selected, this.peak);
      ui.set('ctm-reward-list');
      ui.index = this.selected;
      this.scroll();
      ui.sync();
      return;
    }
    if (s === 'ctm-reward-view') {
      ui.set('ctm-reward-list');
      ui.index = this.selected;
      this.scroll();
      ui.sync();
      return;
    }
  }
  returnTo(screen, index) {
    this.ui.set(screen);
    this.ui.index = index;
    this.scroll();
    this.ui.sync();
  }
  back() {
    const ui = this.ui,
      s = ui.screen;
    if (this.trophyLui(s)) return this.trophies.back();
    if (this.saveLui(s)) return this.saveGame.back();
    if (s === 'ctm-gear') {
      if (this.folders.length) {
        const f = this.folders.pop();
        this.top = 0;
        this.returnTo('ctm-gear', f.index);
      } else
        this.cs.lodgeGo(() => {
          ui.set('ctm-lodge');
          ui.index = this.gearMode === 'buy' ? 2 : 1;
          ui.sync();
        });
      return;
    }
    if (s === 'ctm-gear-buy') {
      this.returnTo('ctm-gear', this.selectedIndex);
      return;
    }
    if (s === 'ctm-uber') {
      ui.set('ctm-details');
      ui.index = 3;
      ui.sync();
      return;
    }
    if (s === 'ctm-highlights') {
      this.cs.lodgeGo(() => {
        ui.set('ctm-details');
        ui.index = 4;
        ui.sync();
      });
      return;
    }
    if (s === 'ctm-uber-list') {
      ui.set('ctm-uber');
      ui.index = this.uberRow;
      ui.sync();
      return;
    }
    if (s === 'ctm-uber-buy') {
      this.returnTo('ctm-uber-list', this.selectedIndex);
      return;
    }
    if (s === 'ctm-music') {
      ui.set('ctm-lodge');
      ui.index = 5;
      ui.sync();
      return;
    }
    if (s === 'ctm-songs') {
      ui.set('ctm-music');
      ui.index = 4;
      ui.sync();
      return;
    }
    if (s === 'ctm-song-buy') {
      this.returnTo('ctm-songs', this.selected);
      return;
    }
    // the Player Name keyboard closes
    if (s === 'ctm-details' && this.ui.feScreens?.keyboard) {
      this.ui.feScreens.keyboard = null;
      ui.sync();
      return;
    }
    if (s === 'ctm-details') {
      this.cs.lodgeGo(() => {
        ui.set('ctm-lodge');
        ui.index = 4;
        ui.sync();
      });
      return;
    }
    if (s === 'ctm-rewards' && this.feReturn) {
      this.feReturn();
      return;
    }
    if (s === 'ctm-rewards' || s === 'ctm-trophies') {
      ui.set('ctm-details');
      ui.index = s === 'ctm-rewards' ? 0 : 1;
      ui.sync();
      return;
    }
    if (s === 'ctm-reward-list') {
      ui.set('ctm-rewards');
      ui.index = this.category;
      ui.sync();
      return;
    }
    if (s === 'ctm-reward-view' || s === 'ctm-buy') {
      ui.set('ctm-reward-list');
      ui.index = this.selected;
      this.scroll();
      ui.sync();
    }
  }
  openGear(mode) {
    // Equip Gear: the original 12equ_char screen with the rider wearing the outfit (web/wardrobe.js); Square = Buy Gear.
    const rider = mode === 'equip' && this.ui.riders?.find((r) => r.id === this.id);
    // entering, leaving and Square = Buy Gear are state changes (career-ui.js lodgeGo). Entering switches at the flash's
    // full white once the screen's own data is in (equipLoading: preloaded by preloadGear, the outfit then loads behind "Loading...";
    // before:
    // full white held until the outfit package was built)
    if (rider) {
      this.c._gear = null;
      return openEquipGear(this.ui, rider, {
        onExit: () =>
          this.cs.lodgeGo(() => {
            this.c._gear = null;
            this.ui.set('ctm-lodge');
            this.ui.index = 1;
            this.ui.sync();
          }),
        onBuy: () =>
          this.cs.lodgeGo(() => {
            this.c._gear = null;
            return this.openGear('buy');
          })
      }).catch((e) => console.error(e));
    }
    this.gearMode = mode;
    this.folders = [];
    this.top = 0;
    this.ui.set('ctm-gear');
  }
  // Equip Gear's screen data for the lodge's rider, loaded while the lodge menu is up (career-ui.js drawLodge)
  preloadGear() {
    if (this._gearPre === this.id) return;
    const rider = this.ui.riders?.find((r) => r.id === this.id);
    if (!rider) return;
    this._gearPre = this.id;
    preloadEquipGear(this.ui, rider).catch(() => {
      this._gearPre = null;
    });
  }
  gearList() {
    const inv = this.c.gear(this.id);
    if (!inv) return [];
    const f = this.folders.at(-1)?.item ?? -1;
    return this.gearMode === 'buy' ? inv.buyList(f, this.peak) : inv.equipList(f);
  }
  // Career Highlights (0x1F5DA0 / 0x1F5F80; PS2 frame local/ps2-capture/menus/lodge/30-career-highlights.png): the 24 monster tricks in
  // list order 0x441B40, three rows a page (hlsec%da, checkbox%d). Row p = highlight tier p % 3 + 1 of stat p / 3 (CMNAMER label, e.g.
  // "Stay on a rail - 25m"), checked when medal[p / 3] > p % 3 (0x155390); then "<name> Monster Trick" (0x46F2A8 with the table-0x43D320
  // name, trailing space included) and the trick (116E08) or kT_FEUnlockMonsterTrick.
  static HIGHLIGHT_LABELS = [0x07240c10, 0x07350090, 0x0d13c770, 0x0b07d700, 0x0513d7d0, 0x0d487730, 0x064f7060, 0x0ba3ad20];
  highlights() {
    const medals = this.c.monster(this.id).medals;
    return MONSTER_LIST.map((id, p) => {
      const m = monsterById(id),
        on = p % 3 < (medals[Math.floor(p / 3)] ?? 0);
      return {
        id,
        on,
        label: this.t(LodgeScreens.HIGHLIGHT_LABELS[Math.floor(p / 3)] + (p % 3) + 1, ''),
        // 0x46F2A8
        title: format('%s Monster Trick', m.name + ' '),
        text: on ? m.trick : this.t(0x0666f1fb, 'Complete highlight to unlock description')
      };
    });
  }
  // Drawn from the original 35car_stat LUI (web/career-highlights.js); the list above only until its data has loaded.
  drawHighlights(c, b) {
    const ui = this.ui,
      cs = this.cs;
    if (ui.index < this.top) this.top = ui.index;
    if (ui.index >= this.top + 3) this.top = ui.index - 2;
    if (!this.hl && !this.hlLoading) {
      this.hlLoading = (async () => {
        const data = await (await fetch('/assets/UI/career-highlights.json')).json();
        for (const p of HIGHLIGHT_PAGES)
          if (!ui.images[p]) {
            const im = new Image();
            im.src = '/assets/UI/' + p + '.png';
            await im.decode();
            ui.images[p] = im;
          }
        this.hl = new CareerHighlights(data, ui.images, ui);
      })().catch((e) => console.warn('Career highlights', e));
    }
    if (this.hl) {
      b.fillStyle = '#75a9cb';
      b.fillRect(0, 0, 640, 448);
      c.save();
      c.scale(1, 448 / 480);
      this.hl.draw(c, this.c.monster(this.id).medals, this.top);
      c.restore();
      return;
    }
    cs.feFrame(c, b, this.t(0x00f1dfa3, 'Career Highlights'));
    const list = this.highlights();
    for (let r = 0; r < 3; r++) {
      const k = this.top + r,
        h = list[k];
      if (!h) break;
      const y = Y(152) + r * Y(70),
        col = '#f4f7f8';
      c.fillStyle = '#f4f7f8';
      c.fillRect(76, y + Y(4), 14, Y(14));
      ui.text(c, h.label, 100, y, 17, col, 'FEFONT', 'left');
      ui.text(c, h.title, 107, y + Y(26), 15, col, 'FEFONT', 'left');
      ui.text(c, h.text, 107, y + Y(47), 15, col, 'FEFONT', 'left');
    }
    cs.help(c, this.t(0x098fa4b3, "Highlights of your rider's events."), [['triangle', 'Previous']]);
  }
  uberCategory() {
    return UBER_ROWS[this.uberRow][1];
  }
  uberList() {
    const cat = this.uberCategory();
    return uberEntries(this.c.shop, cat)
      .map((e, index) => ({ index, ...this.c.uberStatus(this.id, cat, index) }))
      .filter((x) => x.entry);
  }
  songAffordable() {
    const st = this.c.songState(this.id);
    return st.owned.length < SONG_FREE_CREDITS || this.c.rider(this.id).cash >= SONG_PRICE;
  }
  scroll() {
    const i = this.ui.index;
    if (i < this.top) this.top = i;
    if (i >= this.top + ROWS) this.top = i - ROWS + 1;
  }
  key(e) {
    if (this.trophyLui(this.ui.screen)) return this.trophies.key(e);
    if (this.saveLui(this.ui.screen)) return this.saveGame.key(e);
    // Player Name
    if (this.ui.screen === 'ctm-details' && this.ui.feScreens?.keyboard) return this.ui.feScreens.keyboardKey(e);
    // the cheat list
    if (this.ui.characterSelect?.overlay?.(this.ui.screen)) return this.ui.characterSelect.key(e);
    // Up / Down skip the greyed Cheat Characters and wrap, as the PS2 menu (lodge/runs/l1: Trophies -> Ubertrick Setup)
    if (this.ui.screen === 'ctm-details' && (e.code === 'ArrowUp' || e.code === 'ArrowDown')) {
      e.preventDefault();
      {
        const dis = this.items('ctm-details').map((_, i) => this.disabled('ctm-details', i));
        this.ui.index = stepMenu(this.ui.index, e.code === 'ArrowUp' ? -1 : 1, dis);
        this.ui.sync();
      }
      return true;
    }
    // Square (Shift) switches Buy Gear <-> Equip Gear, as the PS2 '□ Equip Gear' button.
    if (this.ui.screen === 'ctm-gear' && (e.code === 'ShiftLeft' || e.code === 'ShiftRight')) {
      this.openGear(this.gearMode === 'buy' ? 'equip' : 'buy');
      return true;
    }
    if (['ctm-reward-list', 'ctm-gear', 'ctm-songs'].includes(this.ui.screen) && ['ArrowUp', 'ArrowDown'].includes(e.code)) {
      setTimeout(() => {
        this.scroll();
        this.ui.sync();
      });
    }
    return false;
  }
  help(s) {
    if (s === 'ctm-details')
      return (
        [this.t(0x0e3fa653, 'Buy and view rewards here.'), this.t('kT_HELPViewTrophies'), '', 'Buy and set up ubertricks.', '', '', ''][
          this.ui.index
        ] || ''
      );
    if (s === 'ctm-rewards') return this.t(0x0a4770a4, 'Select a reward to view.');
    if (s === 'ctm-reward-list') {
      const st = this.c.rewardStatus(this.id, this.section(), this.ui.index, this.peak);
      if (this.feReturn && st && st.state !== 'owned') return this.t(0x0b6490ed, 'Buy this item in Conquer the Mountain mode.');
      return st ? this.t(st.help, '') : '';
    }
    if (s === 'ctm-trophies') return this.t('kT_HELPViewTrophies');
    if (s === 'ctm-gear') {
      const e = this.gearList()[this.ui.index];
      if (!e) return this.gearMode === 'buy' ? 'No items available.' : this.t('kT_HELPNoItemsCTM');
      if (e.flags & 0x20) return this.t('kT_FEHELPChooseCatContinue');
      if (this.gearMode === 'equip')
        return this.c.gear(this.id).equipped(e.item)
          ? this.t(0x05ae1a82, 'This item is equipped.')
          : this.t(0x0ae0875c, 'This item is available to equip.');
      return this.t(this.c.gearStatus(this.id, e.item, this.peak)?.help || '');
    }
    if (s === 'ctm-uber' || s === 'ctm-uber-list') {
      const x = s === 'ctm-uber-list' && this.uberList()[this.ui.index];
      if (!x) return 'Choose an ubertrick category.';
      return x.state === 'buy' ? this.t('kT_FEHELPTrickAvailBuy') : x.state === 'short' ? this.t('kT_OVRCMNSaveCashUber') : '';
    }
    if (s === 'ctm-music')
      return this.ui.index === 4
        ? 'Buy songs and edit your playlist. (In-game music playback is not in the browser port.)'
        : this.t('kT_HELPAUDIORadioBig');
    if (s === 'ctm-songs') {
      const x = this.c.songs()[this.ui.index];
      return x ? `${x.artist} - ${x.album}` : '';
    }
    if (s === 'ctm-highlights') return this.t(0x098fa4b3, "Highlights of your rider's events.");
    return '';
  }
  draw(c, b) {
    const ui = this.ui,
      s = ui.screen,
      cs = this.cs;
    if (this.trophyLui(s) && this.trophies.draw(c, b)) return;
    if (this.saveLui(s) && this.saveGame.draw(c, b)) return;
    if (s === 'ctm-buy') {
      this.drawList(c, b);
      cs.dialog(c, [this.t('kT_OVRCMNBuyItem', 'Buy item?')]);
      return;
    }
    if (s === 'ctm-gear-buy') {
      this.drawGear(c, b, true);
      cs.dialog(c, [this.t('kT_OVRCMNBuyItem', 'Buy item?')]);
      return;
    }
    if (s === 'ctm-gear') {
      this.drawGear(c, b);
      return;
    }
    if (s === 'ctm-uber-buy') {
      this.drawUber(c, b, true);
      cs.dialog(c, [this.t('kT_OVRCMNBuyThisUber', 'Buy this ubertrick?')]);
      return;
    }
    if (s === 'ctm-uber' || s === 'ctm-uber-list') {
      this.drawUber(c, b);
      return;
    }
    if (s === 'ctm-song-buy') {
      this.drawSongs(c, b, true);
      cs.dialog(c, [
        this.c.songState(this.id).owned.length < SONG_FREE_CREDITS
          ? this.t('kT_OVRCMNBuySongCredit', 'Buy song using free song credit?')
          : this.t('kT_16BuyMusicTrack', 'Buy song?')
      ]);
      return;
    }
    if (s === 'ctm-songs') {
      this.drawSongs(c, b);
      return;
    }
    if (s === 'ctm-highlights') {
      this.drawHighlights(c, b);
      return;
    }
    if (s === 'ctm-music') {
      cs.feFrame(c, b, this.t(0x055addb3, 'Music'));
      ui.items().forEach((t, i) => {
        const y = Y(140) + i * Y(20);
        if (ui.index === i) {
          c.fillStyle = '#b45410';
          c.fillRect(260, y, 300, Y(20));
        }
        if (i < 4) {
          c.fillStyle = '#f4f7f8';
          c.fillRect(270, y + 3, 12, 12);
        }
        ui.text(c, t, 296, y + 1, 17, ui.index === i ? '#f4f6f2' : this.disabled(s, i) ? '#6d8ea4' : '#1a2a36');
      });
      cs.help(c, this.help(s), [
        ['cross', 'Select'],
        ['triangle', 'Previous']
      ]);
      return;
    }
    const title = {
      'ctm-details': 'Rider Details',
      'ctm-rewards': this.t(0x01d16b73, 'Rewards'),
      'ctm-reward-list': this.t(REWARD_CATEGORIES[this.category][1], REWARD_CATEGORIES[this.category][2]),
      'ctm-reward-view': this.t(REWARD_CATEGORIES[this.category][1], REWARD_CATEGORIES[this.category][2]),
      'ctm-trophies': this.t('kT_TITLETrophyRoom', 'Trophies')
    }[s];
    // 155rider_details_conquer (web/fe-screens.js)
    if (s === 'ctm-details' && ui.feScreens?.drawLodgeDetails(c, b, (i) => this.disabled(s, i))) {
      const fe = ui.feScreens;
      if (fe.keyboard) {
        c.save();
        c.scale(1, 448 / 480);
        fe.drawKeyboard(c, fe.now());
        c.restore();
      }
      ui.characterSelect?.drawOverlay?.(c);
      return;
    }
    cs.feFrame(c, b, title);
    if (s === 'ctm-details') {
      ui.text(c, ui.rider?.name || 'Sam', 380, Y(102), 22, '#1a2a36', 'FEFONT', 'center');
      ui.items().forEach((t, i) => {
        const y = Y(140) + i * Y(20);
        if (ui.index === i) {
          c.fillStyle = '#b45410';
          c.fillRect(190, y, 320, Y(20));
        }
        ui.text(c, t, 500, y + 1, 17, ui.index === i ? '#f4f6f2' : this.disabled(s, i) ? '#6d8ea4' : '#1a2a36', 'FEFONT', 'right');
      });
    } else if (s === 'ctm-rewards') {
      REWARD_CATEGORIES.forEach(([key], i) => {
        const y = Y(112) + i * Y(43),
          all = this.c.rewardItems(key).length,
          have = this.c.owned(this.id, key).length;
        c.fillStyle = ui.index === i ? '#b45410' : '#dfe8ee';
        c.fillRect(210, y, 305, Y(34));
        ui.text(c, ui.items()[i], 220, y + 6, 17, ui.index === i ? '#f4f6f2' : '#1a2a36');
        ui.text(c, `${have} / ${all}`, 505, y + 6, 17, ui.index === i ? '#f4f6f2' : '#1a2a36', 'FEFONT', 'right');
      });
    } else if (s === 'ctm-reward-list' || s === 'ctm-reward-view') {
      if (s === 'ctm-reward-view') {
        const item = this.c.rewardItems(this.section())[this.selected];
        const im = item?.picture && cs.picture('REWARDS/' + item.picture);
        if (im) {
          const w = im.naturalWidth >= im.naturalHeight ? 560 : 300,
            h = (w * im.naturalHeight) / im.naturalWidth;
          b.drawImage(im, 320 - w / 2, Y(250) - h / 2, w, h);
        } else ui.text(c, item?.name || '', 320, Y(220), 20, '#1a2a36', 'FEFONT', 'center');
        ui.text(c, item?.name || '', 320, Y(96), 18, '#1a2a36', 'FEFONT', 'center');
      } else this.drawList(c, b);
    } else if (s === 'ctm-trophies') {
      const trophies = this.c.rewardItems('trophy');
      ui.items().forEach((t, i) => {
        const y = Y(150) + i * Y(30);
        if (ui.index === i) {
          c.fillStyle = '#b45410';
          c.fillRect(40, y, 200, Y(28));
        }
        ui.text(c, t, 230, y + 3, 18, ui.index === i ? '#f4f6f2' : '#1a2a36', 'FEFONT', 'right');
      });
      const p = ui.index + 1;
      ['race', 'freestyle', 'freeride', 'earnings'].forEach((goal, k) => {
        // RWRDPS2 trophies: race, freestyle, explore, earnings x peaks 1..3
        const tr = trophies[k * 3 + p - 1],
          done = this.c.goalComplete(this.id, p, goal);
        const x = 270 + (k % 2) * 170,
          y = Y(120) + Math.floor(k / 2) * Y(130);
        const im = done && tr?.picture && cs.picture('REWARDS/' + tr.picture);
        if (im) b.drawImage(im, x, y, 120, Y(120));
        else {
          b.fillStyle = 'rgba(30,60,80,.35)';
          b.fillRect(x, y, 120, Y(120));
        }
        ui.text(c, tr?.name?.replace(/^Peak \d /, '') || goal, x + 60, y + Y(122), 12, '#1a2a36', 'FEFONT', 'center');
      });
    }
    cs.help(c, this.help(s), [
      ['cross', 'Select'],
      ['triangle', 'Previous']
    ]);
  }
  drawGear(c, b, dialog = false) {
    const ui = this.ui,
      cs = this.cs,
      inv = this.c.gear(this.id),
      list = this.gearList(),
      r = this.c.rider(this.id),
      idx = dialog ? this.selectedIndex : ui.index;
    cs.feFrame(c, b, this.gearMode === 'buy' ? this.t('kT_CMNBuyGear', 'Buy Gear') : this.t('kT_CMNEquipGear', 'Equip Gear'));
    ui.text(c, this.folders.at(-1)?.name || this.t('kT_FECategories', 'Categories'), 236, Y(80), 19, '#f4f6f2', 'FEFONT', 'right');
    if (this.gearMode === 'buy') {
      ui.text(c, `${this.t(0x0918c5a5, 'You have:')} $ ${r.cash}`, 380, Y(70), 15, '#1a2a36');
      const e = list[idx];
      ui.text(c, `${this.t(0x05f1a304, 'Cost:')}${e && !(e.flags & 0x20) ? ' $ ' + inv.price(e.item) : ''}`, 380, Y(90), 15, '#1a2a36');
    }
    for (let k = 0; k < ROWS && this.top + k < list.length; k++) {
      const i = this.top + k,
        e = list[i],
        y = Y(120) + k * Y(24);
      if (idx === i) {
        c.fillStyle = '#b45410';
        c.fillRect(20, y, 240, Y(22));
      }
      ui.text(c, e.name, 236, y + 2, 15, idx === i ? '#f4f6f2' : '#1a2a36', 'FEFONT', 'right');
      if (!(e.flags & 0x20)) {
        if (this.gearMode === 'equip' && inv.equipped(e.item)) cs.check(c, 242, y + 4);
        else if (this.gearMode === 'buy') ui.sprite('FE_1-11', 57, 3, 17, 16, 242, y + 4, 13, 13);
      }
    }
    cs.help(c, this.help('ctm-gear'), [
      ['cross', 'Select'],
      ['triangle', 'Previous'],
      ['square', this.gearMode === 'buy' ? this.t('kT_CMNEquipGear', 'Equip Gear') : this.t('kT_CMNBuyGear', 'Buy Gear')]
    ]);
  }
  drawUber(c, b, dialog = false) {
    const ui = this.ui,
      cs = this.cs,
      onList = ui.screen !== 'ctm-uber',
      rowIdx = onList ? this.uberRow : ui.index,
      list = (() => {
        const keep = this.uberRow;
        this.uberRow = Math.min(rowIdx, UBER_ROWS.length - 1);
        const l = this.uberList();
        if (!onList) this.uberRow = keep;
        return l;
      })();
    cs.feFrame(c, b, 'Ubertrick Setup');
    [...UBER_ROWS.map(([n]) => n), 'DONE'].forEach((t, i) => {
      const y = Y(140) + i * Y(20);
      if (rowIdx === i) {
        c.fillStyle = '#b45410';
        c.fillRect(20, y, 150, Y(20));
      }
      ui.text(c, t, 160, y + 1, 17, rowIdx === i ? '#f4f6f2' : '#1a2a36', 'FEFONT', 'right');
    });
    b.fillStyle = '#2e6a95';
    b.fillRect(188, Y(96), 420, Y(170));
    const cur = dialog ? this.selectedIndex : ui.index;
    list.forEach((x, i) => {
      const y = Y(104) + i * Y(20);
      if (onList && cur === i) {
        c.fillStyle = '#b45410';
        c.fillRect(200, y, 200, Y(20));
      }
      if (x.state === 'selected') cs.check(c, 212, y + 3);
      else if (x.state === 'buy' || x.state === 'short') ui.sprite('FE_1-11', 57, 3, 17, 16, 210, y + 2, 14, 14);
      else {
        c.fillStyle = '#f4f7f8';
        c.fillRect(212, y + 3, 12, 12);
      }
      ui.text(c, x.entry.name, 232, y + 1, 16, onList && cur === i ? '#f4f6f2' : '#eef4f7');
      if (x.state === 'buy' || x.state === 'short') ui.text(c, `$ ${x.entry.price}`, 590, y + 1, 15, '#eef4f7', 'FEFONT', 'right');
    });
    ui.text(c, `${this.t(0x0918c5a5, 'You have:')} $ ${this.c.rider(this.id).cash}`, 590, Y(282), 15, '#1a2a36', 'FEFONT', 'right');
    cs.help(c, this.help(ui.screen), [
      ['cross', 'Select'],
      ['triangle', 'Previous']
    ]);
  }
  drawSongs(c, b, dialog = false) {
    const ui = this.ui,
      cs = this.cs,
      songs = this.c.songs(),
      st = this.c.songState(this.id),
      idx = dialog ? this.selected : ui.index;
    cs.feFrame(c, b, this.t(0x063cab64, 'Edit Playlist'));
    const credits = Math.max(0, SONG_FREE_CREDITS - st.owned.length);
    ui.text(
      c,
      credits ? `Free song credits: ${credits}` : `${this.t(0x0918c5a5, 'You have:')} $ ${this.c.rider(this.id).cash}`,
      600,
      Y(92),
      16,
      '#1a2a36',
      'FEFONT',
      'right'
    );
    for (let k = 0; k < ROWS && this.top + k < songs.length; k++) {
      const i = this.top + k,
        y = Y(120) + k * Y(24),
        own = st.owned.includes(i);
      if (idx === i) {
        c.fillStyle = '#b45410';
        c.fillRect(20, y, 470, Y(22));
      }
      if (own) cs.check(c, 26, y + 4);
      else ui.sprite('FE_1-11', 57, 3, 17, 16, 24, y + 4, 13, 13);
      ui.text(c, songs[i].title, 46, y + 2, 14, idx === i ? '#f4f6f2' : '#1a2a36');
      if (!own & !credits) ui.text(c, `$ ${SONG_PRICE}`, 600, y + 2, 14, '#1a2a36', 'FEFONT', 'right');
    }
    cs.help(c, this.help('ctm-songs'), [
      ['cross', this.t('kT_16BuyMusicTracks', 'Buy song')],
      ['triangle', 'Previous']
    ]);
  }
  drawList(c, b) {
    const ui = this.ui,
      cs = this.cs,
      section = this.section(),
      items = this.c.rewardItems(section),
      r = this.c.rider(this.id);
    ui.text(c, `${this.t(0x0918c5a5, 'You have:')} $ ${r.cash}`, 600, Y(92), 17, '#1a2a36', 'FEFONT', 'right');
    const st = this.c.rewardStatus(
      this.id,
      section,
      this.selected != null && ui.screen === 'ctm-buy' ? this.selected : ui.index,
      this.peak
    );
    if (st?.item.price) ui.text(c, `${this.t(0x05f1a304, 'Cost:')} $ ${st.item.price}`, 600, Y(112), 17, '#1a2a36', 'FEFONT', 'right');
    for (let r0 = 0; r0 < ROWS && this.top + r0 < items.length; r0++) {
      const i = this.top + r0,
        y = Y(120) + r0 * Y(24),
        s = this.c.rewardStatus(this.id, section, i, this.peak);
      if (ui.index === i) {
        c.fillStyle = '#b45410';
        c.fillRect(20, y, 240, Y(22));
      }
      ui.text(
        c,
        items[i].name,
        236,
        y + 2,
        14,
        ui.index === i ? '#f4f6f2' : s.state === 'owned' ? '#1a2a36' : '#4a6a80',
        'FEFONT',
        'right'
      );
      if (s.state === 'owned') cs.check(c, 242, y + 3);
      else if (['buy', 'short', 'elsewhere'].includes(s.state)) ui.sprite('FE_1-11', 57, 3, 17, 16, 242, y + 3, 13, 13);
    }
    // preview panel: owned pictures only (the item must be bought before it can be viewed)
    const item = st?.item;
    if (item) {
      b.fillStyle = '#2e6a95';
      b.fillRect(280, Y(130), 330, Y(230));
      const im = st.state === 'owned' && item.picture && cs.picture('REWARDS/' + item.picture);
      if (im) {
        const w = 320,
          h = Math.min(Y(220), (w * im.naturalHeight) / im.naturalWidth);
        b.drawImage(im, 285, Y(135), w, h);
      } else
        ui.text(c, st.state === 'buy' || st.state === 'short' ? `$ ${item.price}` : '?', 445, Y(230), 26, '#eef4f7', 'FEFONT', 'center');
    }
  }
}
