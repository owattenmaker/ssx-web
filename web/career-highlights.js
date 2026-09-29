// Career Highlights page of the original 35car_stat screen (DATA/UI/FE.LUI, tools/export_career_highlights.py), drawn with
// web/lui-player.js and filled as 0x1F5DA0 does (docs/tricks-scoring.md "Career Highlights"): per row r = 1..3, item
// p = top + r - 1 (stat p / 3, tier p % 3):
//   hl<r>        "%s%d" % (0x441BA0[stat], tier + 1) (kT_STATStayOnRail1 = "Stay on a rail - 25m" ...)
//   checkmark<r> shown and checkbox<r> hidden when tier < medal[stat], else the reverse
//   hlsec<r>     "%s Monster Trick" (0x46F2A8) with the monster name (116950 of {0, id << 27}, trailing space)
//   hlsec<r>a    the trick (116E08) when unlocked, else kT_FEUnlockMonsterTrick
//   HL_arrowup when top > 0, HL_arrowdown when top + 3 < 24.
// Timeline: the intro events through frame 25 (label 004d81c3 sets the row elements) and the Highlights section label at
// frame 35 (0f220013, the rows' group).
import { LuiScreen } from './lui-player.js';
import { MONSTER_LIST, monsterById } from './monster-tricks.js';

const STATS = ['kT_STATStayOnRail', 'kT_STATHoldHandplt', 'kT_STATStayInAir', 'kT_STATKOPeopleRace', 'kT_STATDoUberGrind', 'kT_STATDoSupUber', 'kT_STATGetPoints', 'kT_CMNDoXCombo'];
export const HIGHLIGHT_PAGES = ['FE_1-7', 'FE_1-9', 'FE_1-11', 'FE_1-14', 'FE_1-18'];
const SECTION_FRAME = 35;
// 0x1F5A38 (page switch): hides the page groups "highlights" 0f220013, "race_platinum" 0279a8bd, "freestyle_platinum" 0e6d9bdd and
// "ridersbest" 0c9693e4, shows the current page's, and sets "horizontal dash" 05c5de78 / "vertical dash" 0e75f658 visible on every
// page but Highlights (page 0).
const OTHER_SECTIONS = ['0e6d9bdd', '0279a8bd', '0c9693e4', '05c5de78', '0e75f658'];

// Row contents for list position p and the rider's eight medals.
export function highlightRow(data, medals, p) {
  const id = MONSTER_LIST[p], m = monsterById(id), stat = Math.floor(p / 3), tier = p % 3, on = tier < (medals?.[stat] ?? 0);
  return { id, on, label: data.strings[`${STATS[stat]}${tier + 1}`], title: `${m.name}  Monster Trick`, text: on ? m.trick : data.strings.kT_FEUnlockMonsterTrick };
}

export class CareerHighlights {
  constructor(data, images, ui) { this.data = data; this.lui = new LuiScreen(data.screen, images, ui); this.names = data.names; }
  events() { const out = []; for (const ev of this.data.screen.events) if (ev.frame <= 25 || ev.frame === SECTION_FRAME) out.push({ ev, start: ev.frame }); return out; }
  // c: a 640x480 LUI frame (the caller scales to its canvas); medals: the rider's 0x155390 bytes; top: first row's p.
  draw(c, medals, top) {
    const n = this.names, rows = [0, 1, 2].map((r) => (top + r < 24 ? highlightRow(this.data, medals, top + r) : null));
    const by = {}; rows.forEach((row, r) => { const k = r + 1;
      by[n[`hl${k}`]] = row ? { text: row.label } : { hidden: true };
      by[n[`hlsec${k}`]] = row ? { text: row.title } : { hidden: true };
      by[n[`hlsec${k}a`]] = row ? { text: row.text } : { hidden: true };
      by[n[`checkmark${k}`]] = row && row.on ? {} : { hidden: true };
      by[n[`checkbox${k}`]] = row && !row.on ? {} : { hidden: true }; });
    for (const g of OTHER_SECTIONS) by[g] = { hidden: true }; // the other pages' groups (platinum freestyle / race medals, best in event)
    by[n.HL_arrowup] = top > 0 ? {} : { hidden: true };
    by[n.HL_arrowdown] = top + 3 < 24 ? {} : { hidden: true };
    this.lui.draw(c, this.events(), 60, (e) => by[e.name] || null);
    return rows;
  }
}
