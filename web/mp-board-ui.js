// The online race board after an online race (pv mpBoard, docs/multiplayer.md "Multiplayer leaderboard"), driven by web/mp-ui.js on
// its 'results' screen (so main.js keeps the finish camera behind it, as behind the Single Event Results):
//   Single Event Results -> Continue -> the event's board on 61toptimes ("Top 5 Record Times / Scores", web/results-lui.js records(),
//   the layout the course records' ctm-board uses): the subtitle "Online Races", rank numbers, the top rows and your own row (the top 4
//   and yours when you are outside the top 5) in the record row's colour, the message line, Continue / Save Records (the PS2 screen's
//   two items after a new record).
// The name: the profile's Player Name (ssx3.playerName: Rider Details > Player Name, the save file; the course records and time attack
// read the same name). With none saved, the game keyboard (cKeyboardPopup, fe-screens.js openKeyboard 'record', Player Name's PS2 rules:
// 8 characters, 1CD088(kb, 8) at 0x1837A4; Tab and the punctuation keys off, 1CE3C8 at 0x1837C4..0x1838A0; 1CD2F0's overwrite at the
// limit) opens over the board; Done saves the name on the profile and the run under it. Later races save under it at once. Saves go to
// web/server/race-board.mjs (unverified boards, separate from the verified course records) with the reconnect token; the server takes
// the time or score from its own record of the race.
import { pv } from './pv-flags.js';
import { raceTime } from './race-time.mjs';
import { storage, writeText } from './save-store.js';

