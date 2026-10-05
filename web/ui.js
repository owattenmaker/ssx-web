import { raceTime, hudRaceTime } from './race-time.mjs';
import { drawRacePlace } from './race-place-hud.js';
// the collectible counter during a career-race collect
import { COLLECTIBLE_TOTALS, drawCollectCounter } from './free-ride-hud.js';
import { boostLetters } from './boost-letters.js';
import { BoostGauge } from './boost-gauge.js';
import { TrickHud, TrickHudRenderer } from './trick-hud.js';
import { pv } from './pv-flags.js';
import { ps2MenuStep } from './menu-rules.js';
import { createPauseContexts } from './pause-contexts.js';
import { createScreenPhases } from './screen-phases.js';
import { CareerScreens } from './career-ui.js';
import { LoadingScreen } from './loading-screen.js';
import { LuiScreen } from './lui-player.js';
import { SPRITE_2D, glyphSource } from './sprite-canvas.js';
// lodge load screens 117/118 (docs/cutscenes.md)
import { TransitionScreens } from './transition-screens.js';
import { CharacterSelect } from './character-select.js';
import { FeScreens } from './fe-screens.js';
// Single Event: Select Peak / Mode / Event (FE.LUI Map)
import { FeEventSelect } from './fe-event-select.js';
// Main Menu > Previews: the EA trailers (146Bonusmat)
import { FePreviews } from './fe-previews.js';
// Main Menu drawn from 07main_men
import { FeMainMenu } from './fe-main-menu.js';
// Options > Save/Load (25saveload) + save file export/import
import { FeSaveLoad } from './fe-saveload.js';
import { loadSelection, saveSelection } from './save-store.js';
// software 2D canvas for the UI layers (see the constructor); ?uicanvas=gpu = the accelerated path, for comparisons
export const UI_CANVAS = /[?&]uicanvas=gpu\b/.test(globalThis.location?.search ?? '') ? {} : { willReadFrequently: true };
import { AudioMenus } from './audio-menu.js';
import { WIDESCREEN_MODES, loadWidescreen, saveWidescreen } from './widescreen.js';
import { KEYBOARD_MODES, loadKeyboardMode } from './pad-input.js';
// keyboard play: PS2 button icons -> key caps (docs/input-glyphs.md)
import { drawGlyphAsKey, inputDevice, keyFor } from './input-glyphs.js';
import { downloadProgress } from './downloads.js';
// the boot / attract movies (attract, bootMovies; docs/intro-movies.md)
import { FeAttract } from './fe-attract.js';
// The title card's own "Loading..." line carries the game-data download (web/downloads.js), in the card's font.
const titleLoading = (text) => {
  if (text !== 'Loading...') return text;
  const d = downloadProgress();
  return d.active && d.expected > 1048576 ? `Loading... ${Math.floor(d.fraction * 100)}%` : text;
};

