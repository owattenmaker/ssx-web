// Conquer the Mountain messages: the rider-profile inbox, every message the original posts (0x1E1550..0x1E3A78), the
// Message Center (OV.LUI 112messagecenter / 113ViewMessage) and the in-race new-message icon (HUD event 8). Data:
// /assets/CAREER/messages.json (tools/export_messages.py); docs/characters.md "Relationship messages" / "Career messages".
//
// Original (SLUS_207.72):
//   inbox    per rider profile block 0x4A6CA8 + bank*0x9B50 + char*0xF88 + 0xE38 (0x147908): 25 x {item, variant},
//            +0xC8 read bits (a new message clears its bit), +0xCC count; +0xD0.. one posted bit per category
//            (0x147980 test / 0x147A30 set; bits 40..51 are flags of empty kind-7 categories, bits 7..11 the open FAQ
//            folders).
//   0x1E2FE0 add: inbox full (25) -> drop the oldest (0x1E31B8); flag-1 records draw their subject variant (0x1E3100:
//            kinds 3/4 r % 10, 5 r % 11, 8 r & 3); store, clear the read bit, HUD event 8.
//   Posters, all gated by 0x5305F9 == 0 (Conquer the Mountain) and, except 0x1E2A08, by a tutorial counter
//   (gp-0x848 +0x84 +0x28: 0 or >= 10 in every career state, so it never blocks here):
//   0x154EE8 event complete -> 0x1E25D8 backcountry result (0x1E2648 race / 0x1E2828 jam) -> [no medal: stop] ->
//            0x1E1EB8 pending peak-challenge notices -> 0x1E1550 rival reminders (not in rival events) -> 0x1591E8:
//            awards 0x159CD0 (0x1E2578 / 0x1E2518 / 0x1E2428 / 0x1E38B8), peak-challenge flags 0x1E1DD0, then the goal
//            list walk 0x45AAD8 (0x1E1C10 rival taunt while a standard event is missing, 0x1E2370 no more taunts once
//            the rival opens).
//   0x119EF8 award cash (collectibles, free-ride tricks, Big Challenges) -> 0x1E3760 earnings trophy notices.   0x155BF0 / 0x155E58 -> 0x1E2A08 aggression.
// The draws come from the presentation generator (web/lineup.js presentationDraw, the session estimate).
import { presentationDraw } from './lineup.js';
import { pv } from './pv-flags.js';
import { glyphKeyRect } from './input-glyphs.js';

export const INBOX_SIZE = 25;
export const PEAK_RIVALS = [3, 7, 8];   // 0x145750: Mac / Nate / Psymon (their riders get the other half of the texts)
const Y = (y) => Math.round(y * 448 / 480);
const VISIBLE = 8;
const NONE = -1, SILVER = 2;

