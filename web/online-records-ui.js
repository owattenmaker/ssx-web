// Online course records in the game's own screens (pv onlineRecords, docs/online-records.md). Driven by web/career-ui.js, which
// owns the screens:
//   ctm-records  61toptimes (Top 5 Record Times / Scores): the event's online top 5 instead of the local table, for the events with
//                an online board (modes 0-5; the peak runs and in-world Conquer the Mountain events keep the PS2 table, decision D1).
//                After a run that makes the online top 5: Continue / Save Records / Online Records; Save Records opens the game's
//                keyboard (cKeyboardPopup, Player Name's rules) and uploads the run with its replay. From the results' Records item:
//                Return / Online Records. The third item is an addition (the PS2 screen has two).
//   ctm-board    the event's full online board on the same 61toptimes layout, 5 rows a page (an addition): Up / Down move the row
//                cursor, Left / Right and L1 / R1 page, Cross Watch Replay, Triangle back. Also opened from the main menu's
//                Leaderboards (through the Select Event maps).
import { pv } from './pv-flags.js';
import { raceTime } from './race-time.mjs';
import { recordSlot } from './career.js';
import { OnlineRecords, eventKeyOf } from './online-records.js';

const PAGE = 5, SY = 448 / 480;
const TEXT = {
  online: 'Online Records', watch: 'Watch Replay', page: (a, b) => `Page ${a} of ${b}`, saving: 'Saving your record online...',
  saved: (rank) => `Your record is number ${rank + 1} online.`, notKept: 'Your best time under this name is already listed.', notKeptScore: 'Your best score under this name is already listed.',
  offline: 'Online records unavailable.', loading: 'Loading...', failed: 'The record could not be saved.',
  review: 'Your run is under review.',   // the server held it (under its event's floor: records.mjs), neutral on purpose
};
let shared = null;
export const onlineRecords = () => (shared ??= new OnlineRecords());

export class OnlineRecordsUI {
  constructor(cu) { this.cu = cu; this.ui = cu.ui; this.run = null; this.board = null; this.note = null; if (this.on) this.records.load(); }   // the cards and INFO read the boards
  get on() { return pv('onlineRecords'); }
  get records() { return onlineRecords(); }
  codeOf(course) { return this.cu.career?.courses?.[course]?.code ?? this.cu.data?.courses?.[course]?.code ?? null; }
  keyOf(ev) { const code = ev && ev.mode >= 0 && ev.mode < 6 ? this.codeOf(ev.course) : null; return code ? eventKeyOf(ev.mode, code) : null; }
  playerName() { return this.ui.feScreens?.playerName || 'PLAYER 1'; }
  // ctm-records shows the online board for this event: modes 0-5 (a board exists for them), not after an in-world Conquer the
  // Mountain run (main.js onlineRun), which keeps the PS2 table (D1).
  // No board loaded at all (offline from the start): the PS2 screen and decision exactly (the local table, Continue / Save Records).
  online(ev) { return this.on && !!this.keyOf(ev) && this.records.has(this.keyOf(ev)) && !this.ui.cb.onlineRun?.()?.inWorld; }

