// Big Challenges in the connected Peak 1 world (docs/peak-mountain.md "Big Challenges"): the browser side of the original
// WScript mission system (web/mission_gameplay.inc).
//  - the game update's offer-queue read (2306A8 @230890): type 8 opens the start prompt, overlay 0x1D "63bc_start"
//    (Big Challenge / title 153C88 / description 153D28 / Accept challenge? Yes / No; 29CED8 kind 4); type 9 (the challenge
//    failed: the core ran 30A868(id, 5)) opens overlay 0x1E "90bc_fail";
//  - the prompt / fail / challenge-pause (overlay 2: Restart Challenge, Quit Challenge) choices -> core mission_prompt
//    (1F75A0: 30B540 accept, 30B6F8 restart, 30B658 decline; 1F7800 retry; 20D944 -> 30B758 quit);
//  - the HUD panel (1EB350 flags 0x400000 / 0x400 / 0x800 / 0x1000 / 0x6000 from the stage-script context words, drawn by
//    1F0404..: 21FD38 badge 'BIG CHALLENGE' with the challenge number, the countdown clock, "Goal: %d", "Count: %d / %d",
//    "Height: %d.%02dm");
//  - the career save: the 88 status words (character block +0x118 + 4*index: bit0 locked follow-on, bit1 new, bit2 failed,
//    bit3 completed, bit4 available) per rider, and the completion cash (10F2D8 -> 119EF8(score, 4, cash));
//  - challenge music (29D6E0 events 33/34/38 by the challenge type, 29D8E0 / 29DBB0 event 39) through web/game-audio.js.
import { format } from './locale.js';
import { pv } from './pv-flags.js';
import { CTX } from './pause-contexts.js';

const TABLE_URL = '/assets/BIGCHAL/big-challenges.json'; // tools/export_peak_missions.py (table 0x43EE10)
// 31paus_freeride icon groups (web/ctm-pda.js ICON): the challenge pause rows
const PDA_ICONS = Object.freeze({ return: 'conticon', restart: 'rstarticon', messages: 'messicon', audio: 'radioicon', options: 'opticon', quit: 'hexicon' });
export const BC_SCREENS = Object.freeze(['ctm-bcstart', 'ctm-bcfail', 'ctm-bcpause']);
const Y = (y) => Math.round(y * 448 / 480); // 480-line PS2 frames -> 448-line UI canvas
// OVAMER / CMNAMER strings (locale hashes).
const T = Object.freeze({ bigChallenge: 0x018ab33c, accept: 0x025019dc, complete: 0x02540845, failed: 0x02141194, restart: 0x05fbe97c,
  quit: 0x0f1a485c, decline: 0x050e087c, info: 0x099250cf, retry: 0x0364db75, restartHelp: 0x0b073c9c, quitHelp: 0x0db5a79c, yes: 'kT_CMNYes', no: 'kT_CMNNo' });