// ---- the inbox (pure; web/test-messages.mjs) -------------------------------------------------------------------
export class MessageInbox {
  constructor(data, box = null) { this.data = data; this.box = box || MessageInbox.empty(); }
  static empty() { return { entries: [], read: 0, posted: [] }; }
  get count() { return this.box.entries.length; }
  isRead(k) { return ((this.box.read >>> k) & 1) === 1; }
  unread() { let n = 0; for (let k = 0; k < this.count; k++) if (!this.isRead(k)) n++; return n; }
  posted(category) { return this.box.posted.includes(category); }
  setPosted(category, on = true) { const p = this.box.posted.filter((c) => c !== category); if (on) p.push(category); this.box.posted = p; }
  // 0x1E3388: the entry index of an item, -1 when it is not in the inbox.
  has(item) { return this.box.entries.findIndex((e) => e[0] === item); }
  // 0x1E31B8: remove entry k, later entries and their read bits move up.
  remove(k) {
    const b = this.box; if (k < 0 || k >= b.entries.length) return;
    for (let i = k; i < b.entries.length - 1; i++) { b.entries[i] = b.entries[i + 1]; if ((b.read >>> (i + 1)) & 1) b.read |= 1 << i; else b.read &= ~(1 << i); }
    b.entries.pop(); b.read &= (1 << b.entries.length) - 1;
  }
  // 0x1E32C8: remove the entries of a category. The loop counts its steps against the shrinking count, so it stops
  // early after removals (an entry near the end can stay), as in the original.
  removeCategory(category) {
    let k = 0;
    for (let n = 0; n < this.count; n++) { if (this.data.records[this.box.entries[k][0]].category === category) this.remove(k); else k++; }
  }
  markRead(k) { this.box.read |= 1 << k; }
  // 0x1E2FE0 / 0x1E3100: returns the new entry.
  add(item, draw = presentationDraw, variant = 0) {
    const b = this.box;
    if (b.entries.length === INBOX_SIZE) this.remove(0);
    const record = this.data.records[item];
    if (record.flag === 1) {
      const kind = this.data.categories[record.category].kind;
      variant = kind === 3 || kind === 4 ? draw() % 10 : kind === 5 ? draw() % 11 : kind === 8 ? draw() & 3 : 0xFF;
    }
    b.read &= ~(1 << b.entries.length);
    b.entries.push([item, variant]);
    return { item, variant };
  }
  // 0x1E2A08: a relationship score of `character` changed; humanBase = the human's character. Returns the entry posted.
  notify(character, score, humanBase, draw = presentationDraw) {
    if (character === humanBase || character < 0 || character > 9) return null;
    const category = 12 + character, row = this.data.categories[category];
    if (this.posted(category)) { if (score < 5) this.setPosted(category, false); return null; }
    if (!((draw() & 7) + 15 < score)) return null;
    const entry = this.add(row.first + draw() % row.count, draw);
    this.setPosted(category);
    return entry;
  }
  // 0x1E4F80: kind 3 kT_MSGSubjectBCFinishWin/Loss%d (by item, 0x46E1E0), 4 Foreshadow%d, 5 Aggression%d, 8 BeatThePeak%d.
  subject(item, variant) {
    const r = this.data.records[item], kind = this.data.categories[r.category]?.kind, s = this.data.subjects;
    const list = kind === 5 ? s.aggression : kind === 4 ? s.foreshadow : kind === 8 ? s.beat_the_peak
      : kind === 3 ? (this.data.bc_win_items.includes(item) ? s.bc_win : s.bc_loss) : null;
    return list?.[variant] ?? r.subject;
  }
  // 0x1E2EA0: riders, kT_CMNGameTitle (FAQs), kT_CMNDJAtomica (mountain notices), kT_FAQFolder.
  sender(item) { return this.data.senders[this.data.records[item].sender] ?? ''; }
}