  // ---- the run's finish (career-ui finish): null keeps the PS2 decision (the local table), else whether the top-5 screen opens ----
  finish(result, ev) {
    this.run = null; this.note = null;
    if (!this.on) return null;
    const key = this.keyOf(ev); if (!key) return null;
    this.records.load();   // refresh in the background for the screen
    const info = this.ui.cb.onlineRun?.();
    if (info?.inWorld) return null;
    if (!this.records.has(key)) return null;   // no board (offline, none cached): the PS2's own decision on the local table
    // the claim: the finish tick's record when the page has it (game-tick.js rec.finish: race ticks, the score latched at the finish,
    // what the verifier re-simulates), else the results' values
    const fin = info?.finish, timed = this.records.timed(key);
    const value = timed ? (result.dnf ? null : Math.round(fin?.ticks ?? result.ticks)) : fin ? fin.score | 0 : Math.round(result.score || 0);
    if (!info?.available || info.giveUp || result.dnf || !(value > 0)) return false;   // no replay, a Give Up / TIME'S UP / DNF: not submitted
    const rank = this.records.rank(key, value, this.playerName());
    this.run = { key, ev: { mode: ev.mode, course: ev.course }, timed, value, rank, character: info.character, state: 'new', id: null };
    return rank >= 0;
  }
  // the board's first entry as career.js topRecord shapes it ({value, ticks, name, character}), or null
  topOf(ev) { const r = this.records.top(this.keyOf(ev))?.[0]; return r ? { ...r, ticks: r.value } : null; }
  topRun(ev) { return this.run && this.run.key === this.keyOf(ev) && this.run.rank >= 0 ? this.run : null; }

