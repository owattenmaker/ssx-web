// Free-ride HUD (docs/peak-mountain.md "HUD"; HUD flags 0x478078[12] = 0x1530C380, 1EA930).
// 0x80: the collectible counter 21F9B0 — snowflake icon (OV_1 page 4 'hude') and "%d/%d" (0x4A2300) of the current
//       location: collected = profile record +0xC*course+4 (153520), total = byte table 0x43FA70[course] (153350);
//       drawn only while the location is loaded (22D390). Hidden while a Big Challenge runs (1EB350 clears 0x80).
// 0x100: the cash "$ n" (198AF0, thousands separators) where a race shows its score.
// No clock (0x4), no score (0x2), no place (0x1), no progress meter (0x40), no standings (0x10).
export const COLLECTIBLE_TOTALS = [30, 35, 30, 25, 30, 30, 30, 30, 2, 4, 5, 4, 5, 8, 44, 44, 44, 5, 5, 5, 5, 5]; // 0x43FA70 by course index

export const cashText = (n) => '$ ' + Math.max(0, Math.trunc(n)).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');

// Collect feedback from the score object's HUD bank (ui.lastState.trickSlots, web/trick-hud.js; PS2 runs/peak3/fr-throne-tuck
// 1605..1640): the 'Collect +$ n' popup (slot type 0x31, 1.5 s) hands its ratio r to the counter (0x1EF630 f29 -> 21F9B0 f12):
// below 0.44 the icon and "n/N" pulse (1 + sin(r*2.27296)*0.19996*(1 - (r*2.27296)^2), icon anchored top-left, the text
// after it, centred on its height) and up to 0.44 the text is green 0x4C8648; the award's flash (slot 0x18, 0.7 s) makes the
// cash green 0x4C8528 and pulsing like the race total (slot case 0x19 0x1EF850 with 0x1ECBC8's f25).
export function collectFeedback(ui) {
  const slots = ui.lastState?.trickSlots, hud = ui.trickHud; let collect = 1, flash = -1;
  if (slots) {
    for (const s of slots) if (s && s.type === 0x31) collect = Math.fround(s.value / s.maximum);
    const s18 = slots[0x18];
    if (s18 && s18.type !== 0x34) flash = Math.fround(s18.value / s18.maximum);
  }
  const f = Math.fround, pulse = (t, amplitude) => (hud?.pulse ? hud.pulse(t, amplitude) : 1);
  return { counterScale: collect < f(0.439954281) ? pulse(f(collect * f(2.27296352)), f(0.19996199)) : 1, counterGreen: !(f(0.439954281) < collect),
    cashScale: flash >= 0 ? pulse(flash) : 1, cashGreen: flash >= 0 };
}

export function drawFreeRideHud(ui, c, { course, collected, cash, challenge = false, loaded = true }) {
  if (ui.bigChallenges?.drawHud?.(ui, c)) challenge = true; // Big Challenge panel 21FD38 (web/big-challenges.js): the counter is hidden
  const fb = collectFeedback(ui);
  if (!challenge && loaded && course >= 0 && course < COLLECTIBLE_TOTALS.length) drawCollectCounter(ui, c, collected, COLLECTIBLE_TOTALS[course], fb);
  // 1EB350: a Big Challenge with a goal (builtin 98) turns the cash (0x100) off and shows its score (0x400) in that place
  const flags = ui.bigChallenges?.hudFlags?.(0x1530C380) ?? 0x1530C380;
  if (flags & 0x100) ui.text(c, cashText(cash), 602, 20, 24 * fb.cashScale, fb.cashGreen ? '#52ff5a' : '#edf5e8', 'HUDFONT', 'right');
}
// 0x80: the counter (descriptor 57: 20,20 50x64 top-left in 480 lines). PS2 frames (menus/ctm/14-after-skip-c.png,
// state-freeride-peak1.png): icon at x 20..68, y 19..77 (448 lines).
export function drawCollectCounter(ui, c, collected, total, fb = collectFeedback(ui)) {
  const k = fb.counterScale;
  ui.sprite('OV_1-4', 96, 96, 50, 50, 19, 21, 50 * k, 55 * k);
  ui.text(c, `${collected}/${total}`, 71 + 50 * (k - 1), 42 + 20 * (k - 1), 16 * k, fb.counterGreen ? '#54ff5a' : '#e6ecec', 'HUDFONT', 'left');
}

// Peak run HUD (flags 0x1530C006 | 0x22, 1EA930; the score is the trick HUD's): the clock counts down the tier limit
// (1EC3F8: int(limit/60)x60 - race ticks as HH:MM:SS of ceil(ticks/60) s, red for half of each second under 10 s),
// no progress meter or place; a station split (HUD type 0x2A time / 0x2B points) for 5 s under the clock.
export function drawPeakRunHud(ui, c, { setup, raceTicks, split }) {
  if (setup) {
    const left = Math.max(0, Math.trunc(setup.limitTicks / 60) * 60 - raceTicks), s = Math.ceil(left / 60);
    const text = `${String(Math.floor(s / 3600)).padStart(2, '0')}:${String(Math.floor(s / 60) % 60).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
    const red = s < 10 && (left % 60) >= 30;
    ui.text(c, text, 320, 20, 21, red ? '#ff4040' : '#eef5ee', 'HUDFONT', 'center');
    // a points challenge (the Jams): "GOAL: %d" (0x46EB50, 1ED4C4: the handler's target) on the clock's line from the left
    // edge (PS2 runs/allpeak/apj-full split frame: "GOAL: 1000000 00:30:29", the same glyph height)
    if (!setup.time) ui.text(c, `GOAL: ${setup.target}`, 23, 20, 21, '#eef5ee', 'HUDFONT', 'left');
  }
  if (split && performance.now() < split.until) {
    const value = setup?.time
      ? (() => {
          const a = Math.abs(split.value);
          return (
            (split.value <= 0 ? '-' : '+') +
            `${String(Math.floor(a / 3600)).padStart(2, '0')}:${String(Math.floor(a / 60) % 60).padStart(2, '0')}:${String(a % 60).padStart(2, '0')}`
          );
        })()
      : `+${split.value}`;
    ui.text(c, value, 320, 46, 18, split.ahead ? '#88c8ff' : '#ff8888', 'HUDFONT', 'center'); // 0x4C88C8 ahead, 0x4C8888 behind
  }
}
