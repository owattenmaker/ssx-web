// Game motion remains 60 Hz regardless of the renderer's cadence. Limit work
// per callback, but retain debt so a slow frame never deletes game time.
export class FixedStepClock {
  // While a clock runs its ticks: how many ticks of this advance() are left, the running one included (1 = the last tick
  // before the frame draws); null outside advance(). Presentation-only work (computer riders' skin palettes, web/ai-race.js)
  // needs only the last two ticks of a frame.
  static ticksLeft = null;
  constructor() { this.pending = 0; this.step = 1 / 60; }
  reset() { this.pending = 0; }
  advance(seconds, tick) {
    if (!Number.isFinite(seconds) || seconds < 0) throw new Error('Invalid frame duration');
    this.pending += seconds;
    // The same arithmetic as the tick loop, to know the count first.
    let planned = 0;
    for (let p = this.pending; p + 1e-10 >= this.step && planned < 12; p = Math.max(0, p - this.step)) planned++;
    let ticks = 0;
    const outer = FixedStepClock.ticksLeft;
    try {
      while (this.pending + 1e-10 >= this.step && ticks < 12) {
        FixedStepClock.ticksLeft = planned - ticks;
        tick();
        this.pending = Math.max(0, this.pending - this.step);
        ticks++;
      }
    } finally { FixedStepClock.ticksLeft = outer; }
    return ticks;
  }
}