  // ---- ctm-records ----
  recordsItems(topTime) {
    const t = (k, f) => this.cu.t(k, f);
    return topTime ? [t(0x0f3ab955, 'Continue'), t(0x0b0bfc13, 'Save Records'), TEXT.online] : [t('kT_OVRCMNReturn', 'Return'), TEXT.online];
  }
  recordsDisabled(topTime, i) {
    if (topTime && i === 1) return !this.run || this.run.state !== 'new' || !this.records.online;
    if (i === (topTime ? 2 : 1)) return !this.records.summary;
    return false;
  }
  // Returns true when the choice was this module's.
  chooseRecords(topTime, i, ev) {
    if (i === (topTime ? 2 : 1)) { if (!this.recordsDisabled(topTime, i)) this.openBoard(this.keyOf(ev), { back: 'ctm-records', ev }); return true; }
    if (topTime && i === 1) { if (!this.recordsDisabled(topTime, i)) this.saveRecord(); return true; }
    return false;
  }
  saveRecord() {
    const fe = this.ui.feScreens; if (!fe?.openKeyboard) return;
    fe.openKeyboard('record', { text: this.playerName(), done: (name) => this.submit(name) });
  }
  async submit(name) {
    const run = this.run; if (!run || run.state !== 'new') return;
    const text = String(name || '').replace(/\s+/g, ' ').trim(); if (!text) return;
    run.state = 'sending'; run.name = text; this.note = TEXT.saving; this.ui.sync();
    let res;
    try {
      const file = await this.ui.cb.onlineReplayFile?.({
        event: run.key,
        mode: run.ev.mode,
        course: this.codeOf(run.ev.course),
        name: text,
        claim: run.timed ? { ticks: run.value } : { score: run.value }
      });
      res = file ? await this.records.submit(file.meta, file.pad) : { error: 'replay' };
    } catch (e) { console.warn('Online record upload failed', e); res = { error: 'network' }; }
    if (res?.ok) {
      run.state = 'sent';
      run.id = res.id;
      run.rank = res.kept ? res.rank : res.review ? -1 : run.rank;
      this.note = res.review ? TEXT.review : res.kept ? TEXT.saved(res.rank) : run.timed ? TEXT.notKept : TEXT.notKeptScore;
      if (this.ui.screen === 'ctm-records' && this.ui.index === 1) this.ui.index = 0;
    } // the greyed Save Records gives the focus to Continue
    else {
      run.state = 'new';
      this.note = res?.error === 'network' || res?.error === 'off' ? TEXT.offline : TEXT.failed;
    }
    this.ui.sync();
  }
  // rows of the screen: the online top 5; before the upload the finished run sits at its rank in the player's colour (as the PS2
  // shows a new record), after it the server's rows with the run's entry in that colour.
  recordRows(ev) {
    const key = this.keyOf(ev), top = (this.records.top(key) ?? []).slice(), timed = this.records.timed(key), run = this.topRun(ev);
    let rows = top.map((r) => ({ ...r, player: !!run?.id && r.id === run.id }));
    if (run && run.state !== 'sent' && run.rank >= 0) {
      const lower = this.playerName().toLowerCase();
      rows = rows.filter((r) => r.default || r.name?.toLowerCase() !== lower);
      rows.splice(run.rank, 0, { name: run.name || this.playerName(), character: run.character, value: run.value, player: true });
    }
    return rows.slice(0, 5).map((r) => this.row(r, timed, false));
  }
  row(r, timed, hundredths, extra = {}) {
    return { name: r.name, rider: this.cu.data?.characters?.[r.character]?.first || '', value: timed ? raceTime(r.value, hundredths) : String(r.value), player: !!r.player, ...extra };
  }
  recordsMessage(ev, topTime) {
    if (this.note) return this.note;
    if (!this.records.summary) return TEXT.offline;
    if (topTime) {
      const timed = this.records.timed(this.keyOf(ev));
      return timed
        ? this.cu.t(0x0eea5fd5, "Congratulations, you've got a top time!")
        : this.cu.t(0x0ea6d945, "Congratulations, you've got a top score!");
    }
    return this.records.online ? null : TEXT.offline;
  }
  // 61toptimes with the online rows; the keyboard over it while a name is typed. False: draw the PS2 table instead.
  drawRecords(c, ev, topTime) {
    const cu = this.cu, ui = this.ui, key = this.keyOf(ev); if (!key || !cu.luiPanels()) return false;
    const timed = this.records.has(key) ? this.records.timed(key) : ev.mode === 0 || ev.mode === 4;
    cu.resultsLui.records(c, { title: cu.eventTitle(ev), timed, rows: this.records.has(key) ? this.recordRows(ev) : this.localRows(ev, timed), message: this.recordsMessage(ev, topTime),
      items: ui.items(), index: ui.index, disabled: (i) => this.recordsDisabled(topTime, i) });
    this.drawKeyboard(c);
    return true;
  }
  // no board ever loaded (offline from the start): the PS2 table (web/career.js), as the screen looked before
  localRows(ev, timed) {
    const career = this.cu.career, slot = career ? recordSlot(career.rules, ev.mode, ev.course) : 26;
    return slot < 26
      ? career
          .records(slot, timed)
          .map((r) => ({
            name: r.name,
            rider: this.cu.data?.characters?.[r.character]?.first || '',
            value: timed ? raceTime(r.ticks, false) : String(r.value),
            player: !!r.player
          }))
      : [];
  }
  drawKeyboard(c) { const fe = this.ui.feScreens; if (fe?.keyboard?.kind !== 'record') return; c.save(); c.scale(1, SY); fe.drawKeyboard(c, fe.now()); c.restore(); }
  keyboardOpen() { return this.ui.feScreens?.keyboard?.kind === 'record'; }
  // keys while the keyboard is up (career-ui key): the keyboard's own handling
  key(e) {
    if (this.keyboardOpen()) { this.ui.feScreens.keyboardKey(e); if (!this.keyboardOpen()) this.ui.sync(); return true; }
    if (this.ui.screen === 'ctm-board') return this.boardKey(e);
    return false;
  }