// ---- table and career save ---------------------------------------------------------------------------------------------------
let tablePromise = null, table = null;
export function loadBigChallengeTable(fetcher = globalThis.fetch) {
  if (!tablePromise) tablePromise = Promise.resolve(fetcher?.(TABLE_URL)).then((r) => (r?.ok ? r.json() : null)).catch(() => null).then((d) => { table = d?.challenges ?? null; return table; });
  return tablePromise;
}
export function setBigChallengeTable(rows) { table = rows; tablePromise = Promise.resolve(rows); }
// The loaded rows (null until loadBigChallengeTable resolves): web/career.js counts a peak's completed challenges with them (1544D0).
export const bigChallengeRows = () => table;
// 151600: a new character's status word: bit0 = flags byte +0x1A (follow-on of a chain), bit1 = 1, bit4 = flags byte +0x1B (first of a chain).
export const initialStatus = (flags) => (((flags >>> 16) & 1) | 2 | (((flags >>> 24) & 1) << 4)) >>> 0;
export function statusWords(career, riderId, rows = table) {
  const r = career.rider(riderId);
  if (!rows) return r.bigChallenges ?? [];
  if (!Array.isArray(r.bigChallenges) || r.bigChallenges.length !== rows.length) r.bigChallenges = rows.map((x) => initialStatus(x.flags));
  return r.bigChallenges;
}
export function setStatusWord(career, riderId, index, word) { const w = statusWords(career, riderId); if (index >= 0 && index < w.length) { w[index] = word >>> 0; career.persist(); } }
// A status word from the core (1540F0 / 154160 ...). 307308 -> 10F2D8 -> 159B08: a completed challenge that brings its peak a
// new Big Challenge medal can complete the Freeride goal (web/career.js exploreGoal: the goal award, the next peak's pass).
export function challengeStatus(career, riderId, index, word, rows = table) {
  const x = rows?.[index], peak = x && x.course >= 0 && career.courses?.[x.course] ? career.peakOf(x.course) : 0;
  const before = peak && career.challengeMedal ? career.challengeMedal(riderId, peak) : null;
  setStatusWord(career, riderId, index, word);
  if (peak && career.challengeMedal && career.challengeMedal(riderId, peak) !== before) { career.exploreGoal(riderId, peak, 'challenge'); career.persist(); }   // 159B08
}
// 1542E0 / 1542A0: completed (bit3) / count of a course's challenges (a number), of a peak ({peak}: flags low byte), or all.
export function bigChallengeCounts(career, riderId, where = null) {
  if (!table) { loadBigChallengeTable(); return null; }
  const w = career && riderId ? statusWords(career, riderId) : table.map((x) => initialStatus(x.flags));
  const pick = where == null ? () => true : typeof where === 'number' ? (x) => x.course === where : (x) => (x.flags & 0xff) === where.peak;
  let count = 0, done = 0; table.forEach((x, i) => { if (pick(x)) { count++; if (w[i] & 8) done++; } });
  return { done, count };
}
// INFO panel text: "%d / %d", "N/A" without challenges (207430).
export function bigChallengeInfo(career, riderId, where = null) { const c = bigChallengeCounts(career, riderId, where); return !c ? '' : c.count ? `${c.done} / ${c.count}` : 'N/A'; }

// ---- challenge clock (1EB584: ctx+0x20 ticks, mode flags 0xC: hundredths) -------------------------------------------------------
export function challengeClock(ticks) {
  if (!(ticks > 0)) return '00:00:00.00';
  const f = Math.fround(Math.fround(ticks) * Math.fround(0.01666666753590107)), s = Math.trunc(f); // gp-0x5738, cvt.w.s
  const hund = Math.trunc(Math.fround(Math.fround(f - Math.fround(s)) * 100));
  const pad = (x) => String(x).padStart(2, '0');
  return `${pad(Math.trunc(s / 3600))}:${pad(Math.trunc(s % 3600 / 60))}:${pad(s % 60)}.${pad(hund)}`;
}
// "Height: %d.%02dm" (309BA8: rider height over the reference instance, cm).
export const heightText = (cm) => { const v = Math.max(0, Math.trunc(cm)); return `Height: ${Math.trunc(v / 100)}.${String(v % 100).padStart(2, '0')}m`; };