// Menu hover (sync() buttons): the item under the mouse is selected only once the mouse has really moved since the last menu rebuild or key
// press. sync() rebuilds the buttons after every keyboard / pad move, and the browser reports the new button under a resting cursor as
// entered, which used to snap the selection back to wherever the mouse sat.
let mouseMoved = false;
if (typeof addEventListener === 'function') {
  addEventListener(
    'pointermove',
    (e) => {
      if (e.pointerType === 'mouse' && (e.movementX || e.movementY)) mouseMoved = true;
    },
    { capture: true, passive: true }
  );
  addEventListener(
    'keydown',
    () => {
      mouseMoved = false;
    },
    { capture: true }
  );
}
// Original UI atlases and bitmap fonts extracted from the user's PS2 disc.
const root = '/assets/UI/';
// Main Menu (07main_men): Single Event / Conquer The Mountain / Multi Play / Previews / Online and each item's help text. Multi Play (local
// split screen, 86multiplayermode) is not ported and stays greyed like on the PS2 frame (menus/ctm/01-main-menu.png greys Multi Play and
// Online: no network adapter there).
export const MAIN_HELP = [
  // the sixth, pv onlineRecords: web/fe-main-menu.js LEADERBOARDS_HELP
  'Play any unlocked Single Event.',
  'Earn medals or play BIG Challenges to unlock peaks and build your character.',
  '2 player head to head competition.',
  'View trailers of other EA games.',
  'Compete against SSX 3 players around the world.',
  'See the best online times and scores of every event, and watch their replays.'
];
export function mainDisabled({ career = false, previews = false, online = false } = {}) {
  return [false, !career, true, !previews, !online, ...(pv('onlineRecords') ? [false] : [])];
}
// Game data streaming while a menu is up (a rider model, a movie...): a small "Loading" with cycling dots in the front end's own font and
// help-text colour, bottom right like the original's legend text; nothing on the loading screen (its percentage shows it), the title card,
// in a race or a cutscene, or for quick cache hits.
export function drawStreamingNote(ui) {
  // FE whitefade started on a screen another module draws (Square / the deck's ≡ on Main Menu, Select Character, a pause menu): drawn after
  // the frame's UI, then the screen change (web/fe-screens.js)
  ui?.feScreens?.drawForeignFlash?.(ui.fg);
  const s = ui?.screen;
  if (!ui?.fg || !ui.fonts?.FEFONT || ['loading', 'title', 'game', 'cutscene', 'pause'].includes(s)) return;
  const d = downloadProgress();
  if (!d.active || d.busyMs < 600 || d.expected < 262144) return;
  const dots = '.'.repeat(1 + (Math.floor(performance.now() / 400) % 3));
  ui.text(ui.fg, 'Loading' + dots, 604, 436, 12, '#132834', 'FEFONT', 'right');
}
export class OriginalUI {
  constructor(callbacks) {
    // the pause context stack (web/pause-contexts.js, 0x5366E8) and the screens' phase machine (web/screen-phases.js, 0x39ECB0); its PS2
    // frame rules with ps2MenuInput. Dev (?qa) warns about a context left open past its owner screen
    this.cb = callbacks;
    {
      const dev = !!import.meta.env?.DEV || /[?&]qa\b/.test(globalThis.location?.search ?? '');
      this.contexts = createPauseContexts({
        warn: (m) => {
          if (dev) console.warn(m);
        }
      });
      this.phases = createScreenPhases({ contexts: () => this.contexts });
    }
    this.screen = 'title';
    this.index = 0;
    this.ready = false;
    this.images = {};
    this.fonts = {};
    this.tints = new Map();
    // UI_CANVAS: Safari's GPU 2D canvas draws each image as two triangles and shows a faint diagonal seam across scaled / translucent
    // sprites (the attribute boxes, panels); the software path draws them whole. 640x448 is cheap on the CPU.
    this.fg = document.querySelector('#ui').getContext('2d', UI_CANVAS);
    this.bg = document.querySelector('#ui-bg').getContext('2d', UI_CANVAS);
    this.nav = document.querySelector('#game-menu');
    this.error = 'Loading...';
    this.lastState = {};
    this.riders = [];
    this.riderIndex = 0;
    this.cameraView = 0x3d;
    this.widescreen = loadWidescreen();
    this.keyboardMode = loadKeyboardMode();
    this.stage = document.querySelector('#stage');
    this.stage.dataset.screen = this.screen;
    this.careerUI = new CareerScreens(this);
    this.loading = new LoadingScreen(this);
    this.transitions = new TransitionScreens(this);
    this.characterSelect = new CharacterSelect(this);
    this.feScreens = new FeScreens(this);
    // web/fe-screens.js: Setup Character, Rider Details, Options, Load game
    this.eventSelect = new FeEventSelect(this);
    this.previews = new FePreviews(this);
    this.mainMenu = new FeMainMenu(this);
    this.saveLoad = new FeSaveLoad(this);
    this.audioMenus = new AudioMenus(this);
    // web/audio-menu.js: Music, Edit Playlist, Sound Options, in-game Audio
    this.attract = new FeAttract(this);

    // Only a screen's phase 5 takes input (web/screen-phases.js; ps2MenuInput: the intro locks and outros of web/menu-rules.js): until
    // then no key reaches the menus, their sounds or main.js (registered first, capture phase)
    // preKey: the menu sounds' before-state (web/audio-menu.js watchSounds), taken before any other listener
    addEventListener(
      'keydown',
      (e) => {
        if (this.screen !== 'game' && !this.phases.accepts()) {
          e.preventDefault();
          e.stopImmediatePropagation();
          return;
        }
        try {
          this.preKey?.(e);
        } catch {}
      },
      true
    );

    // the full replay (web/replay-ui.js) reads its own keys, and Circle's release
    addEventListener('keyup', (e) => {
      if (this.replayUi?.owns(this.screen)) this.replayUi.keyup(e);
    });
    addEventListener('keydown', (e) => {
      if (this.replayUi?.owns(this.screen)) {
        if (this.replayUi.key(e)) e.preventDefault();
        return;
      }
      // cutscene: Cross skips (web/cutscenes.js)
      if (this.screen === 'game' || this.screen === 'loading' || this.screen === 'cutscene' || this.screen === 'transition') return;
      if (this.careerUI.owns(this.screen) && this.careerUI.key(e)) return;
      if (this.audioMenus.key(e)) return;
      // web/character-select.js: Select Character, cheat list
      if (this.characterSelect.key(e)) return;
      if (this.eventSelect.key(e)) return;
      if (this.previews?.key(e)) return;
      // web/fe-previews.js
      if (this.saveLoad?.key(e)) return;
      // Square: Options (PS2 main menu legend)
      if (
        this.screen === 'main' &&
        this.ready &&
        !e.repeat &&
        (e.code === 'ShiftLeft' || e.code === 'ShiftRight') &&
        this.feScreens.ready
      ) {
        e.preventDefault();
        this.feScreens.openOptions('main');
        return;
      }
      if (this.feScreens.key(e)) return;
      if (this.screen === 'options' && ['ArrowLeft', 'ArrowRight'].includes(e.code)) {
        e.preventDefault();
        if (!e.repeat && this.index === 0) this.cycleCamera(e.code === 'ArrowLeft' ? -1 : 1);
        if (!e.repeat && this.index === 1) this.cycleWidescreen(e.code === 'ArrowLeft' ? -1 : 1);
        if (!e.repeat && this.index === 2) this.cycleKeyboard();
        return;
      }
      if (this.screen === 'character' && ['ArrowLeft', 'ArrowRight'].includes(e.code)) {
        e.preventDefault();
        if (!e.repeat) this.cycleRider(e.code === 'ArrowLeft' ? -1 : 1);
        return;
      }
      if (['ArrowUp', 'ArrowDown', 'Enter', 'Space', 'Escape'].includes(e.code)) e.preventDefault();
      if (e.repeat) return;
      if (this.screen === 'main' && (e.code === 'ArrowUp' || e.code === 'ArrowDown')) {
        const dis = this.mainDisabled(),
          n = dis.length,
          d = e.code === 'ArrowUp' ? -1 : 1;
        for (let k = 1; k <= n; k++) {
          const j = (((this.index + d * k) % n) + n) % n;
          if (!dis[j]) {
            this.index = j;
            break;
          }
        }
        // wraps and skips the greyed items (FE menus)
      } else if (
        (e.code === 'ArrowUp' || e.code === 'ArrowDown') &&
        ps2MenuStep(this.screen, this.index, this.items().length, e.code === 'ArrowUp' ? -1 : 1) != null
      ) {
        this.index = ps2MenuStep(this.screen, this.index, this.items().length, e.code === 'ArrowUp' ? -1 : 1);
        // the PS2's wrapping CTM menus (web/menu-rules.js)
      } else {
        if (e.code === 'ArrowUp') this.index = Math.max(0, this.index - 1);
        if (e.code === 'ArrowDown') this.index = Math.min(this.items().length - 1, this.index + 1);
      }
      if (e.code === 'Enter' || e.code === 'Space') this.choose(this.index);
      if (e.code === 'Escape') this.back();
      this.sync();
    });
  }
  async load() {
    // everything loads side by side (a first visit waits on the network, a repeat visit on one revalidation round trip per file); the
    // title's pictures are already decoded by the boot title (web/boot-screen.js)
    const boot = globalThis.ssxBoot,
      json = async (p) => (await fetch(p)).json();
    const [, feGlyphs, hudGlyphs, riders, gauge, trick] = await Promise.all([
      Promise.all(
        ['part-glow', 'FE_1-20', 'FE_1-7', 'FE_1-11', 'OV_1-3', 'OV_1-4', 'OV_1-7', 'OV_1-6', 'OV_1-2', 'FEFONT-0', 'HUDFONT-0'].map(
          async (name) => {
            const had = boot?.images?.[name];
            if (had) {
              this.images[name] = had;
              return;
            }
            const im = new Image();
            im.src = root + name + '.png';
            await im.decode();
            this.images[name] = im;
          }
        )
      ),
      json(root + 'FEFONT-glyphs.json'),
      json(root + 'HUDFONT-glyphs.json'),
      json('/assets/riders.json'),
      json(root + 'boost-gauge.json'),
      json(root + 'trick-hud.json')
    ]);
    this.fonts.FEFONT = feGlyphs;
    this.fonts.HUDFONT = hudGlyphs;
    this.riders = riders;
    this.boostGauge = new BoostGauge(gauge, this.images['OV_1-4'], this.images['part-glow']);
    // original trick-score HUD (trick-hud.js): score object message slots from the core
    this.trickHud = new TrickHud(trick, { FEFONT: this.fonts.FEFONT, HUDFONT: this.fonts.HUDFONT });
    this.trickHudRenderer = new TrickHudRenderer(this.images, this.trickHud.glyphs);
    // Select Character first for the screens that share its pages and snow loop
    await Promise.all([
      this.careerUI.load(),
      this.loading.load(),
      this.characterSelect
        .load()
        .then(() =>
          Promise.all([
            this.feScreens.load(),
            this.eventSelect.load(),
            this.previews?.load?.(),
            this.mainMenu.load(),
            this.saveLoad.load(),
            this.audioMenus.load()
          ])
        )
    ]);
    // main.js starts on its boot rider (zoeBoot: RIDER_ZOE, else RIDER_SAM)
    this.riderIndex = Math.max(
      0,
      this.riders.findIndex((r) => r.id === 'zoe')
    );
    this.sync();
  }
  // Event load (loading-screen.js): the original "Basic Controls" load screen, then next(); work = promises to await.
  loadEvent(next, work = []) {
    let framesDone;
    const frames = new Promise((r) => (framesDone = r));
    this.warmFramesDone = framesDone;
    const warm = this.cb.warmup?.();
    Promise.resolve(warm)
      .finally(framesDone)
      .catch(() => {});
    // then the event's intro cutscene (world state 10 -> 1, web/cutscenes.js): scripts and actor models load under the load screen, from
    // the end of the warm-up's frames (its GPU wait runs alongside, docs/firefox-load.md)
    const intro = this.cb.introPrepare
      ? frames
          .then(() => {
            this.loading.begin('intro');
            return this.cb.introPrepare();
          })
          .catch((e) => console.warn('Intro cutscene prep failed', e))
          .finally(() => this.loading.done('intro'))
      : null;
    this.loading.run(this.cb.intro ? () => this.cb.intro(next) : next, [...work, warm, intro].filter(Boolean));
    // pv loadMeter: the event load's stages (main.js cb.loadStages: the course still loading, the rider, the lineup, the warm-up), then
    // the intro's preparation
    const stages = this.cb.loadStages?.() ?? ['rider', 'lineup', 'warm'];
    this.loading.plan(intro ? [...stages, 'intro'] : stages);
  }
  get rider() {
    return this.riders[this.riderIndex] || { name: 'Sam', card: [], bio: '' };
  }
  // Pause Options camera select (0x1F8DF8): Near 0x3C / Mid 0x3D / Far 0x3E, session-wide.
  cycleCamera(direction) {
    const views = [0x3c, 0x3d, 0x3e];
    const next = views[(views.indexOf(this.cameraView) + direction + 3) % 3];
    this.cameraView = this.cb.camera ? this.cb.camera(next) : next;
    this.sync();
  }
  // Front-end Options "Widescreen" (profile 0x535610 bits 20..21 -> 0x228C08 -> 0x377950): Off / 16:9 / Anamorphic, persisted. Keyboard
  // layout (pad-input.js): Simple = arrows/WASD are the stick on the ground and the D-pad in the air; Classic = IJKL D-pad.
  cycleKeyboard() {
    this.keyboardMode = KEYBOARD_MODES[(KEYBOARD_MODES.indexOf(this.keyboardMode) + 1) % KEYBOARD_MODES.length];
    this.cb.keyboard?.(this.keyboardMode);
    this.sync();
  }
  cycleWidescreen(direction) {
    this.widescreen = (this.widescreen + direction + 3) % 3;
    saveWidescreen(this.widescreen);
    this.cb.widescreen?.(this.widescreen);
    this.sync();
  }
  async cycleRider(direction) {
    if (this.characterSelect.owns(this.screen)) return this.characterSelect.cycle(direction);
    if (!this.ready) return;
    const next = (this.riderIndex + direction + this.riders.length) % this.riders.length;
    try {
      await this.cb.rider(this.riders[next]);
      this.riderIndex = next;
      this.sync();
    } catch (error) {
      console.error(error);
    }
  }
  items() {
    if (this.careerUI?.owns(this.screen)) return this.careerUI.items(this.screen);
    if (this.mpUI?.owns(this.screen)) return this.mpUI.items(this.screen);
    if (this.characterSelect?.owns(this.screen)) return this.characterSelect.items();
    if (this.audioMenus?.owns(this.screen)) return this.audioMenus.items();
    if (this.eventSelect?.owns(this.screen)) return this.eventSelect.items();
    if (this.previews?.owns?.(this.screen)) return this.previews.items();
    if (this.saveLoad?.owns(this.screen)) return this.saveLoad.items();
    if (this.feScreens?.owns(this.screen)) return this.feScreens.items();
    return this.screen === 'title'
      ? ['Press START button']
      : this.screen === 'main'
        ? [
            'Single Event',
            'Conquer The Mountain',
            'Multi Play',
            'Previews',
            'Online',
            // pv onlineRecords: the added sixth row (web/fe-main-menu.js withLeaderboards)
            ...(pv('onlineRecords') ? ['Leaderboards'] : [])
          ]
        : this.screen === 'character'
          ? [this.rider.name]
          : this.screen === 'setup'
            ? ['Continue', 'Rider Details']
            : this.screen === 'event'
              ? (this.courses || [{ name: 'Snow Jam' }]).map((c) => c.name)
              : this.screen === 'pause'
                ? ['Return', 'Restart', 'Audio', 'Options', 'Quit']
                : this.screen === 'options'
                  ? [
                      'Camera 1',
                      'Widescreen',
                      'Keyboard',
                      ...(this.audioMenus?.ready ? ['Sound'] : []),
                      // the port's Display & Touch (quality tier, resolution, frame rate) for every player, keyboard and pad too
                      ...(this.feScreens?.extra?.owns?.('fe-display') ? ['Display & Touch'] : []),
                      'Return'
                    ]
                  : this.screen === 'details'
                    ? ['Previous', 'Cheat Characters']
                    : this.screen === 'results'
                      ? ['Next event', 'Restart', 'Replay', 'Records', 'Quit']
                      : [];
  }
  pdaPause() {
    return !!this.careerUI?.pda?.ready && !!this.careerUI.mcommFrame;
  }
  set(screen) {
    // The in-game Options is the PDA page 37beoptions (web/audio-menu.js); optionsReturn names the menu it goes back to
    if (screen === 'options' && this.audioMenus?.owns?.('pda-options')) screen = 'pda-options';
    // Single Event: the original Select Peak / Mode / Event (web/fe-event-select.js)
    if (screen === 'event' && this.eventSelect?.ready) screen = 'fe-peak';
    // free ride: Start opens MCOMM (overlay 3, 31paus_freeride)
    if (screen === 'pause' && this.careerUI?.active) screen = 'ctm-pause';
    else if (screen === 'pause' && this.careerUI?.freeRide) screen = 'ctm-mcomm';
    this.previousScreen = this.screen;
    this.screen = screen;
    // the new screen's phase 2, from the UI frame counter (web/screen-phases.js)
    if (screen !== this.previousScreen) this.phases.enter(screen, this.previousScreen);
    // the screen intro's input lockout (web/menu-rules.js)
    if (screen === 'character') this.characterSelect?.onEnter();
    this.feScreens?.enter(screen, this.previousScreen);
    this.eventSelect?.enter(screen, this.previousScreen);
    this.previews?.enter?.(screen, this.previousScreen);
    this.mainMenu?.enter(screen, this.previousScreen);
    this.saveLoad?.enter(screen, this.previousScreen);
    if (screen === 'character') this.restoreRider();
    // Cross on Select Character: remember the rider
    if (screen === 'setup' && this.previousScreen === 'character') this.rememberRider();
    this.audioMenus?.enter(screen, this.previousScreen);
    if (screen !== 'details' && this.characterSelect) this.characterSelect.cheatOpen = false;
    this.stage.dataset.screen = screen;
    this.index = screen === 'main' ? this.mainIndex || 0 : 0;
    // the main menu reopens on the item last chosen (PS2 ctm/caps reenter: back from CTM on Conquer The Mountain)
    this.sync();
  }
  sync() {
    mouseMoved = false;
    this.nav.replaceChildren();
    this.items().forEach((label, i) => {
      const b = document.createElement('button');
      b.textContent = label;
      b.setAttribute('aria-label', label);
      b.disabled =
        !this.ready ||
        (this.screen === 'main' && this.mainDisabled()[i]) ||
        (this.screen === 'pause' && ((i === 2 && !this.audioMenus?.ready) || (i === 1 && !!this.mpUI?.racing))) ||
        (this.screen === 'results' && i !== 1 && i !== 4 && !(i === 2 && this.cb.replayAvailable?.())) ||
        (this.screen === 'details' && i === 1 && !this.characterSelect?.canOpenCheats());
      let x = 282,
        y = 154 + i * 23,
        w = 315,
        h = 24;
      if (this.screen === 'pause' || this.screen === 'options') {
        x = 198;
        y = 113 + i * 37;
        w = 260;
        h = 32;
      }
      if (this.screen === 'pause' && this.pdaPause()) [x, y, w, h] = this.careerUI.layout('ctm-pause', i);
      if (this.screen === 'results') {
        x = 450;
        y = 299 + i * 15;
        w = 126;
        h = 17;
      }
      if (this.screen === 'title') {
        x = 205;
        y = 348;
        w = 230;
        h = 35;
      }
      if (this.screen === 'character') {
        x = 399;
        y = 342;
        w = 150;
      }
      if (this.screen === 'details') {
        x = 240;
        y = 358 + i * 21;
        w = 170;
        h = 20;
      }
      if (this.screen === 'main' && this.mainMenu?.ready) [x, y, w, h] = this.mainMenu.layout(i);
      if (this.characterSelect?.owns(this.screen)) [x, y, w, h] = this.characterSelect.layout(i);
      if (this.feScreens?.owns(this.screen)) {
        b.disabled = !this.ready || this.feScreens.disabled(i);
        [x, y, w, h] = this.feScreens.layout(i);
      }
      if (this.eventSelect?.owns(this.screen)) {
        b.disabled = !this.ready || this.eventSelect.disabled(i);
        [x, y, w, h] = this.eventSelect.layout(i);
      }
      if (this.previews?.owns?.(this.screen)) {
        b.disabled = !this.ready || this.previews.disabled(i);
        [x, y, w, h] = this.previews.layout(i);
      }
      if (this.saveLoad?.owns(this.screen)) {
        b.disabled = !this.ready || this.saveLoad.disabled(i);
        [x, y, w, h] = this.saveLoad.layout(i);
      }
      if (this.audioMenus?.owns(this.screen)) {
        b.disabled = !this.ready || this.audioMenus.disabled(i);
        [x, y, w, h] = this.audioMenus.layout(i);
      }
      if (this.careerUI?.owns(this.screen)) {
        b.disabled = !this.ready || this.careerUI.disabled(this.screen, i);
        [x, y, w, h] = this.careerUI.layout(this.screen, i);
      }
      if (this.mpUI?.owns(this.screen)) {
        b.disabled = !this.ready || this.mpUI.disabled(this.screen, i);
        [x, y, w, h] = this.mpUI.layout(this.screen, i);
      }
      b.style.cssText = `left:${x / 6.4}%;top:${y / 4.48}%;width:${w / 6.4}%;height:${h / 4.48}%;`;
      b.onclick = () => {
        this.index = i;
        this.choose(i);
      };
      b.onpointerenter = (e) => {
        if (e.pointerType === 'mouse' && !mouseMoved) return;
        // a button rebuilt under a resting cursor is not a hover
        this.index = i;
      };
      this.nav.appendChild(b);
    });
    if (this.screen === 'character')
      for (const [direction, x, label] of [
        [-1, 354, 'Previous rider'],
        [1, 572, 'Next rider']
      ]) {
        const b = document.createElement('button');
        b.textContent = label;
        b.setAttribute('aria-label', label);
        b.disabled = !this.ready;
        b.style.cssText = `left:${x / 6.4}%;top:${298 / 4.48}%;width:${32 / 6.4}%;height:${32 / 4.48}%;`;
        b.onclick = () => this.cycleRider(direction);
        this.nav.appendChild(b);
      }
    this.feScreens?.legend(this.nav);
    if (this.previews?.owns?.(this.screen)) this.previews.legend?.(this.nav);
    if (this.screen === 'main' && this.feScreens?.ready) {
      const b = document.createElement('button');
      b.textContent = 'Options';
      b.setAttribute('aria-label', 'Options');
      b.disabled = !this.ready;
      b.style.cssText = `left:${410 / 6.4}%;top:${402 / 4.48}%;width:${150 / 6.4}%;height:${18 / 4.48}%;`;
      b.onclick = () => this.feScreens.openOptions('main');
      this.nav.appendChild(b);
    }
  }
  mainDisabled() {
    return mainDisabled({ career: !!this.careerUI?.ready, previews: !!this.previews?.ready, online: !!this.mpUI });
  }
  // The last rider comes back on the first Select Character of a visit (web/save-store.js selection; a ?rider= URL wins). Press START: the
  // original snow flake burst (FE.LUI 'transition', 40 frames, ends white) over the title, then the Main Menu builds in (PS2 frames
  // local/ps2-capture/menus/fe-texture/13-title-to-menu-*).
  leaveTitle() {
    if (this.titleOut) return;
    const t = this.mainMenu?.data?.screens?.transition;
    if (!t || !this.images['FE_1-11']) this.set('main');
    else this.titleOut = { lui: new LuiScreen(t, this.images, this), at: performance.now() };
    // cFEStateTitle's notify 0x1946A8 plays FE event 15 (snd 7) and the menu's UINext accept event 0 (snd 3) in the press's frame
    // (web/game-audio.js ui); after the transition's start, which a first play's buffer build (about 10 ms) must not delay
    try {
      this.gameAudio?.ui?.(15);
      this.gameAudio?.ui?.(0);
    } catch {}
  }
  drawTitleOut(c) {
    const o = this.titleOut;
    if (!o) return;
    const frame = ((performance.now() - o.at) * 60) / 1000;
    if (frame >= 40) {
      this.titleOut = null;
      this.set('main');
      return;
    }
    const events = o.lui.screen.events.filter((ev) => ev.frame <= frame).map((ev) => ({ ev, start: ev.frame }));
    c.save();
    c.scale(1, 448 / 480);
    o.lui.draw(c, events, frame);
    c.restore();
  }
  restoreRider() {
    if (this.riderRestored) return;
    this.riderRestored = true;
    try {
      if (new URL(location.href).searchParams.get('rider')) return;
    } catch {}
    const sel = loadSelection(),
      id = sel?.rider;
    if (!id || !this.cb.rider) return;
    const base = this.riders.find((r) => r.id === (sel.base || id)),
      entry = this.riders.find((r) => r.id === id);
    if (!base || (base.id === this.rider?.id && !sel.base)) return;
    const target = entry?.kind === 'cheat' && sel.base ? { ...entry, base: sel.base } : base;
    Promise.resolve(this.cb.rider(target))
      .then(() => {
        this.characterSelect?.adopt?.(target);
        this.riderIndex = Math.max(0, this.riders.indexOf(base));
        this.sync();
      })
      .catch((e) => console.warn('Last rider not restored', e));
  }
  rememberRider() {
    const h = this.characterSelect?.human?.() || this.rider;
    if (h?.id) saveSelection({ rider: h.id, base: h.base || null });
  }
  // Single Event start (Select Event, the old event list): the career objectives flow, else the course callback and the load screen.
  // cb.course may return false (navigating away), true, or a Promise of either (in-app course switch).
  startSingleEvent(c) {
    if (this.careerUI?.ready && this.careerUI.single(c)) return;
    const r = c && this.cb.course ? this.cb.course(c) : true;
    if (r === false) return;
    const go = () => {
      this.set('game');
      this.cb.start();
    };
    if (r && typeof r.then === 'function') {
      let ok = true;
      this.loadEvent(() => {
        if (ok) go();
      }, [
        Promise.resolve(r).then((v) => {
          if (v === false) ok = false;
        })
      ]);
    } else this.loadEvent(go);
  }
  showResults(result) {
    if (this.careerUI?.active) {
      this.careerUI.finish({
        ...result,
        score: result.score ?? this.lastState.score,
        raceTicks: result.raceTicks ?? this.lastState.raceTicks
      });
      return;
    }
    this.result = result;
    this.screen = 'results';
    this.index = 1;
    this.sync();
  }
  choose(i) {
    if (!this.ready) return;
    if (this.careerUI?.owns(this.screen)) {
      this.careerUI.choose(i);
      return;
    }
    if (this.mpUI?.owns(this.screen)) {
      this.mpUI.choose(i);
      return;
    }
    if (this.characterSelect?.owns(this.screen)) {
      this.characterSelect.choose(i);
      return;
    }
    if (this.audioMenus?.owns(this.screen)) {
      this.audioMenus.choose(i);
      return;
    }
    if (this.eventSelect?.owns(this.screen)) {
      this.eventSelect.choose(i);
      return;
    }
    if (this.previews?.owns?.(this.screen)) {
      this.previews.choose(i);
      return;
    }
    if (this.saveLoad?.owns(this.screen)) {
      this.saveLoad.choose(i);
      return;
    }
    if (this.feScreens?.owns(this.screen)) {
      this.feScreens.choose(i);
      return;
    }
    if (this.screen === 'details' && i === 1) {
      this.characterSelect?.openCheats();
      return;
    }
    if (this.screen === 'results') {
      if (i === 1) this.cb.start();
      // the full replay (web/replay-ui.js)
      if (i === 2) this.cb.replay?.();
      if (i === 4) {
        this.cb.quit();
        this.set('main');
      }
      return;
    }
    if (this.screen === 'main' && this.mainDisabled()[i]) return;
    if (this.screen === 'main') this.mainIndex = i;
    if (this.screen === 'title') this.leaveTitle();
    else if (this.screen === 'main' && i === 3 && this.previews?.ready) {
      this.set('fe-previews');
      // Previews: the EA trailers (146Bonusmat, web/fe-previews.js)
    } else if (this.screen === 'main' && i === 5 && pv('onlineRecords')) {
      this.careerMode = false;
      this.onlineMode = false;
      this.careerUI?.online?.records?.load();
      this.eventSelect?.openBoards?.();
      // pv onlineRecords: Leaderboards -> the Select Event maps -> an event's board
    } else if (this.screen === 'main' && i === 0) {
      this.careerMode = false;
      this.onlineMode = false;
      this.set('character');
    } else if (this.screen === 'main' && i === 4 && this.mpUI) {
      // Online: rider select, then the lobby browser (web/mp-ui.js)
      this.careerMode = false;
      this.onlineMode = true;
      this.set('character');
    } else if (this.screen === 'main' && i === 1 && this.careerUI.ready) {
      this.careerMode = true;
      this.set('character');
    } else if (this.screen === 'character') this.set('setup');
    else if (this.screen === 'setup') {
      if (i === 0 && this.onlineMode) this.mpUI.enter();
      else if (i === 0 && this.careerMode) this.careerUI.enter();
      else this.set(i === 0 ? 'event' : 'details');
    } else if (this.screen === 'details') this.set('setup');
    else if (this.screen === 'event') {
      // fallback course list (courses.json) when the Map screen is not exported
      this.startSingleEvent((this.courses || [])[i]);
    } else if (this.screen === 'options') {
      if (i === 0) this.cycleCamera(1);
      else if (i === 1) this.cycleWidescreen(1);
      else if (i === 2) this.cycleKeyboard();
      else if (this.items()[i] === 'Display & Touch') {
        this.feScreens.openDisplay('options');
      } else if (i === 3 && this.audioMenus?.ready)
        this.audioMenus.open('fe-sound', {
          ingame: this.optionsReturn !== 'fe-options' && !!this.optionsReturn,
          back: () => {
            this.set('options');
            this.index = 3;
            this.sync();
          }
        });
      else {
        this.set(this.optionsReturn || 'pause');
        this.optionsReturn = null;
      }
    } else if (this.screen === 'pause') {
      if (i === 2 && this.audioMenus?.ready) {
        this.audioMenus.open('audio', {
          back: () => {
            this.set('pause');
            this.index = 2;
            this.sync();
          }
        });
        return;
      }
      if (i === 3) {
        this.optionsReturn = null;
        this.set('options');
        return;
      }
      if (i === 0) {
        this.set('game');
        this.cb.resume();
      }
      if (i === 1 && !this.mpUI?.racing) {
        this.set('game');
        this.cb.start();
      }
      if (i === 4) {
        this.cb.quit();
        // online race: DNF, back to the lobby (web/mp-ui.js)
        if (this.mpUI?.racing) this.mpUI.quitRace();
        else this.set('main');
      }
    }
  }
  back() {
    if (this.careerUI?.owns(this.screen)) {
      this.careerUI.back();
      return;
    }
    if (this.mpUI?.owns(this.screen)) {
      this.mpUI.back();
      return;
    }
    if (this.characterSelect?.owns(this.screen)) {
      this.characterSelect.back();
      return;
    }
    if (this.audioMenus?.owns(this.screen)) {
      this.audioMenus.back();
      return;
    }
    if (this.eventSelect?.owns(this.screen)) {
      this.eventSelect.back();
      return;
    }
    if (this.previews?.owns?.(this.screen)) {
      this.previews.back();
      return;
    }
    if (this.saveLoad?.owns(this.screen)) {
      this.saveLoad.back();
      return;
    }
    if (this.feScreens?.owns(this.screen)) {
      this.feScreens.back();
      return;
    }
    if (this.screen === 'results') return;
    if (this.screen === 'options') {
      const to = this.optionsReturn || 'pause';
      this.optionsReturn = null;
      this.set(to);
      this.index = to === 'ctm-pause' ? (this.careerUI?.singlePause?.() ? 3 : 4) : to === 'ctm-mcomm' ? 5 : 3;
      this.sync();
    } else if (this.screen === 'pause') {
      this.set('game');
      this.cb.resume();
    } else this.set({ main: 'title', character: 'main', setup: 'character', event: 'setup', details: 'setup' }[this.screen] || 'title');
  }
  wrap(value, width, size) {
    const measure = (text) => [...text].reduce((sum, ch) => sum + ((this.fonts.FEFONT[ch]?.advance || 10) * size) / 22, 0),
      lines = [];
    let line = '';
    for (const word of value.split(/\s+/)) {
      const next = line ? line + ' ' + word : word;
      if (line && measure(next) > width) {
        lines.push(line);
        line = word;
      } else line = next;
    }
    if (line) lines.push(line);
    return lines;
  }
  text(ctx, value, x, y, size = 19, color = '#101c28', font = 'FEFONT', align = 'left') {
    const glyphs = this.fonts[font];
    if (!glyphs) return;
    const scale = size / (font === 'FEFONT' ? 22 : 17);
    let total = 0;
    for (const ch of value) total += (glyphs[ch]?.advance || 10) * scale;
    if (align === 'right') x -= total;
    if (align === 'center') x -= total / 2;
    let key = font + color,
      im = this.tints.get(key);
    if (!im) {
      im = document.createElement('canvas');
      const original = this.images[font + '-0'];
      im.width = original.width;
      im.height = original.height;
      const c = im.getContext('2d', SPRITE_2D);
      // web/sprite-canvas.js: software in Firefox, where an accelerated glyph tint cost a GPU readback per draw (docs/firefox-load.md)
      c.drawImage(original, 0, 0);
      c.globalCompositeOperation = 'source-in';
      c.fillStyle = color;
      c.fillRect(0, 0, im.width, im.height);
      this.tints.set(key, im);
    }
    im = glyphSource(im);
    // WebKit (softGlyphs): a software copy of the same tint
    for (const ch of value) {
      const g = glyphs[ch];
      if (!g) {
        x += 10 * scale;
        continue;
      }
      ctx.drawImage(im, g.x, g.y, g.w, g.h, x + g.dx * scale, y + g.dy * scale, g.w * scale, g.h * scale);
      x += g.advance * scale;
    }
  }
  sprite(name, sx, sy, sw, sh, x, y, w = sw, h = sh) {
    if (drawGlyphAsKey(this.fg, this, name, sx, sy, sw, sh, x, y, w, h)) return;
    // a PS2 button icon as the key cap while the keyboard is in use
    const im = this.images[name];
    if (im) this.fg.drawImage(im, sx, sy, sw, sh, x, y, w, h);
  }
  drawBoostLetters(s) {
    if (!this.boostGauge) return;
    const c = this.fg;
    c.save();
    c.scale(1, 448 / 480);
    for (const item of boostLetters(
      this.boostGauge.profile,
      s.boostLetterCount ?? 0,
      s.boostLetterFraction ?? 0,
      s.boostLetterRemoved ?? true,
      s.boostPendingCount ?? 0,
      s.boostPendingPhase ?? -1
    )) {
      const center = [item.letter < 4 ? 558 + 16 * item.letter : 550 + 16 * (item.letter - 4), item.letter < 4 ? 90 : 68];
      c.save();
      c.translate(...center);
      c.scale(item.scale, item.scale);
      c.translate(-center[0], -center[1]);
      for (const d of [...item.draws].sort((a, b) => a.order - b.order)) {
        c.save();
        c.globalAlpha = Math.trunc(d.argb[0] * 128) / 128;
        if (d.text) {
          const color =
            '#' +
            d.argb
              .slice(1)
              .map((x) =>
                Math.round((Math.trunc(x * 128) * 255) / 128)
                  .toString(16)
                  .padStart(2, '0')
              )
              .join('');
          c.translate(...d.position);
          c.scale(...d.scale);
          this.text(c, d.text, 0, 0, 17, color, 'HUDFONT');
        } else {
          c.translate(...center);
          c.scale(item.outlineScaleX, 1);
          c.translate(-center[0], -center[1]);
          const [v, u, u1, v1] = d.uv;
          this.sprite(
            'OV_1-4',
            u * 256,
            v * 256,
            (u1 - u) * 256,
            (v1 - v) * 256,
            ...d.position,
            d.size[0] * d.scale[0],
            d.size[1] * d.scale[1]
          );
        }
        c.restore();
      }
      c.restore();
    }
    c.restore();
  }
  drawResetFade(alpha) {
    if (this.screen !== 'game' || !(alpha > 0)) return;
    const c = this.fg;
    c.save();
    c.globalAlpha = Math.min(alpha, 1);
    c.fillStyle = '#ffffff';
    c.fillRect(0, 0, 640, 448);
    c.restore();
  }
  draw(s) {
    // pv transportFade: last frame's HUD squeeze
    if (this.hudSqueezed) {
      this.hudSqueezed = false;
      this.fg.restore();
    }
    this.lastState = s;
    const c = this.fg,
      b = this.bg;
    c.clearRect(0, 0, 640, 448);
    b.clearRect(0, 0, 640, 448);
    if (!this.images['FE_1-20']) return;
    if (this.screen === 'loading') {
      this.loading.draw(c, b);
      return;
    }
    if (this.screen === 'transition') {
      this.transitions?.draw(c);
      return;
    }
    if (this.screen === 'cutscene') {
      this.audioMenus?.drawHud(c);
      // EA RADIO BIG now-playing, as in the PS2 intro
      this.cutscene?.draw(c, this);
      return;
    }
    if (this.replayUi?.owns(this.screen)) {
      this.replayUi.draw(c, b);
      return;
    }
    if (this.careerUI?.owns(this.screen)) {
      this.careerUI.draw(c, b);
      return;
    }
    if (this.mpUI?.owns(this.screen)) {
      this.mpUI.draw(c, b);
      return;
    }
    if (this.characterSelect?.owns(this.screen)) {
      this.characterSelect.draw(c, b);
      return;
    }
    if (this.audioMenus?.owns(this.screen)) {
      this.audioMenus.draw(c, b);
      return;
    }
    if (this.eventSelect?.owns(this.screen)) {
      this.eventSelect.draw(c, b);
      return;
    }
    if (this.mainMenu?.owns(this.screen)) {
      this.mainMenu.draw(c, b);
      return;
    }
    if (this.previews?.owns?.(this.screen)) {
      this.previews.draw(c, b);
      return;
    }
    if (this.saveLoad?.owns(this.screen)) {
      this.saveLoad.draw(c, b);
      return;
    }
    if (this.feScreens?.owns(this.screen)) {
      this.feScreens.draw(c, b);
      return;
    }
    if (this.screen === 'results') {
      // Layout follows the owned Single Event Results reference; ranks come from the six-rider race (web/ai-race.js results) when computer
      // riders raced.
      c.fillStyle = 'rgba(13,58,79,.9)';
      c.fillRect(50, 57, 540, 329);
      c.strokeStyle = '#6698a9';
      c.lineWidth = 3;
      c.strokeRect(53, 82, 534, 303);
      this.sprite('OV_1-6', 0, 226, 256, 30, 330, 80, 256, 46);
      this.text(c, this.course?.label || 'Snow Jam - Race', 88, 88, 21, '#d1e1e2');
      this.text(c, 'Single Event Results', 88, 112, 19, '#66a9bb');
      this.text(c, 'Rank', 118, 149, 17, '#d1e1e2');
      this.text(c, 'Riders', 198, 149, 17, '#d1e1e2');
      this.text(c, 'Time', 418, 149, 17, '#d1e1e2');
      if (this.result.rows?.length) {
        const ord = ['1st', '2nd', '3rd', '4th', '5th', '6th'];
        this.result.rows.forEach((r, i) => {
          const color = r.human ? '#e8bd72' : '#d1e1e2',
            y = 175 + i * 19;
          this.text(c, ord[i] || String(i + 1), 128, y, 18, color);
          this.text(c, r.name || r.character || '', 198, y, 18, color);
          this.text(c, raceTime(r.ticks, false), 418, y, 18, color);
        });
        // 0x1E6718: rows in 0x238BF8 time order (0x536708); riders still on course show their 0x122D78 estimate
      } else {
        this.text(c, '-', 143, 179, 18, '#e8bd72');
        this.text(c, this.result.rider, 198, 179, 18, '#e8bd72');
        this.text(c, raceTime(this.result.ticks, false), 418, 179, 18, '#e8bd72');
      }
      this.items().forEach((label, i) => this.text(c, label, 450, 299 + i * 15, 14, i === this.index ? '#dce5e4' : '#559aad'));
      this.sprite('OV_1-2', 55, 122, 24, 24, 430, 313, 17, 17);
      return;
    }
    if (this.screen === 'game') {
      // FE HUD Options 0 Full/1 Minimal/2 None, Speed units (web/fe-options.js)
      const hudLevel = this.feScreens?.hudLevel?.() ?? 0,
        kmh = this.feScreens?.speedUnits?.() === 1;
      // a fade over the world, the HUD over it
      if (this.cutscene?.overlayUnderHud) this.cutscene.draw(c, this);
      {
        const sq = this.cutscene?.hudSqueeze?.() || 0;
        if (sq > 0) {
          // pv transportFade: the render block 2EAA28 / 2EA900 (top 0.125, height 0.75) holds the HUD inside the letterbox while the bars
          // slide out (PS2 to-c-fade: HUD top row 73 -> 21)
          c.save();
          this.hudSqueezed = true;
          c.translate(0, 448 * 0.125 * sq);
          c.scale(1, 1 - 0.25 * sq);
        }
      }
      this.audioMenus?.drawHud(c);
      // EA RADIO BIG now-playing popup (web/now-playing.js)
      if (this.cutscene?.overlay && !this.cutscene.overlayUnderHud) {
        // a cutscene's pre-fade over the running world: bars, Loading..., fade; no HUD (web/cutscenes.js preFade)
        this.cutscene.draw(c, this);
        return;
      }
      this.careerUI?.tick(s);
      if (hudLevel < 2) this.careerUI?.hud(c, s);
      // freestyle (career-ui.js freestyleHud): its own clock/standings, no progress meter, banner only once finished
      const fs = !!this.careerUI?.freestyleHud?.();
      // pv finishHudHide: once the rider has finished (rider+0x470 >= 0; TIME'S UP too) 1EB9E8 sets the per-player mask +0x80 = 0xFFEFFFFF
      // (0x1EB9FC; 0x1ECB04 draws owner+0x3CC & ~mask), and 12A250 (every human finished, 0x1EB91C) cuts owner+0x3CC to 0x170000: every
      // element but the 0x100000 banner goes on the finish tick, a cut (PS2 setpieces/full, ctm-parity race-f / race-q). Races and peak
      // runs; free ride (event type 4, excluded at 0x1EB9C0) never finishes.
      const finishHide = !fs && !!s.message && pv('finishHudHide');
      // free ride (web/free-ride-hud.js via main.js): collectibles and cash, no clock/score/progress
      const frFlags = this.freeRideHud?.(c, hudLevel, finishHide),
        fr = !!frFlags,
        frHud = typeof frFlags === 'number' ? frFlags : 0x1530c380;
      // 0x1EBA10 clears the hints bit 0x1000000 (the RECOVER = button label, the Uber hint) once the rider's profile visited mask (+0xACC)
      // has any Peak 2 or Peak 3 location (0x45A7C4 / C8 masks; PS2 fr-dbc2, fr-throne, frc-1800 states: clear mask 0x01000000, Peak 1
      // states 0)
      const hintsOff = (fr || this.careerMode) && !!this.careerUI?.hintsOff?.(),
        baseFlags = ((fr ? frHud : (this.trickHud?.flags ?? 0)) & ~(hintsOff ? 0x1000000 : 0)) >>> 0;
      if (fs && s.message) return;
      // a Conquer the Mountain collect in a race (HUD slot 0x31, pre-pass 0x1EBB00): the snowflake counter (0x80) shows while the 'Collect
      // +$ n' popup lives, place/standings (0x8400031) hide
      const collecting = !fr && hudLevel < 2 && !!s.trickSlots?.some((x) => x && x.type === 0x31),
        act = this.careerUI?.career?.active;
      if (collecting && !finishHide && act && act.ev?.course >= 0 && act.ev.course < COLLECTIBLE_TOTALS.length)
        drawCollectCounter(this, c, this.careerUI.career.collectCount(act.id, act.ev.course), COLLECTIBLE_TOTALS[act.ev.course]);
      if (s.racePlace && hudLevel < 2 && !collecting && !finishHide) drawRacePlace(this, c, s.racePlace);
      // career new-message icon (web/career-messages.js); under the finish HUD its timer 1EB6E4 runs, the draw (owner bit 0x80000) is cut
      if (hudLevel < 2) this.careerUI?.messages?.drawHud(c, true, !finishHide);
      const raceHud = this.trickHud && this.trickHudRenderer ? [] : null;
      // the race clock 0x1F16C0 and the speed widget 0x2200C0 through the original HUD text (web/trick-hud.js raceClock /
      // speed, docs/visual-parity.md)
      if (!fs && !fr && hudLevel < 2 && !finishHide) {
        const ticks = s.raceTicks ?? Math.round(Math.max(0, s.seconds || 0) * 60);
        if (raceHud) {
          const t = Math.trunc(Math.fround(Math.fround(ticks) * Math.fround(0.01666666753590107)));
          this.trickHud.raceClock(raceHud, Math.trunc(t / 3600), Math.trunc((t % 3600) / 60), t % 60);
        } else {
          const time = hudRaceTime(ticks);
          this.text(c, time, 320, 20, 21, '#eef5ee', 'HUDFONT', 'center');
        }
      }
      if (!fr && (!s.trickSlots || !this.trickHud) && hudLevel < 2 && !finishHide)
        this.text(c, String(Math.round(s.score || 0)), 602, 20, 24, '#edf5e8', 'HUDFONT', 'right');
      if (hudLevel < 1 && !finishHide) {
        if (raceHud) this.trickHud.speed(raceHud, (s.speed || 0) * 100, kmh);
        else {
          this.text(c, String(Math.round((s.speed || 0) * (kmh ? 3.6 : 2.237))), 32, 391, 27, '#edf5e8', 'HUDFONT', 'center');
          this.text(c, kmh ? 'KM/H' : 'MPH', 32, 418, 11, '#e1e9e5', 'HUDFONT', 'center');
        }
      }
      if (raceHud?.length) this.trickHudRenderer.render(c, raceHud);
      // 0x220260: km/h = cm/s x 0.036, profile 0x535610 bit 19
      if ((!fs || this.careerUI?.slopeHud?.()) && !fr && hudLevel < 1 && !finishHide) {
        // slope style keeps the progress meter (0x40, flags 0x1D31C047): the original 0x20EDA0 (web/progress-meter-hud.js, fed per tick by
        // main.js)
        this.progressMeter?.draw(this, c);
      }
      if (hudLevel < 2 && !finishHide) {
        this.boostGauge?.drawOrb(c, s.boostFlashPhase ?? -1, s.boostFlashPaletteTier ?? 0);
        this.boostGauge?.draw(c, s.boostPreview ?? 0, s.boostStored ?? 0, s.boostFlashPhase ?? -1, s.boostFlashPaletteTier ?? 0);
        this.drawBoostLetters(s);
      }
      if (hudLevel < 2 && !finishHide && this.trickHud && this.trickHudRenderer) {
        // The switch-stance 'S' under the meter (web/trick-hud.js switchIcon, HUD flags 0x10000000)
        const flags =
          hudLevel === 1
            ? this.feScreens.minimalHudFlags(fr || hintsOff ? baseFlags : this.trickHud.flags)
            : fr || hintsOff
              ? baseFlags
              : this.trickHud.flags;
        if (flags & 0x10000000) {
          const l = [];
          this.trickHud.switchIcon(l, s.stanceRegular !== false);
          this.trickHudRenderer.render(c, l);
        }
      }
      if (s.countdown >= 1 && s.countdown <= 3) {
        const glyph = [
          [0, 84, 24, 42],
          [26, 84, 48, 42],
          [76, 84, 46, 42]
        ][s.countdown - 1];
        this.sprite('OV_1-3', ...glyph, 320 - glyph[2] / 2, 112, glyph[2], glyph[3]);
      }
      if (hudLevel < 2) {
        if (s.trickSlots && this.trickHud) {
          let slotFlags =
            hudLevel === 1
              ? this.feScreens.minimalHudFlags(fr || hintsOff ? baseFlags : this.trickHud.flags)
              : fr || hintsOff
                ? baseFlags
                : this.trickHud.flags;
          // finished: 0x1EB9FC masks the per-player flags (pv finishHudHide: to 0x100000 alone; before, only the Uber hint went)
          if (finishHide) slotFlags = (slotFlags & 0x100000) >>> 0;
          this.trickHudRenderer.render(
            c,
            this.trickHud.frame(s.trickSlots, {
              flags: slotFlags,
              finished: !!s.message || !!s.timedOut,
              keys: inputDevice() === 'keyboard' ? (b) => keyFor(b, { context: 'race', mode: this.keyboardMode }) : null
            })
          );
        } else if (s.trick && hudLevel < 1 && !finishHide) this.text(c, s.trick, 320, 399, 17, '#ecec13', 'HUDFONT', 'center');
      }
      if (s.message && !fs) {
        // TIME'S UP (+0x480: pause Give Up) is the 'timeup' banner sprite (21F660, same as freestyle) for 3 s of +0x470, then nothing until
        // the results
        if (this.trickHud && this.trickHudRenderer) {
          // the banner 0x21F660 (web/trick-hud.js finishBanner): 'fini' + the finish time "%02d:%02d:%02d" at scale
          // 1.9076, or 'timeup'
          const list = [];
          if (s.timedOut) {
            if ((s.finishElapsed ?? 0) < 3) this.trickHud.finishBanner(list, true);
          } else {
            const t = Math.trunc(Math.fround(Math.fround(s.finishTicks || s.raceTicks || 0) * Math.fround(0.01666666753590107))),
              two = (n) => String(n).padStart(2, '0');
            this.trickHud.finishBanner(list, false, `${two(Math.trunc(t / 3600))}:${two(Math.trunc((t % 3600) / 60))}:${two(t % 60)}`);
          }
          this.trickHudRenderer.render(c, list);
        } else if (s.timedOut) {
          if ((s.finishElapsed ?? 0) < 3) this.sprite('OV_1-3', 1.5, 127.5, 169, 23, 200, 159.5, 240, 41);
        } else this.text(c, s.message, 320, 180, 24, '#ff6124', 'HUDFONT', 'center');
      }
      return;
    }
    if (this.screen === 'options') {
      b.fillStyle = '#7198b0';
      b.fillRect(0, 0, 640, 448);
      this.sprite('OV_1-7', 0, 0, 256, 80, 0, 0, 640, 85);
      b.fillStyle = '#eaf0ec';
      b.fillRect(122, 109, 43, 210);
      this.items().forEach((t, i) => {
        this.text(c, t, 190, 111 + i * 37, 22, this.index === i ? '#f2f5ec' : '#112634');
      });
      this.text(
        c,
        { 0x3c: 'Near', 0x3d: 'Mid', 0x3e: 'Far' }[this.cameraView],
        600,
        111,
        22,
        this.index === 0 ? '#f2f5ec' : '#112634',
        'FEFONT',
        'right'
      );
      this.text(c, WIDESCREEN_MODES[this.widescreen].label, 600, 148, 22, this.index === 1 ? '#f2f5ec' : '#112634', 'FEFONT', 'right');
      this.text(c, this.keyboardMode, 600, 185, 22, this.index === 2 ? '#f2f5ec' : '#112634', 'FEFONT', 'right');
      const help = this.wrap(
        this.index === 0
          ? 'Choose a camera view.'
          : this.index === 1
            ? 'Select widescreen support for anamorphic and 16:9 displays.'
            : this.index === 2
              ? this.keyboardMode === 'Simple'
                ? 'Arrows/WASD steer on the ground and spin/flip in the air.'
                : 'Arrows/WASD steer; IJKL spin/flip in the air.'
              : this.index === 3 && this.audioMenus?.ready
                ? 'Set sound mode options.'
                : 'Get back to boarding.',
        400,
        16
      );
      help.forEach((line, i) => this.text(c, line, 42, 387 - (help.length - 1 - i) * 19, 16));
      this.sprite('OV_1-2', 55, 122, 24, 24, 460, 380, 20, 20);
      this.text(c, 'Select', 481, 381, 17);
      return;
    }
    if (this.screen === 'pause' && this.pdaPause()) {
      // the online race pause / fallback Single Event pause in the MCOMM PDA (PS2 menus/single/11-single-pause)
      const cu = this.careerUI,
        online = !!this.mpUI?.racing;
      cu.mcommFrame(c, b);
      cu.pda.menu(
        c,
        this.items().map((label, i) => ({
          label,
          icon: ['conticon', 'rstarticon', 'radioicon', 'opticon', 'hexicon'][i],
          disabled: !!this.nav.children[i]?.disabled
        })),
        this.index,
        [
          cu.t('kT_OVRHELPGetBoarding'),
          cu.t('kT_OVRHELPRestartComp'),
          cu.t('kT_OVRHELPChangeMusic'),
          cu.t('kT_OVRHELPOptions'),
          online ? cu.t(0x0b25df85, 'Quit to Online Main Menu.') : cu.t('kT_OVRHELPQuitComp')
        ][this.index] || ''
      );
      return;
    }
    if (this.screen === 'pause') {
      b.fillStyle = '#7198b0';
      b.fillRect(0, 0, 640, 448);
      this.sprite('OV_1-7', 0, 0, 256, 80, 0, 0, 640, 85);
      b.fillStyle = '#eaf0ec';
      b.fillRect(122, 109, 43, 210);
      this.items().forEach((t, i) => {
        this.text(
          c,
          t,
          190,
          111 + i * 37,
          22,
          this.index === i ? '#f2f5ec' : (i === 2 && !this.audioMenus?.ready) || i === 3 ? '#466375' : '#112634'
        );
      });
      this.text(c, 'Get back to boarding.', 42, 387, 16);
      this.sprite('OV_1-2', 55, 122, 24, 24, 460, 380, 20, 20);
      this.text(c, 'Select', 481, 381, 17);
      return;
    }
    b.fillStyle = this.screen === 'title' ? '#78a5c8' : '#7daccc';
    b.fillRect(0, 0, 640, 448);
    if (this.screen === 'title') {
      this.attract?.frame();
      // the original 06title with its load meter, the same screen as from the first paint (web/title-screen.js via web/boot-screen.js)
      if (
        globalThis.ssxBoot?.draw(c, b, {
          ready: this.ready,
          keyboard: inputDevice() === 'keyboard',
          error: this.error && this.error !== 'Loading...' ? this.error : null
        })
      ) {
        this.drawTitleOut(c);
        return;
      }
      for (let i = 0; i < 14; i++) {
        const x = (i * 83 + 17) % 640,
          y = ((performance.now() / 150 + i * 79) % 500) - 40;
        b.globalAlpha = 0.18;
        b.drawImage(this.images['FE_1-11'], 105, 90, 65, 66, x, y, 70, 70);
      }
      b.globalAlpha = 1;
      this.sprite('FE_1-20', 0, 0, 512, 347, 83, 95, 475, 322);
      this.text(
        c,
        this.ready ? (inputDevice() === 'keyboard' ? 'Press Enter' : 'Press START button') : titleLoading(this.error),
        320,
        350,
        17,
        '#132834',
        'FEFONT',
        'center'
      );
      this.text(c, '© 2003 Electronic Arts Inc. All rights reserved.', 320, 378, 12, '#1d3140', 'FEFONT', 'center');
      return;
    }
    b.fillStyle = '#6b9fc2';
    b.beginPath();
    b.moveTo(0, 198);
    b.lineTo(110, 55);
    b.lineTo(180, 218);
    b.lineTo(269, 302);
    b.lineTo(315, 448);
    b.lineTo(0, 448);
    b.fill();
    b.fillStyle = '#c9e1ee';
    b.beginPath();
    b.moveTo(420, 448);
    b.lineTo(615, 295);
    b.lineTo(640, 285);
    b.lineTo(640, 448);
    b.fill();
    b.drawImage(this.images['FE_1-7'], 0, 85, 180, 170, -4, -9, 194, 178);
    this.sprite('FE_1-7', 0, 0, 102, 52, 531, 17, 93, 47);
    const title = {
      main: 'Main Menu',
      character: 'Select Character',
      setup: 'Setup Character',
      event: 'Select Event',
      details: 'Rider Details',
      pause: 'Paused'
    }[this.screen];
    this.text(c, '· ' + title, 41, 43, 23, '#e5e8db');
    c.setLineDash([4, 3]);
    c.strokeStyle = '#50788f';
    c.lineWidth = 2;
    c.beginPath();
    c.moveTo(30, 78);
    c.lineTo(615, 78);
    c.stroke();
    c.setLineDash([]);
    if (this.screen === 'character') {
      this.text(c, 'Rider ranking', 247, 101, 17);
      this.text(c, '1.0', 574, 101, 17);
      const names = ['Acceleration', 'Edging', 'Speed', 'Spin', 'Stability', 'Toughness', 'Tricks'];
      names.forEach((name, i) => {
        let y = 144 + i * 20;
        this.text(c, name, 245, y, 16);
        for (let j = 0; j < 10; j++) {
          c.fillStyle = j === 0 ? '#c76c03' : '#b9d6e8';
          c.fillRect(397 + j * 15, y + 1, 13, 13);
          c.strokeStyle = '#234a61';
          c.lineWidth = 2;
          c.strokeRect(397 + j * 15, y + 1, 13, 13);
        }
        this.text(c, '1.0', 558, y, 16);
      });
      this.text(c, '<', 358, 305, 23);
      this.text(c, this.rider.name, 400, 305, 23);
      this.text(c, '>', 576, 305, 23);
      this.wrap(this.rider.card.join(' '), 360, 13)
        .slice(0, 3)
        .forEach((line, i) => this.text(c, line, 42, 374 + i * 17, 13));
    } else if (this.screen === 'details') {
      this.text(c, this.rider.name, 280, 114, 23);
      let size = 11,
        lines = this.wrap(this.rider.bio, 350, size);
      while (lines.length * 15 > 195 && size > 8) {
        size--;
        lines = this.wrap(this.rider.bio, 350, size);
      }
      lines.forEach((line, i) => this.text(c, line, 245, 159 + i * 15, size));
      this.items().forEach((t, i) =>
        this.text(
          c,
          t,
          245,
          360 + i * 21,
          17,
          i === 1 && !this.characterSelect?.canOpenCheats() ? '#7396ab' : this.index === i ? '#c46b04' : '#101c28'
        )
      );
    } else {
      if (this.screen === 'main') {
        this.wrap(MAIN_HELP[this.index] || '', 215, 15)
          .slice(0, 5)
          .forEach((line, k) => this.text(c, line, 41, 157 + k * 20, 15));
      }
      if (this.screen === 'setup') this.text(c, this.rider.name, 280, 114, 23);
      if (this.screen === 'event')
        this.text(
          c,
          ({ superpipe: 'Super Pipe', slopestyle: 'Slopestyle', bigair: 'Big Air', backcountry: 'Backcountry' }[
            (this.courses || [])[this.index]?.event
          ] || 'Race') + ' - Peak 1',
          280,
          114,
          23
        );
      const items = this.items();
      items.forEach((label, i) => {
        if (this.index === i) {
          c.fillStyle = '#c46b04';
          c.fillRect(276, 153 + i * 23, 319, 22);
        }
        this.text(
          c,
          label,
          284,
          153 + i * 23,
          19,
          this.screen === 'main' && this.mainDisabled()[i] ? '#7396ab' : this.index === i ? '#e5e8db' : '#102638'
        );
      });
    }
    let cy = this.screen === 'character' ? 342 : this.screen === 'main' ? 360 : 380;
    // main: three legend rows (Select / Previous / Options) above the orange line
    this.sprite('OV_1-2', 55, 122, 24, 24, 449, cy, 18, 19);
    this.text(c, 'Select', 469, cy + 1, 16);
    this.sprite('OV_1-2', 9, 122, 24, 24, 430, cy + 21, 18, 19);
    this.text(c, 'Previous', 450, cy + 22, 15);
    // Square: Options (PS2 main menu)
    if (this.screen === 'main' && this.feScreens?.ready) {
      this.sprite('OV_1-2', 33, 122, 24, 24, 413, cy + 42, 18, 19);
      this.text(c, 'Options', 433, cy + 43, 15);
    }
    c.strokeStyle = '#d57b0b';
    c.lineWidth = 3;
    c.beginPath();
    c.moveTo(4, 429);
    c.lineTo(535, 429);
    c.stroke();
    this.characterSelect?.drawOverlay(c);
  }
}