  // ---- ctm-board ----
  // key: "<mode>:<COURSE>"; opts {back: the screen Triangle returns to, ev: {mode, course}, fe: drawn over the front end}
  openBoard(key, { back = 'ctm-records', ev = null, fe = false, backIndex = 0 } = {}) {
    if (!key) return;
    const [mode, code] = key.split(':'), course = ev?.course ?? (this.cu.career?.courses ?? this.cu.data?.courses ?? []).findIndex((x) => x.code === code);
    this.board = { key, ev: ev ?? { mode: +mode, course }, back, backIndex, fe, rows: [], total: 0, cursor: 0, status: 'loading', timed: this.records.timed(key) };
    this.ui.set('ctm-board'); this.ui.index = 0; this.ui.sync();
    this.fetchBoard();
  }
  async fetchBoard() {
    const b = this.board; if (!b) return;
    try { const r = await this.records.board(b.key, 0, 100); if (this.board !== b) return; b.rows = r.rows; b.total = r.total; b.timed = r.timed; b.status = 'ready'; }
    catch { if (this.board !== b) return; const top = this.records.top(b.key); b.rows = top ?? []; b.total = b.rows.length; b.status = top ? 'cached' : 'error'; }
    this.ui.sync();
  }
  boardItems() { return [TEXT.watch, this.cu.t('kT_OVRCMNReturn', 'Return')]; }
  focusedEntry() { return this.board?.rows?.[this.board.cursor] ?? null; }
  boardDisabled(i) { if (i !== 0) return false; const r = this.focusedEntry(); return !r || !r.replay || !this.ui.cb.watchOnlineReplay || this.board.status === 'error'; }
  chooseBoard(i) {
    if (i === 1) { this.backBoard(); return; }
    if (this.boardDisabled(0)) return;
    const b = this.board, entry = this.focusedEntry();
    this.ui.cb.watchOnlineReplay({ entry, key: b.key, ev: b.ev, back: () => { this.board = b; this.ui.set('ctm-board'); this.ui.index = 0; this.ui.sync(); } });
  }
  backBoard() {
    const b = this.board; this.board = null;
    if (typeof b?.back === 'function') { b.back(); return; }
    this.ui.set(b?.back || 'ctm-records'); this.ui.index = b?.backIndex ?? (b?.back === 'ctm-records' ? (this.ui.items().length - 1) : 0); this.ui.sync();
  }
  boardKey(e) {
    const b = this.board; if (!b) return false;
    const n = b.rows.length, move = (to) => { b.cursor = Math.max(0, Math.min(Math.max(0, n - 1), to)); this.ui.sync(); };
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { e.preventDefault(); move(b.cursor + (e.code === 'ArrowUp' ? -1 : 1)); return true; }
    if (e.code === 'ArrowLeft' || e.code === 'KeyQ') { e.preventDefault(); if (!e.repeat) move(b.cursor - PAGE); return true; }
    if (e.code === 'ArrowRight' || e.code === 'KeyE') { e.preventDefault(); if (!e.repeat) move(b.cursor + PAGE); return true; }
    if (e.code === 'Enter' || e.code === 'Space') { e.preventDefault(); if (!e.repeat) this.chooseBoard(0); return true; }
    if (e.code === 'Escape') { e.preventDefault(); if (!e.repeat) this.backBoard(); return true; }
    return false;
  }
  drawBoard(c, bg) {
    const cu = this.cu, b = this.board; if (!b) return;
    if (b.fe && bg) { bg.fillStyle = '#75a9cb'; bg.fillRect(0, 0, 640, 448); }   // the front end's sky blue behind the panel (web/fe-screens.js)
    if (!cu.luiPanels()) return;
    const start = Math.floor(b.cursor / PAGE) * PAGE, pages = Math.max(1, Math.ceil(b.rows.length / PAGE));
    const rows = b.rows.slice(start, start + PAGE).map((r, k) => this.row(r, b.timed, true, { rank: (r.rank ?? start + k) + 1, focus: start + k === b.cursor, player: this.mine(r) }));
    const message = b.status === 'loading' ? TEXT.loading : b.status === 'error' ? TEXT.offline : TEXT.page(start / PAGE + 1, pages) + (b.status === 'cached' ? `  (${TEXT.offline})` : '');
    cu.resultsLui.records(c, {
      title: cu.eventTitle(b.ev),
      timed: b.timed,
      subtitle: TEXT.online,
      rows,
      message,
      items: this.boardItems(),
      index: this.ui.index,
      disabled: (i) => this.boardDisabled(i)
    });
  }
  mine(r) { return !r.default && (r.id === this.run?.id || (!!r.name && r.name.toLowerCase() === this.playerName().toLowerCase())); }
}