// ---- the posters (pure: the career facts come in as arguments) --------------------------------------------------
export class MessageRules {
  constructor(data, inbox, draw = presentationDraw) { this.data = data; this.box = inbox; this.draw = draw; this.posts = []; }
  add(item) { const e = this.box.add(item, this.draw); this.posts.push(e); return e; }
  first(category) { return this.data.categories[category].first; }
  // 0x1E25D8 -> 0x1E2648 (rival race, mode 4) / 0x1E2828 (rival jam, mode 5) on backcountry course 14 + peak.
  backcountryResult(mode, course, place, character) {
    const p = course - 14; if ((mode !== 4 && mode !== 5) || p < 0 || p > 2) return;
    const result = (mode === 4 ? 22 : 25) + p, reminder = (mode === 4 ? 1 : 4) + p;
    if (this.box.posted(result)) return;
    this.box.removeCategory(reminder); this.box.removeCategory(result);
    let item = (mode === 4 ? 0x73 : 0x7F) + 4 * p + (character === PEAK_RIVALS[p] ? 2 : 0);
    if (place === 0) { this.box.setPosted(result); item++; }
    this.add(item);
  }
  // 0x1E1EB8: flags 40..51 (set by 0x1E1DD0 at a silver/bronze peak event) post the peak-challenge notice now.
  pendingPeakChallenges() {
    for (const [flag, jam, p] of [[40, 0, 0], [44, 0, 1], [48, 0, 2], [42, 1, 0], [46, 1, 1], [50, 1, 2]]) {
      const silver = this.box.posted(flag) ? 0 : this.box.posted(flag + 1) ? 1 : -1; if (silver < 0) continue;
      const category = 34 + jam + 2 * p;   // 0x1E2220 race / 0x1E20D0 jam
      if (!this.box.posted(category)) { if (silver) this.box.setPosted(category); this.add(this.first(category) + silver); }
      this.box.setPosted(flag + silver, false);
    }
  }
  // 0x1E1DD0: a peak event (modes 6..11) finished with silver (flag + 1) or bronze.
  peakChallengeFlag(mode, silver) {
    const k = mode - 6; if (k < 0 || k > 5) return;
    const peak = k % 3, jam = k >= 3 ? 1 : 0;
    if (!this.box.posted(34 + jam + 2 * peak)) this.box.setPosted(40 + 4 * peak + 2 * jam + (silver ? 1 : 0));
  }
  // 0x1E1550: the first unposted rival-race reminder (categories 1..3) and rival-jam reminder (4..6), when that rival
  // event is open (+0x278 bit 6+peak / 9+peak clear) and has no medal (0x145EF0: +0xAD0 slot byte 0).
  rivalReminders(character, open, medalled) {
    for (const [base, list] of [[1, 0], [4, 1]]) {
      const p = [0, 1, 2].find((q) => !this.box.posted(base + q)); if (p === undefined) continue;
      if (!open(p, list) || medalled(p, list)) continue;
      this.box.setPosted(base + p);
      this.add(this.first(base + p) + (character === PEAK_RIVALS[p] ? 3 : 0) + this.draw() % 3);
    }
  }
  // 0x1E1C10: a rival taunt of the peak (categories 28 + 2 * peak, 29 + 2 * peak for the rival's own rider) from a
  // rider other than the human, not already in the inbox.
  rivalTaunt(peak, character) {
    if (this.box.posted(28 + 2 * peak) || this.box.posted(29 + 2 * peak)) return;
    const category = (character === PEAK_RIVALS[peak] ? 29 : 28) + 2 * peak, row = this.data.categories[category], pick = [];
    for (let k = row.first; k < row.first + row.count; k++) if (this.data.records[k].sender !== character && this.box.has(k) < 0) pick.push(k);
    if (pick.length) this.add(pick[this.draw() % pick.length]);
  }
  // 0x1591E8 goal-list walk (0x45AAD8 entries {mode, medalled}); goal 'race' lists end with the -1 terminator.
  goalWalk(peak, goal, entries, character) {
    let rival = false, i = 0;
    for (; i < entries.length; i++) {
      const e = entries[i];
      if (e.mode === 4 || e.mode === 5) { rival = true; if (e.medalled) continue; break; }
      if (e.medalled) continue;
      if (rival) break;
      this.rivalTaunt(peak, character); return;   // a standard event still missing: the rival stays locked
    }
    if (i === entries.length && goal === 'race') this.rivalTaunt(peak, character);
    this.box.setPosted(28 + 2 * peak); this.box.setPosted(29 + 2 * peak);   // 0x1E2370
  }
  // 0x159CD0 award notices: 0 all golds (Far East Myth), 1 whole mountain, 2..4 all goals of peak 1..3, 11/12 the
  // free-ride goal of peak 1/2 (0x1E38B8, both peaks checked). peakLocked(n): +0x278 bit 11+n (peak n+1 pass missing).
  award(award, peakLocked, freerideDone) {
    const once = (bit, item, test = bit) => { if (!this.box.posted(test)) { this.box.setPosted(bit); this.add(item); } };
    if (award === 0) once(56, 251);
    else if (award === 1) once(60, 255);
    else if (award === 2) once(57, 252);
    else if (award === 3) once(57, 253, 58);   // the original tests bit 58 and sets 57
    else if (award === 4) once(59, 254);
    else if (award === 11 || award === 12) {
      if (peakLocked(1) && freerideDone(0)) once(54, 249);
      if (peakLocked(2) && freerideDone(1)) once(55, 250);
    }
  }
  // 0x1E3760: an award's cash in the world (before it is added) reaching a peak's earnings goal while the next peak is locked.
  earnings(amount, earned, goals, peakLocked) {
    if (peakLocked(1) && earned + amount >= goals[0] && !this.box.posted(52)) { this.box.setPosted(52); this.add(247); }
    if (peakLocked(2) && earned + amount >= goals[1] && !this.box.posted(53)) { this.box.setPosted(53); this.add(248); }
  }
}

// The career event types of a goal list (0x15A3B8): race list for races, rival races and peak races.
const goalOf = (mode) => (mode === 0 || mode === 4 || (mode >= 6 && mode <= 8) ? 'race' : 'freestyle');

// The mail icon's tint after `frames` race frames (0x1EB6E4 / 0x1F0F3C), null once the 5 s are over.
export function hudTint(spec, frames) { return frames >= spec.seconds * 60 ? null : (frames / 60) % 1 <= spec.blink ? spec.colors[0] : spec.colors[1]; }

