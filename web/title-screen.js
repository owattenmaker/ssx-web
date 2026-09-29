// The title screen (docs/first-load.md): the original FE.LUI "06title" (the FE blue, the white ramp, two pale mountains,
// the SSX 3 logo, "Press START button" and the copyright) with the front end's falling snow (bg_snow_loop), played by
// web/lui-player.js. It is drawn from the first paint of the page (web/boot-screen.js, before the game code has
// arrived) to "Press START button", and by web/ui.js afterwards, through the same object.
//
// While the game loads, the title shows the load in the game's own widgets: the Select Character "Rider ranking" meter
// (08sel_char: the 10-cell bar widget, its black frame and cell dividers, the orange fill; the fill is the bar value,
// 0x39C428 background width * value / max) with its value text beside it as a percentage; the "Press START button" line
// stays empty meanwhile (no phase names: the user wants the title silent). When the game is ready the meter fades out and
// "Press START button" shows.
import { LuiScreen } from './lui-player.js';

import { TITLE_LUI, SNOW_LUI, SNOW_LOOP, FPS, TITLE_PAGES, FE_BLUE, SY, PRESS, METER, METER_AT, METER_FADE, titleScreenData, titleLuiScreen } from './title-data.js';
export { TITLE_LUI, SNOW_LUI, SNOW_LOOP, FPS, TITLE_PAGES, FE_BLUE, METER, METER_AT, titleScreenData, titleLuiScreen };

export class TitleScreen {
  // ui: { fonts: { FEFONT }, text(ctx, value, x, y, size, color, font, align) } (web/ui.js OriginalUI or a boot stand-in).
  constructor(data, images, ui, { t0 = 0 } = {}) {
    this.data = data; this.images = images; this.ui = ui; this.t0 = t0;
    this.lui = new LuiScreen(titleLuiScreen(data), images, ui);
    this.snowEvents = data.snow?.events || [];
    this.meterEvents = this.lui.screen.events.filter((ev) => ev.meter);
    this.readyAt = null;
  }
  frame(now) { return Math.max(0, (now - this.t0) * FPS / 1000); }
  // state: { progress: {fraction, label}, ready, error, keyboard }
  draw(c, b, now, state = {}) {
    const frame = this.frame(now), events = [];
    for (const ev of this.lui.screen.events) if (!ev.meter && ev.frame <= frame) events.push({ ev, start: ev.frame });
    const sf = frame % SNOW_LOOP;
    for (const ev of this.snowEvents) if (ev.frame <= sf) events.push({ ev, start: frame - sf + ev.frame });
    for (const ev of this.meterEvents) if (ev.frame <= frame) events.push({ ev, start: ev.frame });
    if (state.ready && this.readyAt == null) this.readyAt = frame;
    if (!state.ready) this.readyAt = null;
    const out = this.readyAt == null ? 1 : Math.max(0, 1 - (frame - this.readyAt) / METER_FADE);   // meter alpha
    const fraction = Math.max(0, Math.min(1, state.progress?.fraction ?? 0));
    const press = state.error ? state.error : state.ready ? (state.keyboard ? 'Press Enter' : null) : (state.progress?.label || 'Loading...');
    const pressAlpha = state.error || !state.ready ? 255 : 255 * (1 - out);
    b.fillStyle = FE_BLUE; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    this.lui.draw(c, events, frame, (e) => {
      // The PS2 title says nothing while it loads: the line stays empty (only the meter shows) until "Press START button".
      if (e.name === PRESS) return !state.ready && !state.error ? { hidden: true } : { ...(press != null ? { text: press } : {}), alpha: pressAlpha };
      if (!e.meter) return null;
      if (out <= 0 || state.error) return { hidden: true };
      const o = { alpha: 255 * out };
      if (e.name === METER.bar) o.fill = fraction;
      if (e.name === METER.value) o.text = `${Math.floor(fraction * 100)}%`;
      return e.name === METER.bar || e.name === METER.value || !e.parent ? o : null;
    });
    c.restore();
    return true;
  }
}
