// Peak challenges (Peak 1 Race / Peak 1 Jam and the other peaks' runs; docs/peak-mountain.md "Peak 1 Race / Peak 1 Jam").
// Pure rules of the GameModeMan handlers 4 (time challenge, modes 6..8: init 23B268, split 23B5F8, results 23B468) and
// 7 (points challenge, modes 9..11: init 23C0D0, split 23C560, results 23C2D8) over table 0x440D18 (career.json
// rules.peak_challenges: 18 rows {mode, tier 1..3, target, limit s, split[5], cash/100}).
import { MEDAL } from './career.js';

export const isPeakRun = (mode) => mode >= 6 && mode <= 11;
export const isTimeChallenge = (mode) => mode >= 6 && mode <= 8;
// Event kind 0x535C10 (144DF0) of a peak run: 5 time challenge, 6 points challenge.
export const peakKind = (mode) => (isTimeChallenge(mode) ? 5 : 6);
// Start course 0x535C08 (the objectives card's load): the backcountry at the top of the peak (Happiness 14, Ruthless 15,
// The Throne 16), and the finish course the finish rule 10E5D8 accepts (goal-list course / GMM+0x6C).
export const PEAK_RUNS = {
  // finish: the goal-list course of the run (0x45AAD8; 10E5D8 accepts a finish line only on course 1 or 5..13): the All
  // Peak Race and Jam both end at Metro-City (docs/peak3.md; 4 was GMM+0x6C, which has no reader), and so does the Peak 2 Race
  // (Intimidator, course 3, is not accepted: DBC2 -> D -> DRA4 -> A -> ARA1 -> B -> BRA2, docs/peak2.md)
  // route: the course rows (0x535C08) a run crosses, in order (each connector's Unload trigger requests the next one; the
  // stations 17..21 are the splits); the whole mountain prefetches the rows ahead on it (web/free-ride.js). PS2: the All Peak
  // Race capture (docs/peak3.md section 6) crosses 16 -> 21 -> 4 -> 19 -> 2 -> 20 -> 3 -> 17 -> 0 -> 18 -> 1.
  6: { start: 14, finish: 1, name: 'Peak 1 Race', route: [14, 17, 0, 18, 1] }, 7: { start: 15, finish: 1, name: 'Peak 2 Race', route: [15, 20, 3, 17, 0, 18, 1] },
  8: { start: 16, finish: 1, name: 'All Peak Race', route: [16, 21, 4, 19, 2, 20, 3, 17, 0, 18, 1] },
  9: { start: 14, finish: 5, name: 'Peak 1 Jam', route: [14, 17, 5] }, 10: { start: 15, finish: 6, name: 'Peak 2 Jam', route: [15, 20, 6] },
  11: { start: 16, finish: 1, name: 'All Peak Jam', route: [16, 21, 4, 19, 2, 20, 3, 17, 0, 18, 1] },
};

const row = (rules, mode, tier) => rules.peak_challenges[(mode - 6) * 3 + tier];

// 23B268 / 23C0D0: the tier from the stored best medal (1558F8): time: gold or silver -> 2, bronze -> 1, none (or
// platinum) -> 0; points: platinum, gold or silver -> 2, bronze -> 1, none -> 0.
export function peakTier(mode, storedMedal) {
  if (isTimeChallenge(mode)) return storedMedal === MEDAL.GOLD || storedMedal === MEDAL.SILVER ? 2 : storedMedal === MEDAL.BRONZE ? 1 : 0;
  return storedMedal === MEDAL.PLATINUM || storedMedal === MEDAL.GOLD || storedMedal === MEDAL.SILVER ? 2 : storedMedal === MEDAL.BRONZE ? 1 : 0;
}

// The run's setup: GMM+0x78 time limit (ticks; timed +0x88 = 1), the target (time challenge: the limit itself; points
// challenge: handler+4 = target x 100 points), the station splits of the tier row (145668).
export function peakSetup(rules, mode, storedMedal) {
  const tier = peakTier(mode, storedMedal), r = row(rules, mode, tier);
  const time = isTimeChallenge(mode);
  return { mode, tier, time, limitTicks: (time ? r[2] : r[3]) * 60, target: time ? r[2] * 60 : r[2] * 100,
    splits: r.slice(4, 9), cash: r[9] * 100, run: PEAK_RUNS[mode] };
}

// 23B5F8 / 23C560 (238510 from the ride state's leave handler 235868 in a peak run, current course a station 17..21):
// the difference to the next split of the tier row, shown for 5 s (HUD 0x2A time H:MM:SS / 0x2B "+%d"); a zero split
// shows nothing; the split counter handler+8 advances either way.
// time: int(float(raceTicks) * 0.016666668) - split (cvt.w.s truncates); points: score - split x 100.
export function peakSplit(setup, counter, { raceTicks = 0, score = 0 }) {
  const split = setup.splits[counter] ?? 0;
  if (!split) return { counter: counter + 1, shown: false };
  const value = setup.time ? Math.trunc(Math.fround(raceTicks * 0.01666666753590107)) - split : Math.trunc(score) - split * 100;
  return { counter: counter + 1, shown: true, value, ahead: setup.time ? value <= 0 : value >= 0 };
}

// 23B468 / 23C2D8: +0x480 (time up / give up) fails; the time challenge fails above the current tier's limit and the
// points challenge below its target; otherwise place 0 (gold) / 1 (silver) / 2 (bronze) against the tier-3 and tier-2
// rows. Returns {place (3 = no medal), fail, value}.
export function peakResult(rules, setup, { ticks = 0, score = 0, dnf = false }) {
  const { mode } = setup;
  if (isTimeChallenge(mode)) {
    const value = dnf ? 360000 : ticks;
    if (dnf || ticks > setup.target) return { place: 3, fail: true, value };
    return { place: ticks <= row(rules, mode, 2)[2] * 60 ? 0 : ticks <= row(rules, mode, 1)[2] * 60 ? 1 : 2, fail: false, value };
  }
  const value = dnf ? 0 : Math.trunc(score);
  if (dnf || value < setup.target) return { place: 3, fail: true, value };
  return { place: value >= row(rules, mode, 2)[2] * 100 ? 0 : value >= row(rules, mode, 1)[2] * 100 ? 1 : 2, fail: false, value };
}

// HUD 0x2A split value: H:MM:SS of |d| with the sign glyph (1F1840 t1 = 4 / 2), colour 0x4C88C8 ahead / 0x4C8888 behind.
export function splitText(split, time) {
  if (!time) return `+${split.value}`;
  const s = Math.abs(split.value), text = `${String(Math.floor(s / 3600)).padStart(2, '0')}:${String(Math.floor(s / 60) % 60).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
  return (split.value <= 0 ? '-' : '+') + text;
}