// ---- screens and HUD ---------------------------------------------------------------------------------------------
export const MESSAGE_SCREENS = ['ctm-messages', 'ctm-message'];

export class CareerMessages {
  constructor(careerUI) {
    this.cu = careerUI; this.ui = careerUI.ui; this.data = null; this.back = null; this.view = null; this.top = 0;
    this.hud = null; this.lastHud = 0; this.awards = null;
  }
  async load() {
    try { this.data = await (await fetch('/assets/CAREER/messages.json')).json(); } catch { this.data = null; }
    // OV_1-1 (envelope, folder and arrow icons) is not among the pages ui.js loads.
    try { const im = new Image(); im.src = '/assets/UI/OV_1-1.png'; await im.decode(); this.ov1 = im; } catch { this.ov1 = null; }
    this.hook(this.cu.career);
  }
  icon(c, sx, sy, sw, sh, x, y, w, h) { if (this.ov1) c.drawImage(this.ov1, sx, sy, sw, sh, x, y, w, h); }
  get ready() { return !!this.data; }
  // The inbox of a rider profile (the career save's rider record; saved with it, old saves start empty).
  inbox(rider = this.cu.me) { rider.messages ??= MessageInbox.empty(); return new MessageInbox(this.data, rider.messages); }
  humanBase() { return this.cu.me?.character ?? 3; }
  // Relationship notice (web/ai-race.js onRelationshipNotice): only in a Conquer the Mountain event.
  notify(character, score) {
    if (!this.ready || !this.cu.active?.career) return null;
    const entry = this.inbox().notify(character, score, this.humanBase());
    if (entry) { this.hud = { frames: 0 }; this.cu.career.persist(); }
    return entry;
  }
  // The career hooks (web/career.js stays untouched): event completion, awards and collectible cash.
  hook(career) {
    if (!career || career.messagesHooked) return;
    career.messagesHooked = true;
    const self = this, complete = career.completeEvent, grant = career.grantAward, earn = career.earnCash;
    career.completeEvent = function (place, result) {
      const before = self.ready ? self.snapshot(this) : null;
      self.awards = [];
      try { const out = complete.call(this, place, result); if (before) self.eventComplete(this, before, place, out); return out; } finally { self.awards = null; }
    };
    career.grantAward = function (id, award) {
      // pv awardCascade: 159CD0 posts an award's message at its case, before the items, the pass (158F60) and the cascade's awards
      if (pv('awardCascade')) {
        if (!this.rider(id).awards?.includes(award) && self.ready) { if (self.awards) self.awards.push(award); else self.postAward(this, id, award, self.snapshot(this)); }
        return grant.call(this, id, award);
      }
      const had = this.rider(id).awards?.includes(award), got = grant.call(this, id, award);
      if (!had && self.ready) { if (self.awards) self.awards.push(award); else self.postAward(this, id, award, self.snapshot(this)); }
      return got;
    };
    // 119EF8 -> 1E3760 before 1597B0 adds the cash: every award's cash in the world (collectibles, free-ride tricks, Big
    // Challenges; web/career.js earnCash).
    career.earnCash = function (id, amount) {
      if (self.ready && amount > 0) {
        const r = this.rider(id), rules = new MessageRules(self.data, self.inbox(r));
        rules.earnings(amount, r.earned, this.rules.earnings_goal, (n) => !r.peaks[n]);
        if (rules.posts.length) self.hud = { frames: 0 };
      }
      return earn.call(this, id, amount);
    };
  }
  // Career facts before the event result is applied: rival locks (+0x278 bits 6+peak / 9+peak) and peak passes.
  snapshot(career) {
    const id = career.active?.id ?? this.cu.riderId, r = career.rider(id);
    const open = [1, 2, 3].map((peak) => ['race', 'freestyle'].map((g) => career.goalEvents(id, peak, g).filter((e) => e.mode < 4).every((e) => e.medal !== NONE)));
    return { id, open, peaks: [...r.peaks] };
  }
  postAward(career, id, award, before) {
    const rules = new MessageRules(this.data, this.inbox(career.rider(id)));
    rules.award(award, (n) => !before.peaks[n], (p) => career.goalComplete(id, p + 1, 'freeride'));
    if (rules.posts.length) this.hud = { frames: 0 };
  }
  // 0x154EE8's message calls, in the original order.
  eventComplete(career, before, place, out) {
    const { id, ev } = career.active || {}; if (!ev?.career) return;   // 0x5305F9: Conquer the Mountain only
    const r = career.rider(id), character = r.character, rules = new MessageRules(this.data, this.inbox(r));
    const { mode, course } = ev, medal = out.medal;
    rules.backcountryResult(mode, course, place, character);
    if (medal !== NONE) {
      rules.pendingPeakChallenges();
      if (mode !== 4 && mode !== 5) rules.rivalReminders(character, (p, list) => before.open[p][list], (p, list) => career.medal(id, list ? 5 : 4, 14 + p) !== NONE);
      for (const a of this.awards || []) rules.award(a, (n) => !before.peaks[n], (p) => career.goalComplete(id, p + 1, 'freeride'));
      if (medal >= SILVER) rules.peakChallengeFlag(mode, medal === SILVER);
      const peak = mode >= 6 ? (mode - 6) % 3 : career.peakOf(course) - 1, goal = goalOf(mode);
      rules.goalWalk(peak, goal, career.goalEvents(id, peak + 1, goal).map((e) => ({ mode: e.mode, medalled: e.medal !== NONE })), character);
    }
    this.awards = null;
    // every 1E2FE0 post sends HUD event 8; the icon's 5 s clock (1EB6E4) runs only while the HUD is up, so an event's posts
    // show it when the ride resumes (PS2 ctm/caps peak2-arr: the item posted at the Snow Jam final, the icon at the next free ride)
    if (rules.posts.length) { career.persist(); this.hud = { frames: 0 }; }
  }
  owns(screen) { return this.ready && MESSAGE_SCREENS.includes(screen); }
  open(back) { this.back = back; this.view = null; this.top = 0; this.hud = null; this.ui.set('ctm-messages'); this.ui.index = 0; this.ui.sync(); }
  // The career's first FAQ (world state 4 at 0x2309A4 with gp-0x1024 != -1, set by Green Base Station's "?" through stage
  // builtin 100 -> 1E3510; web/peak_world.inc): the Message Center (1E3C00) opens the Progression/Rewards folder (category 7,
  // 1E3A78) for as long as it is shown and views the folder's first FAQ, "How do I open up other peaks?" (PS2 new career:
  // scratchpad music/runs/stall). Previous shows the list; leaving the list returns to the ride and restores the folder.
  openFaq(back, category = 7) {
    const box = this.inbox(), c = this.data.categories[category]; if (!c) { back?.(); return; }
    const wasOpen = box.posted(category); box.setPosted(category, true);
    this.back = () => { if (!wasOpen) { this.inbox().setPosted(category, false); this.cu.career.persist(); } back?.(); };
    this.top = 0; this.hud = null; this.view = { type: 'faq', category, item: c.first + 1 };
    this.ui.set('ctm-message'); this.ui.index = 2; this.ui.sync();
  }
  // List rows (0x1E2BB8): the inbox newest first, then the FAQ folders (kind 2 categories), an open folder's FAQs below it.
  // A folder is open while its posted bit is set (0x1E4338 toggles it), so it stays open in the save.
  rows() {
    const box = this.inbox(), rows = [];
    for (let k = box.count - 1; k >= 0; k--) { const [item, variant] = box.box.entries[k]; rows.push({ type: 'message', index: k, item, variant, number: box.count - k, read: box.isRead(k) }); }
    this.data.categories.forEach((c, cat) => {
      if (c.kind !== 2 || !c.folder) return;
      rows.push({ type: 'folder', category: cat, item: c.first });
      if (box.posted(cat)) for (let i = c.first + 1; i < c.first + c.count; i++) rows.push({ type: 'faq', category: cat, item: i });
    });
    return rows;
  }
  items() { return this.ui.screen === 'ctm-message' ? [this.data.texts.previous, this.data.texts.delete_message, this.data.texts.keep] : this.rows().map((r) => this.inbox().subject(r.item, r.variant)); }
  disabled() { return false; }
  layout(i) { return this.ui.screen === 'ctm-message' ? [30 + 170 * i, Y(405), 150, Y(26)] : (i < this.top || i >= this.top + VISIBLE ? [0, 0, 0, 0] : [40, Y(150) + (i - this.top) * Y(25), 400, Y(24)]); }
  key(e) {
    if (this.ui.screen === 'ctm-messages' && (e.code === 'ShiftLeft' || e.code === 'ShiftRight')) {   // Square: Delete
      const row = this.rows()[this.ui.index]; if (row?.type === 'message') { this.inbox().remove(row.index); this.cu.career.persist(); this.ui.index = Math.max(0, Math.min(this.ui.index, this.rows().length - 1)); this.ui.sync(); }
      return true;
    }
    if (this.ui.screen === 'ctm-messages' && ['ArrowUp', 'ArrowDown'].includes(e.code)) setTimeout(() => this.scroll());
    return false;
  }
  scroll() { const i = this.ui.index, top = this.top; if (i < this.top) this.top = i; if (i >= this.top + VISIBLE) this.top = i - VISIBLE + 1; if (top !== this.top) this.ui.sync(); }   // off-list rows get no pointer target
  choose(i) {
    if (this.ui.screen === 'ctm-message') {
      const box = this.inbox();
      // pv ctmSmallFixes: 0x1E5800 -> 1E3268(item) removes the FIRST entry with the viewed item (a repeated message: the oldest)
      if (i === 1 && this.view?.type === 'message') { box.remove(pv('ctmSmallFixes') && this.view.item != null && box.has(this.view.item) >= 0 ? box.has(this.view.item) : this.view.index); this.cu.career.persist(); }
      this.view = null; this.ui.set('ctm-messages'); this.ui.index = 0; this.top = 0; this.ui.sync(); return;
    }
    const row = this.rows()[i]; if (!row) return;
    if (row.type === 'folder') { const box = this.inbox(); box.setPosted(row.category, !box.posted(row.category)); this.cu.career.persist(); this.ui.sync(); return; }
    if (row.type === 'message') { this.inbox().markRead(row.index); this.cu.career.persist(); }
    this.view = row; this.ui.set('ctm-message'); this.ui.index = 2; this.ui.sync();
  }
  goBack() { if (this.ui.screen === 'ctm-message') { this.view = null; this.ui.set('ctm-messages'); this.ui.index = 0; this.ui.sync(); return; } const back = this.back; this.back = null; back?.(); }
  // ---- drawing (layout from the PS2 frames local/ps2-capture/menus/lineup-messages*.png, 640x480 -> 448 lines) ----
  draw(c, b) {
    const ui = this.ui, t = this.data.texts, box = this.inbox();
    this.cu.mcommFrame(c, b);
    ui.sprite('OV_1-6', 66.5, 65.5, 142, 47, -30, Y(45), 141, Y(48));
    this.icon(c, 26.5, 51.5, 23, 15, 60, Y(56), 32, Y(22));
    ui.text(c, t.message_center, 106, Y(52), 22, '#eef4f7');
    if (ui.screen === 'ctm-message') return this.drawView(c, box);
    ui.text(c, t.total.replace('%d', box.count), 50, Y(92), 16, '#0c1a26');
    ui.text(c, t.unread.replace('%d', box.unread()), 350, Y(92), 16, '#0c1a26');
    ui.text(c, t.from_label, 110, Y(123), 15, '#0c1a26'); ui.text(c, t.subject_label, 220, Y(123), 15, '#0c1a26');
    const rows = this.rows(); this.scroll();
    rows.slice(this.top, this.top + VISIBLE).forEach((r, k) => {
      const i = this.top + k, y = Y(152 + 25 * k), on = ui.index === i, color = on ? '#eef4f7' : '#0c1a26';
      if (r.type === 'message' && !r.read) this.icon(c, 26.5, 51.5, 23, 15, 43, y + Y(4), 23, Y(15));
      const open = r.type === 'folder' && box.posted(r.category);   // OV_1-1 folder '-' / folder '?' / big '?'
      if (r.type === 'folder') this.icon(c, open ? 77.5 : 121.5, 194.5, 40, open ? 30 : 29, 44, y, 18, Y(16));
      if (r.type === 'faq') this.icon(c, 164.5, 2.5, 36, 41, 44, y - Y(4), 17, Y(26));
      if (r.type === 'message') ui.text(c, `${r.number}.`, 73, y, 15, color);
      else if (open || r.type === 'faq') ui.text(c, open ? '-' : '>', 78, y, 15, color);
      else { c.fillStyle = color; c.fillRect(79, y + Y(7), 3, Y(3)); }   // the font has no bullet glyph
      ui.text(c, r.type === 'folder' ? t.folder : box.sender(r.item), 110, y, 15, color);
      ui.text(c, r.type === 'message' ? box.subject(r.item, r.variant) : this.data.records[r.item].subject, 220, y, 15, color);
    });
    if (this.top + VISIBLE < rows.length) this.icon(c, 76.5, 34.5, 41, 25, 71, Y(349), 22, Y(22));
    if (this.top > 0) this.icon(c, 76.5, 2.5, 41, 25, 71, Y(129), 22, Y(22));
    const row = rows[ui.index];
    this.help(c, t.select_message, row?.type === 'folder' ? [['cross', box.posted(row.category) ? t.collapse : t.expand], ['triangle', t.previous]]
      : row?.type === 'message' ? [['cross', t.view], ['triangle', t.previous], ['square', t.delete]] : [['cross', t.view], ['triangle', t.previous]]);
  }
  help(c, text, buttons) {
    const ui = this.ui; c.fillStyle = '#4488b2'; c.fillRect(0, Y(382), 640, Y(64));
    ui.text(c, text, 48, Y(399), 13, '#0c1a26');
    const cell = { cross: [55, 122], triangle: [10, 122], square: [33, 122] };
    buttons.forEach(([button, label], i) => { const [u, v] = cell[button]; ui.sprite('OV_1-2', u, v, 24, 24, 453 - 13 * i, Y(388) + i * Y(17), 17, Y(17)); ui.text(c, label, 473 - 13 * i, Y(389) + i * Y(17), 13, '#e3edf3'); });
  }
  // ui.wrap without collapsing spaces (the texts keep their double spaces after a sentence).
  wrap(text, width, size) {
    const font = this.ui.fonts?.FEFONT || {}, measure = (t) => [...t].reduce((sum, ch) => sum + (font[ch]?.advance || 10) * size / 22, 0), lines = [];
    let line = null;
    for (const word of text.split(' ')) { const next = line === null ? word : line + ' ' + word; if (line !== null && line.trim() && measure(next) > width) { lines.push(line); line = word; } else line = next; }
    if (line) lines.push(line); return lines;
  }
  // A LUI shape (vertices at props 21+9k / 22+9k, alpha / r / g / b at 26..29+9k) at its parents' offset; alpha is
  // a/255 and a two-colour shape is a vertical gradient (PS2 frame samples: shadow 0.78 over the grey background).
  luiShape(c, screen, el) {
    const L = this.data.layout[screen], p = el.props, n = el.shape[0], v = [];
    let ox = 0, oy = 0; for (let q = L.find((e) => e.name === el.parent); q; q = L.find((e) => e.name === q.parent)) { ox += q.props[0]; oy += q.props[1]; }
    for (let k = 0; k < n; k++) v.push({ x: ox + p[0] + p[21 + 9 * k], y: (oy + p[1] + p[22 + 9 * k]) * 448 / 480, color: `rgba(${p[27 + 9 * k]},${p[28 + 9 * k]},${p[29 + 9 * k]},${Math.min(1, p[26 + 9 * k] / 255)})` });
    c.beginPath(); v.forEach((q, k) => (k ? c.lineTo(q.x, q.y) : c.moveTo(q.x, q.y))); c.closePath();
    if (el.shape[1] === 0) { c.strokeStyle = v[0].color; c.lineWidth = 1; c.stroke(); return; }   // outline (mode 2)
    const top = v.reduce((a, b) => (b.y < a.y ? b : a)), bottom = v.reduce((a, b) => (b.y > a.y ? b : a));
    if (top.color === bottom.color) c.fillStyle = top.color;
    else { const g = c.createLinearGradient(0, top.y, 0, bottom.y); g.addColorStop(0, top.color); g.addColorStop(1, bottom.color); c.fillStyle = g; }
    c.fill();
  }
  // 1E2DC0: the FAQ's number counts the FAQ records (the folders' questions, not the folder rows) up to it, folder by folder.
  faqNumber(item) {
    let n = 0;
    for (const c of this.data.categories) { if (c.kind !== 2 || !c.folder) continue; for (let i = c.first + 1; i < c.first + c.count; i++) { n++; if (i === item) return n; } }
    return Math.max(1, n);
  }
  measure(text, size) { const font = this.ui.fonts?.FEFONT || {}; return [...text].reduce((sum, ch) => sum + (font[ch]?.advance || 10) * size / 22, 0); }
  // 113ViewMessage: the grey background, the notched "3D Ov" frame (shadow, big and small shapes, outlines; drawn by
  // layer), the header (labels right-aligned at x 175, values at 179) and separator, the body and the buttons.
  drawView(c, box) {
    const ui = this.ui, t = this.data.texts, r = this.view; if (!r) return;
    const L = this.data.layout['113ViewMessage'];
    L.filter((e) => e.kind === 'shape').sort((a, b) => a.layer - b.layer).forEach((e) => this.luiShape(c, '113ViewMessage', e));
    // an FAQ shows kT_MSGFAQNumber 'FAQ %d' with its running FAQ number (0x1E5504 -> 1E2DC0) and no Delete (PS2 new career:
    // "Message #FAQ 1", Previous / Keep message)
    const n = r.type === 'message' ? String(r.number) : r.type === 'faq' ? (this.cu.t?.('kT_MSGFAQNumber', 'FAQ %d') || 'FAQ %d').replace('%d', String(this.faqNumber(r.item))) : t.folder;
    const subject = r.type === 'message' ? box.subject(r.item, r.variant) : this.data.records[r.item].subject;
    [[t.message_number, n], [t.from_label, box.sender(r.item)], [t.subject_label, subject]].forEach(([label, value], k) => {
      ui.text(c, label, 176 - this.measure(label, 13), Y(120 + 15 * k), 13, '#0c1a26'); ui.text(c, value, 179, Y(120 + 15 * k), 13, '#0c1a26');   // 60% text
    });
    const body = (this.data.records[r.item].body || '').replace(/\\\\/g, '\n');
    let line = 0; for (const para of body.split('\n')) for (const l of (para ? this.wrap(para, 522, 13) : [''])) { if (line < 10) ui.text(c, l, 61, Y(204) + line * Y(20), 13, '#ffffff'); line++; }   // MessageLine0: (61, 203), 522 wide, 60%
    const labels = [t.previous, t.delete_message, t.keep];
    let end = -Infinity;   // keyboard: a wide key cap (web/input-glyphs.js) moves its entry right, clear of the previous label
    [['triangle', 75, 89], ['square', 215, 229], ['cross', 415, 429]].forEach(([button, x, tx], i) => {   // buttons group (55, 420), texts (54, 409) + 11
      if (i === 1 && r.type !== 'message') return;   // Delete only for inbox messages (0x1E54A4 hides it for an FAQ)
      const [u, v] = { cross: [55, 122], triangle: [10, 122], square: [33, 122] }[button];
      const cap = glyphKeyRect(ui, 'OV_1-2', u, v, 24, 24, x - 9, Y(411), 17, Y(17)), dx = cap ? Math.max(0, end + 8 - cap.x) : 0;
      ui.sprite('OV_1-2', u, v, 24, 24, x - 9 + dx, Y(411), 17, Y(17)); ui.text(c, labels[i], tx + dx, Y(412), 13, ui.index === i ? '#ffffff' : '#dbe6ee');
      end = tx + dx + this.measure(labels[i], 13);
    });
  }
  // In-race mail icon (HUD event 8): white while the 1 s phase <= 0.5, orange after; 5 s; paused with the race.
  drawHud(c, racing = true) {
    const h = this.hud; if (!h || !this.data) return;
    const now = performance.now();
    if (racing) { h.frames += Math.min(4, Math.max(0, Math.round((now - (this.lastHud || now)) * 60 / 1000))); }
    this.lastHud = now;
    const spec = this.data.hud, color = hudTint(spec, h.frames); if (!color) { this.hud = null; return; }
    const s = this.data.sprites.mail_icon, key = color.join();
    this.tinted ??= new Map();
    let im = this.tinted.get(key);
    const src = this.ui.images[s.page];
    if (!im && src) {
      im = document.createElement('canvas'); im.width = Math.ceil(s.sw); im.height = Math.ceil(s.sh);
      const x = im.getContext('2d', { willReadFrequently: true }); x.drawImage(src, s.sx, s.sy, s.sw, s.sh, 0, 0, s.sw, s.sh);
      const d = x.getImageData(0, 0, im.width, im.height);
      for (let k = 0; k < d.data.length; k += 4) { d.data[k] *= color[0]; d.data[k + 1] *= color[1]; d.data[k + 2] *= color[2]; }
      x.putImageData(d, 0, 0); this.tinted.set(key, im);
    }
    if (im) c.drawImage(im, spec.x, Y(spec.y), s.sw, Y(s.sh));
  }
}