// ---- runtime ----------------------------------------------------------------------------------------------------------------
export async function createBigChallenges({ core, ui, gameAudio = null, riderId = () => null, careerUI = () => ui.careerUI }) {
  if (!core?._mission_hud) return null;
  const rows = await loadBigChallengeTable();
  // Stage builtin 59 (0x3032C0, the tick's distance |rider+0x1E0| x 1/60) answers (the Kick Doubt Grinder challenges);
  // set in the human's context at every tick, where the WScript tick runs
  const speedBuiltin = 1;
  const lifecycle = pv('bcDecline') ? 7 : 0; // core mission_lifecycle: 1235F8 reset, builtin 67's 30B7F8, WS10 enter's 308988
  if (!rows) return null;
  const I32 = (p, n) => new Int32Array(core.HEAPU8.buffer, p, n);
  const byId = new Map(rows.map((r, i) => [r.id >>> 0, i]));
  const t = (key, fallback) => careerUI()?.t?.(key, fallback) ?? fallback;
  let prompt = null, banner = null, lastHud = null, statusSent = false, badgeId = null; // badgeId: HUD +0x18C, the last running challenge
  const career = () => careerUI()?.career ?? null;
  function sendStatus() {
    const c = career(), id = riderId(); if (!c || !id) return;
    statusWords(c, id).forEach((w, i) => core._mission_set_status(i, w >>> 0));
    statusSent = true;
  }
  function hud() {
    const v = I32(core._mission_hud(), 24);
    const out = {
      active: v[0] >>> 0,
      running: !!v[1],
      state: v[2],
      task: v[3],
      tasks: v[4],
      words: Array.from(v.slice(5, 21)),
      height: new Float32Array(v.buffer, v.byteOffset + 21 * 4, 1)[0],
      called: ''
    };
    if (out.words[13] && core._mission_called_trick) { const H = core.HEAPU8; let p = core._mission_called_trick(); while (H[p]) out.called += String.fromCharCode(H[p++]); }
    return out;
  }
  function row(id) { const i = byId.get(id >>> 0); return i == null ? null : { index: i, ...rows[i] }; }
  function openPrompt(id, from) {
    prompt = { id: id >>> 0, from };
    hold(); ui.set('ctm-bcstart'); ui.index = 0; ui.sync();
  }
  // The offer (overlay 0x1D) and fail (0x1E) prompts push pause context 3 (0x2308B4 / 0x230954) without the audio pause 289B70: the
  // music and SFX keep playing under them (audio +0x5FB4 = 0 in the prompt state; docs/ctm-decomp-freeride.md). This module holds the
  // handle and pops it when the prompt returns to the ride (web/pause-contexts.js).
  let promptCtx = null;
  function hold() {
    promptCtx ??= ui.contexts.push(CTX.PROMPT, { owner: 'Big Challenge prompt', audio: false, ends: (s) => s === 'game' });
  }
  // back to the ride: the prompt's own context, then the pause menu's if the prompt came from it (Restart Challenge)
  function resume() { ui.set('game'); ui.contexts?.pop(promptCtx); promptCtx = null; ui.cb.resume?.(); }
  function drainEvents() {
    const p = core._mission_ui_events() >> 2, H = new Int32Array(core.HEAPU8.buffer), n = H[p];
    const c = career(), id = riderId();
    for (let k = 0; k < n; k++) {
      const [kind, a, b] = [H[p + 1 + 3 * k], H[p + 2 + 3 * k], H[p + 3 + 3 * k]];
      if (kind === 5 && c && id)
        challengeStatus(c, id, a, b); // status word
      else if (kind === 6 && c && id && a > 0) {
        if (c.earnCash) c.earnCash(id, a);
        else {
          const r = c.rider(id);
          r.cash += a;
          r.earned += a;
        }
        c.persist();
      } // 119EF8 kind 4 -> 1597B0 (web/career.js earnCash)
      else if (kind === 2)
        banner = { text: 'MISSION SUCCESS', cash: b, until: performance.now() + 1500 }; // HUD popup 0x1B (11A110: 1.5 s; string at ELF 0x36FC18)
      else if (kind === 1)
        gameAudio?.challengeStart?.((row(a)?.misc >>> 16) & 0xffff); // 29D6E0: type = row +0x22
      else if (kind === 3)
        gameAudio?.challengeEnd?.(); // 29D8E0
      else if (kind === 4)
        gameAudio?.challengeStop?.(b === 0); // 29DBB0(audio, 0 fail / 1)
      else if (kind === 7) gameAudio?.challengeAccepted?.();                                           // 29D6D0 (+0x5FD8, bigChallengeAudio)
    }
  }
  const api = {
    rows, row,
    // Per game tick, after race_end (main.js simTick): the status words once, the queue read, the UI events.
    tick() {
      if (!statusSent) sendStatus();
      core._mission_speed_builtin?.(speedBuiltin);
      core._mission_lifecycle?.(lifecycle);
      const q = I32(core._mission_offer_peek(), 3), type = q[0], id = q[1] >>> 0;
      drainEvents();
      if (type === 8) openPrompt(core._mission_offer_pop() >>> 0, 'offer');
      else if (type === 9) { prompt = { id, from: 'fail' }; hold(); ui.set('ctm-bcfail'); ui.index = 2; ui.sync(); } // overlay 0x1E opens on Challenge Info
      lastHud = hud();
    },
    running: () => !!core._mission_running?.(),
    // 1EB350: 0x400000 while a Big Challenge runs (counter 0x80 off); goal 0x400 (and 0x302 off), count 0x800, height 0x1000, 0x6000.
    hudFlags(flags) {
      const h = lastHud; if (!h?.running) return flags;
      let f = (flags | 0x400000) & ~0x80;
      if (h.words[1]) f = (f | 0x400) & ~0x302; if (h.words[4]) f |= 0x800; if (h.words[10]) f |= 0x1000; if (h.words[13]) f |= 0x6000;
      return f >>> 0;
    },
    // Challenge panel (21FD38 and the elements under it). Returns true while a challenge runs (the counter is hidden).
    drawHud(u, c) {
      const h = lastHud ?? hud(); const now = performance.now();
      // the trick HUD draws MISSION SUCCESS itself (big message 0x3A, web/trick-hud.js) once its data has the record
      if (banner && now < banner.until && !u.trickHud?.sprites?.go) {
        // PS2 bigchal/sd-complete20.png: "MISSION SUCCESS" / "$ 2,000" small under the clock
        u.text(c, banner.text, 320, Y(126), 13, '#e6ecec', 'HUDFONT', 'center');
        // 1F09D0 skips the cash line when the award is 0 (blez +0x3C8: a completed challenge repeated pays $0)
        if (banner.cash > 0) u.text(c, '$ ' + String(Math.max(0, banner.cash)).replace(/\B(?=(\d{3})+(?!\d))/g, ','), 320, Y(140), 13, '#e6ecec', 'HUDFONT', 'center');
      }
      // 1EB350: flag 0x400000 (the badge) while a challenge runs (+0x18C = its id), and also while the MISSION SUCCESS popup
      // (score slot 0x1B, 1.5 s) lives after it: the badge stays with the counter (0x80, back on) drawn over it (PS2
      // ctm-parity/runs/sd-goal s150..221: the snowflake and "0/30" over the badge, the disk now green).
      if (h?.running) badgeId = h.active >>> 0;
      const s1B = u.lastState?.trickSlots?.[0x1B], success = !h?.running && badgeId != null && !!s1B && s1B.type !== 0x34;
      if (!h?.running && !success) return false;
      const r = row(badgeId);
      // Badge 21FD38: sprite owner +0x4CC (OV_1 'hud ' page, atlas 4..150 x 200..255) at descriptor 69 (x 20, y 19 of the PS2 frame).
      u.sprite('OV_1-3', 4, 200, 146, 55, 20, Y(19), 146, Y(55));
      // +0x4D0, the disk behind the number: 0x4C8848 (red) for a challenge not yet completed, 0x4C8828 (A 1, R 0.3244, G 1, B 0)
      // once its status word has bit 3 (153D78), with the challenge number (154240: row + 1) in it (PS2 frame: disk r 17 at 43, 43)
      const cc = career(), who = riderId(), done = !!r && !!cc && !!who && !!((statusWords(cc, who, rows)[r.index] ?? 0) & 8);
      c.save(); c.fillStyle = done ? '#53ff00' : '#ff0000'; c.beginPath(); c.ellipse(43, Y(43), 17, Y(17), 0, 0, Math.PI * 2); c.fill(); c.restore();
      if (r) u.text(c, String(r.index + 1), 43, Y(35), 14, '#e8e4e4', 'HUDFONT', 'center');
      if (success) return false;
      const w = h.words;
      if (w[7]) { // timer: ctx+0x20 ticks; red at or under 0, and for half of each second under 10 s when +0x24 < 0
        const ticks = w[8], red = ticks <= 0 || (w[9] < 0 && ticks < 601 && (ticks % 60) >= 30);
        u.text(c, challengeClock(ticks), 320, Y(20), 21, red ? '#ff2020' : '#eef5ee', 'HUDFONT', 'center');
      }
      let y = Y(98);
      if (w[1]) { u.text(c, format('GOAL: %d', w[3]), 22, y, 13, '#e6ecec', 'HUDFONT', 'left'); y += Y(20); }
      if (w[4]) { u.text(c, format('COUNT: %d / %d', w[5], w[6]), 22, y, 13, '#e6ecec', 'HUDFONT', 'left'); y += Y(20); }
      if (w[10]) { u.text(c, heightText(h.height).toUpperCase(), 22, y, 13, '#e6ecec', 'HUDFONT', 'left'); }
      // 0x6000: the called trick (builtin 83 -> C+0x34 / +0x38) named by 116950, red at the bottom centre
      // (PS2 ctm-parity/runs/dizzy: "180" at x 299..336, y 381..395 of the 480-line frame, (200, 0, 0))
      if (w[13] && h.called) u.text(c, h.called, 320, Y(381), 17, '#c80000', 'HUDFONT', 'center');
      return true;
    },
    // Start during a Big Challenge opens overlay 2 (0x20CAA4: 30B8C0), not MCOMM.
    pauseScreen: () => (core._mission_running?.() ? 'ctm-bcpause' : 'pause'),
    // ---- overlays, delegated by web/career-ui.js --------------------------------------------------------------------------
    owns: (screen) => BC_SCREENS.includes(screen),
    overWorld: (screen) => screen === 'ctm-bcstart' || screen === 'ctm-bcfail', // drawn over the paused world (overlay 2 is full screen)
    items(screen) {
      if (screen === 'ctm-bcstart') return [t(T.yes, 'Yes'), t(T.no, 'No')];
      if (screen === 'ctm-bcfail') return [t(T.yes, 'Yes'), t(T.no, 'No'), t(T.info, 'Challenge Info')]; // PS2 bigchal/sd-run-stop.png
      if (screen === 'ctm-bcpause') return ['Return', t(T.restart, 'Restart Challenge'), 'Messages', 'Audio', 'Options', t(T.quit, 'Quit Challenge')]; // overlay 2 (PS2 bigchal/sd-pause.png)
      return [];
    },
    disabled: (screen, i) => screen === 'ctm-bcpause' && ((i === 2 && !careerUI()?.messages?.ready) || (i === 3 && !ui.audioMenus?.ready)),
    layout(screen, i) {
      // 63bc_start menu (270, 295) + yes / no (25, 0 / 22); 90bc_fail (202, 249) + Menu0000 (11, 11) + yes / no / info (25, 0 / 20 / 40)
      if (screen === 'ctm-bcstart') return careerUI()?.pda?.lui?.['63bc_start'] ? [280, Y(295 + 22 * i), 110, Y(20)] : [270, Y(294) + i * Y(21), 120, Y(21)];
      if (screen === 'ctm-bcfail') return careerUI()?.pda?.lui?.['90bc_fail'] ? [223, Y(260 + 20 * i), 160, Y(20)] : [214, Y(256) + i * Y(21), 200, Y(21)];
      return [195, Y(94) + i * Y(40), 240, Y(34)];
    },
    key: () => false,
    choose(i) {
      const s = ui.screen, id = prompt?.id ?? (lastHud?.active ?? 0);
      if (s === 'ctm-bcstart') { core._mission_prompt(i === 0 ? 1 : 2, id | 0); prompt = null; resume(); return; }
      if (s === 'ctm-bcfail') {
        if (i === 0) { core._mission_prompt(4, id | 0); prompt = null; resume(); }
        else if (i === 1) { core._mission_prompt(2, id | 0); prompt = null; resume(); }
        else { prompt = { id, from: 'fail' }; ui.set('ctm-bcstart'); ui.index = 0; ui.sync(); } // "info": overlay 0x1D
        return;
      }
      if (s === 'ctm-bcpause') {
        const back = (k) => () => { ui.set('ctm-bcpause'); ui.index = k; ui.sync(); };
        // 20D944 -> 30B758
        if (i === 0) resume();
        else if (i === 1) { const go = () => { prompt = { id: lastHud?.active ?? id, from: 'pause' }; ui.set('ctm-bcstart'); ui.index = 0; ui.sync(); }; // 20D8F4: overlay 0x1D
          if (careerUI()?.bcConfirm) careerUI().bcConfirm(ui.items()[1], go, back(1)); else go(); }   // "Are you sure?" first
        else if (i === 2) careerUI()?.messages?.open?.(back(2));
        else if (i === 3) ui.audioMenus?.open?.('audio', { back: back(3) });
        else if (i === 4) { ui.optionsReturn = 'ctm-bcpause'; ui.set('options'); }
        else { const go = () => { core._mission_prompt(3, 0); resume(); }; if (careerUI()?.bcConfirm) careerUI().bcConfirm(ui.items()[5], go, back(5)); else go(); }
      }
    },
    back() {
      const s = ui.screen, id = prompt?.id ?? 0;
      if (s === 'ctm-bcstart') { core._mission_prompt(2, id | 0); prompt = null; resume(); return; } // 1F75A0 event 6: decline
      if (s === 'ctm-bcfail') { core._mission_prompt(2, id | 0); prompt = null; resume(); return; }
      resume();
    },
    // Overlay 0x1D (PS2 local/ps2-capture/menus/bigchal/sd1-prompt.png): an opaque blue panel over the paused world.
    draw(c, bg = null) {
      const s = ui.screen, r = row(prompt?.id ?? lastHud?.active ?? 0);
      const panel = (x0, y0, x1, y1) => {
        c.fillStyle = '#31516a'; c.fillRect(x0, Y(y0), x1 - x0, Y(y1) - Y(y0));
        c.fillStyle = '#34637f'; c.fillRect(x0 + 10, Y(y0 + 9), x1 - x0 - 20, Y(y1 - 9) - Y(y0 + 9));
        c.fillStyle = '#3e7aa2'; c.fillRect(x0 + 22, Y(y0 + 19), x1 - x0 - 44, Y(y1 - 20) - Y(y0 + 19));
      };
      const lines = (text, y, colour, size = 17) => {
        for (const l of ui.wrap(String(text ?? '').replace(/\n/g, ' '), 470, size)) {
          ui.text(c, l, 320, y, size, colour, 'FEFONT', 'center');
          y += Y(22);
        }
        return y;
      };
      const pda = careerUI()?.pda;
      // overlay 0x1D / 0x1E from OV.LUI (tools/export_ctm_screens.py: 63bc_start, 90bc_fail)
      if (
        s === 'ctm-bcstart' &&
        pda?.lui?.['63bc_start'] &&
        pda.popup(c, '63bc_start', 60, [65, 70], ui.index, {
          title: t(T.bigChallenge, 'Big Challenge'),
          chalname: r ? t(r.title, '') : '',
          ObjText: r ? String(t(r.description, '')).replace(/\n/g, ' ') : '',
          '006c02b4': t(T.accept, 'Accept challenge?'),
          yes: t(T.yes, 'Yes'),
          no: t(T.no, 'No')
        })
      )
        return;
      if (
        s === 'ctm-bcfail' &&
        pda?.lui?.['90bc_fail'] &&
        pda.popup(c, '90bc_fail', 60, [65, 70, 75], ui.index, {
          '0e82cadc': t(T.failed, 'Challenge failed'),
          '0c2684a5': t(T.retry, 'Retry?'),
          yes: t(T.yes, 'Yes'),
          no: t(T.no, 'No'),
          info: { text: t(T.info, 'Challenge Info'), props: { 6: 300 } }
        })
      )
        return;
      if (s === 'ctm-bcstart') {
        panel(31, 93, 606, 366);
        ui.text(c, t(T.bigChallenge, 'Big Challenge'), 320, Y(130), 20, '#ffffff', 'FEFONT', 'center');
        ui.text(c, r ? t(r.title, '') : '', 320, Y(160), 18, '#a9d7f0', 'FEFONT', 'center');
        lines(r ? t(r.description, '') : '', Y(182), '#ffffff');
        ui.text(c, t(T.accept, 'Accept challenge?'), 320, Y(266), 18, '#ffffff', 'FEFONT', 'center');
        ui.items().forEach((label, i) => {
          const y = Y(294) + i * Y(21);
          if (ui.index === i) ui.sprite('OV_1-2', 55, 122, 24, 24, 272, y, 18, 18);
          ui.text(c, label, 296, y, 18, ui.index === i ? '#f2f4f6' : '#0c1a26');
        });
        return;
      }
      if (s === 'ctm-bcfail') { // overlay 0x1E (PS2 local/ps2-capture/menus/bigchal/sd-run-stop.png): a smaller panel, dark text
        panel(132, 142, 504, 341);
        ui.text(c, t(T.failed, 'Challenge failed'), 320, Y(180), 18, '#0c1a26', 'FEFONT', 'center');
        ui.text(c, t(T.retry, 'Retry?'), 320, Y(222), 18, '#0c1a26', 'FEFONT', 'center');
        ui.items().forEach((label, i) => {
          const y = Y(256) + i * Y(21);
          if (ui.index === i) ui.sprite('OV_1-2', 55, 122, 24, 24, 214, y, 18, 18);
          ui.text(c, label, 238, y, 18, ui.index === i ? '#f2f4f6' : '#0c1a26');
        });
        return;
      }
      // Overlay 2 (challenge pause, PS2 bigchal/sd-pause.png): the MCOMM frame and list of web/career-ui.js.
      const cu = careerUI(), b = bg ?? ui.bg;
      const help = [
        cu?.t?.('kT_OVRHELPGetBoarding'),
        t(T.restartHelp, 'Restart the current challenge.'),
        cu?.t?.('kT_OVRHELPMessages'),
        cu?.t?.('kT_OVRHELPChangeMusic'),
        cu?.t?.('kT_OVRHELPOptions'),
        t(T.quitHelp, 'Quit out of the current challenge.')
      ][ui.index];
      // 31paus_freeride with the challenge rows' icons (table 0x441C30 by item id: cont, rstart, mess, radio, opt, hex; PS2 bigchal/sd-pause.png)
      if (cu?.pda?.ready && cu.mcommFrame && b) {
        cu.mcommFrame(c, b); const I = PDA_ICONS;
        cu.pda.menu(
          c,
          ui
            .items()
            .map((label, i) => ({
              label,
              icon: [I.return, I.restart, I.messages, I.audio, I.options, I.quit][i],
              disabled: this.disabled('ctm-bcpause', i)
            })),
          ui.index,
          help || ''
        );
        return;
      }
      if (cu?.mcommFrame && b) { cu.mcommFrame(c, b); b.fillStyle = '#f2f5f6'; b.fillRect(112, Y(92), 62, Y(282)); }
      ui.items().forEach((label, i) => ui.text(c, label, 200, Y(98) + i * Y(40), 21, ui.index === i ? '#f4f6f2' : this.disabled('ctm-bcpause', i) ? '#51708a' : '#0f2533'));
      cu?.help?.(c, help || '');
    },
    debug: () => ({ hud: hud(), info: Array.from(I32(core._mission_info(), 10)), prompt }),
  };
  ui.bigChallenges = api;
  return api;
}