const NAME_KEY = 'ssx3.playerName';
const TIMEOUT_MS = 6000;
const SY = 448 / 480;
const NAME_RE = /^[A-Za-z0-9 !@#$%^&*()]{1,8}$/;
const TEXT = {
  sub: 'Online Races',
  loading: 'Loading...',
  saving: 'Saving your record online...',
  saved: (rank) => `Your record is number ${rank + 1} online.`,
  notKept: 'Your best time under this name is already listed.',
  notKeptScore: 'Your best score under this name is already listed.',
  rank: (rank, total) => `Your rank: ${rank + 1} of ${total}.`,
  noFinish: 'Only finished runs are saved.',
  failed: 'The record could not be saved.',
  offline: 'Online records unavailable.',
  empty: 'No online races saved for this event yet.'
};

async function timed(url, init = {}) {
  const ctl = typeof AbortController === 'function' ? new AbortController() : null;
  const t = ctl ? setTimeout(() => ctl.abort(), TIMEOUT_MS) : null;
  try {
    return await fetch(url, { cache: 'no-store', ...init, ...(ctl ? { signal: ctl.signal } : {}) });
  } finally {
    if (t) clearTimeout(t);
  }
}

export class MpBoard {
  constructor(ui, session) {
    this.ui = ui;
    this.session = session;
    this.view = null;
  }
  get on() {
    return pv('mpBoard');
  }
  get open() {
    return !!this.view;
  }
  // ---- the profile's name ----
  savedName() {
    try {
      const v = storage()?.getItem(NAME_KEY);
      return v && v.trim() ? v : null;
    } catch {
      return null;
    }
  }
  setName(name) {
    writeText(NAME_KEY, name);
    const fe = this.ui.feScreens;
    if (fe) fe.playerName = name;
    this.ui.playerName = name;
    const career = this.ui.careerUI?.career;
    if (career) career.playerName = name;
  }
  restoreName(previous) {
    if (previous == null) {
      try {
        storage()?.removeItem(NAME_KEY);
      } catch {}
      const fe = this.ui.feScreens;
      const fallback = 'PLAYER 1';
      if (fe) fe.playerName = fallback;
      this.ui.playerName = fallback;
      return;
    }
    this.setName(previous);
  }
  // ---- this race ----
  mine() {
    const race = this.session.race;
    const fin = this.session.finalResults?.find((x) => x.slot === race?.slot);
    return { race, fin };
  }
  courseIndex(code) {
    return (this.ui.careerUI?.data?.courses ?? []).findIndex((x) => x.code === code);
  }
  // Results' Continue: the board, saving the run at once under the profile's name, or asking for one
  start() {
    const { race, fin } = this.mine();
    const code = race?.course || this.ui.course?.code;
    this.view = {
      race: race?.id ?? null,
      code,
      key: null,
      timed: true,
      rows: [],
      total: 0,
      you: null,
      savedId: null,
      status: 'loading',
      note: null,
      canSave: !!race?.id && !!fin && !fin.dnf,
      saved: false
    };
    this.ui.index = 0;
    this.ui.sync();
    const name = this.savedName();
    if (!this.view.canSave) {
      this.view.note = fin?.dnf ? TEXT.noFinish : null;
      this.fetch(name);
    } else if (name) this.save(name);
    else {
      this.fetch(null);
      this.askName();
    }
  }
  close() {
    if (this.ui.feScreens?.keyboard?.kind === 'record') this.ui.feScreens.keyboard = null;
    this.view = null;
  }
  async fetch(name) {
    const v = this.view;
    if (!v) return;
    const q = new URLSearchParams({ course: v.code || '', limit: '10' });
    if (name) q.set('name', name);
    try {
      const r = await timed(`/mp/race-board?${q}`);
      if (!r.ok) throw new Error(String(r.status));
      const b = await r.json();
      if (this.view !== v) return;
      this.take(b);
      v.status = 'ready';
    } catch {
      if (this.view !== v) return;
      v.status = 'error';
    }
    this.ui.sync();
  }
  take(b) {
    const v = this.view;
    v.key = b.event ?? v.key;
    v.timed = b.timed ?? v.timed;
    v.rows = Array.isArray(b.rows) ? b.rows : [];
    v.total = b.total | 0;
    v.you = b.you ?? null;
  }
  askName() {
    const fe = this.ui.feScreens;
    if (!fe?.openKeyboard || !this.view?.canSave || this.view.saved) return;
    fe.openKeyboard('record', { text: this.ui.feScreens?.playerName || 'PLAYER 1', done: (text) => this.named(text) });
  }
  named(text) {
    const name = String(text || '')
      .replace(/\s+/g, ' ')
      .trim();
    if (!NAME_RE.test(name)) {
      if (this.view) this.view.note = TEXT.failed;
      this.ui.sync();
      return;
    }
    const previous = this.savedName();
    this.setName(name);
    this.save(name, previous);
  }
  // previous: the profile's name before this one (undefined: it was already saved), put back if the server refuses the name
  async save(name, previous) {
    const v = this.view;
    if (!v || v.saved || v.status === 'saving') return;
    v.status = 'saving';
    v.note = TEXT.saving;
    this.ui.sync();
    let res;
    try {
      const r = await timed('/mp/race-board/save', {
        method: 'POST',
        headers: { 'content-type': 'application/json', 'x-ssx-session': this.session.client.token ?? '' },
        body: JSON.stringify({ race: v.race, name })
      });
      res = await r.json().catch(() => ({ error: 'network' }));
      if (!r.ok && !res.error) res.error = String(r.status);
    } catch {
      res = { error: 'network' };
    }
    if (this.view !== v) return;
    if (res?.ok) {
      v.saved = true;
      v.savedId = res.id;
      this.take(res);
      v.status = 'ready';
      v.note = res.kept ? TEXT.saved(res.rank) : v.timed ? TEXT.notKept : TEXT.notKeptScore;
      this.ui.index = 0;
    } else {
      if (res?.error === 'name' && previous !== undefined) this.restoreName(previous);
      // 'race': the server no longer has this race (restarted for a deploy, or 30 minutes later); 'saved': already saved
      v.status = 'ready';
      v.note = res?.error === 'network' || res?.error === 'off' ? TEXT.offline : TEXT.failed;
      if (res?.error === 'race' || res?.error === 'saved' || res?.error === 'dnf' || res?.error === 'racer') v.canSave = false;
      this.fetch(this.savedName());
    }
    this.ui.sync();
  }
  // ---- the screen ----
  items() {
    const t = (k, f) => this.ui.careerUI?.t?.(k, f) || f;
    return [t(0x0f3ab955, 'Continue'), t(0x0b0bfc13, 'Save Records')];
  }
  disabled(i) {
    const v = this.view;
    return i === 1 && (!v || !v.canSave || v.saved || v.status === 'saving');
  }
  choose(i) {
    if (this.keyboardOpen()) return true;
    if (i === 1) {
      if (!this.disabled(1)) this.askName();
      return true;
    }
    return false;
  }
  keyboardOpen() {
    return this.ui.feScreens?.keyboard?.kind === 'record';
  }
  key(e) {
    if (!this.keyboardOpen()) return false;
    this.ui.feScreens.keyboardKey(e);
    this.ui.sync();
    return true;
  }
  // Back with the keyboard up closes it (the run stays unsaved: Save Records opens it again)
  back() {
    if (!this.keyboardOpen()) return false;
    this.ui.feScreens.keyboard = null;
    this.ui.sync();
    return true;
  }
  rider(id) {
    const r = (this.ui.riders || []).find((x) => x.id === id);
    const name = r?.name || (id ? id[0].toUpperCase() + id.slice(1) : '');
    return name.split(' ')[0];
  }
  // the top 5, or the top 4 and your own row below them
  shownRows() {
    const v = this.view;
    const yourRank = v.you?.rank ?? -1;
    const top = v.rows.slice(0, 5);
    const list = yourRank >= 5 ? [...top.slice(0, 4), v.you] : top;
    return list.map((r) => ({
      rank: r.rank + 1,
      name: r.name,
      rider: this.rider(r.rider),
      value: v.timed ? raceTime(r.value, true) : String(r.value),
      player: !!v.you && r.id === v.you.id
    }));
  }
  message() {
    const v = this.view;
    if (v.note) return v.note;
    if (v.status === 'loading') return TEXT.loading;
    if (v.status === 'error') return TEXT.offline;
    if (!v.rows.length) return TEXT.empty;
    if (v.you) return TEXT.rank(v.you.rank, v.total);
    return null;
  }
  title() {
    const cu = this.ui.careerUI;
    const v = this.view;
    const course = this.courseIndex(v.code);
    const mode = v.key ? +v.key.split(':')[0] : course >= 0 && cu?.career ? (cu.career.standardMode(course) ?? 4) : 0;
    try {
      if (course >= 0) return cu.eventTitle({ mode, course, round: 3, career: false });
    } catch {}
    return this.ui.course?.label || this.ui.course?.name || '';
  }
  draw(c) {
    const cu = this.ui.careerUI;
    const v = this.view;
    if (!v) return;
    const drawn =
      cu?.luiPanels?.() &&
      cu.resultsLui.records(c, {
        title: this.title(),
        timed: v.timed,
        subtitle: TEXT.sub,
        rows: this.shownRows(),
        message: this.message(),
        items: this.items(),
        index: this.ui.index,
        disabled: (i) => this.disabled(i)
      });
    if (!drawn) {
      c.fillStyle = 'rgba(13,58,79,.9)';
      c.fillRect(50, 57, 540, 329);
      this.ui.text(c, this.title(), 88, 88, 21, '#d1e1e2');
      this.ui.text(c, TEXT.sub, 88, 112, 19, '#66a9bb');
      this.shownRows().forEach((r, i) =>
        this.ui.text(c, `${r.rank}  ${r.name}  ${r.rider}  ${r.value}`, 110, 160 + i * 22, 17, r.player ? '#e8bd72' : '#d1e1e2')
      );
      this.ui.text(c, this.message() || '', 88, 300, 14, '#66a9bb');
    }
    if (this.keyboardOpen()) {
      const fe = this.ui.feScreens;
      c.save();
      c.scale(1, SY);
      fe.drawKeyboard(c, fe.now());
      c.restore();
    }
  }
}
