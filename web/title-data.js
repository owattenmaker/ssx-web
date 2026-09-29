// Data of the title screen (docs/first-load.md), shared by web/title-screen.js (browser) and web/boot-plugin.js (node,
// which inlines it into index.html): no imports.
export const TITLE_LUI = '06title', SNOW_LUI = 'bg_snow_loop', SNOW_LOOP = 600, FPS = 60;
export const TITLE_PAGES = ['FE_1-18', 'FE_1-20', 'FE_1-11'];            // mountains, logo, snow flakes
export const FE_BLUE = '#75a9cb';                                      // 06title background shape (117, 169, 203)
export const SY = 448 / 480;
export const PRESS = '00798b7c';                                       // "Press START button" (menu 0c583950)
// The Rider ranking meter of 08sel_char and its value text (NumAttrOver): bar widget, background, fill, frame, dividers.
export const METER = { bar: '06cc88cc', frame: '0674fb82', cells: '0870ff82', value: '0b1cefc2' };
export const METER_AT = { x: 226, y: 334 };                            // frame top left on the title (LUI 640x480), centred
export const METER_FADE = 12;                                          // frames: the meter out, "Press START button" in

// The screens the title needs, cut from the exported front-end data (tools/export_fe_menus.py fe-menus.json and
// tools/export_character_select.py character-select.json). Small enough to inline in index.html (web/boot-plugin.js).
export function titleScreenData(feMenus, charSelect) {
  const title = feMenus?.screens?.[TITLE_LUI], snow = charSelect?.screens?.[SNOW_LUI], sel = charSelect?.screens?.['08sel_char'];
  if (!title) return null;
  let meter = null;
  if (sel) {
    const by = new Map(sel.elements.map((e) => [e.name, e])), bar = by.get(METER.bar), cells = by.get(METER.cells);
    const names = new Set([METER.bar, METER.frame, METER.cells, METER.value, bar?.bar?.background, bar?.bar?.fill, ...(cells?.children || [])]);
    const elements = sel.elements.filter((e) => names.has(e.name));
    if (elements.length === names.size) {
      const used = new Set(elements.map((e) => e.name)), events = sel.events.filter((ev) => used.has(ev.element));
      const animations = Object.fromEntries(events.filter((ev) => ev.anim && sel.animations[ev.anim]).map((ev) => [ev.anim, sel.animations[ev.anim]]));
      meter = { elements, events, animations };
    }
  }
  return { title, snow: snow ? { elements: snow.elements, events: snow.events, animations: snow.animations } : null, meter };
}

// One LUI screen: 06title, the snow (indices + 1000, as the FE screens merge it) and the meter (indices + 2000, moved to
// METER_AT; the value text leaves its 08sel_char group).
export function titleLuiScreen(data) {
  const { title, snow, meter } = data;
  // The white ramp ends on line 479 of 480: the GS fills that last line (it samples pixel corners), a canvas leaves
  // 1/15 of the bottom row blue (a blue line under the title on large screens); end it on 480.
  const toEdge = (e) => { if (e.kind !== 'shape' || !e.parent) return e; const up = title.elements.find((x) => x.name === e.parent), y0 = (up?.props?.[1] || 0) + (e.props?.[1] || 0), p = { ...e.props };
    for (let k = 0; k < (e.shape?.[0] ?? 0); k++) if (y0 + (p[22 + 9 * k] || 0) === 479) p[22 + 9 * k] += 1;
    return { ...e, props: p }; };
  const elements = title.elements.map(toEdge), animations = { ...title.animations }, events = [...(title.events || [])];
  if (snow) { elements.push(...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))); Object.assign(animations, snow.animations); }
  if (meter) {
    const frame = meter.elements.find((e) => e.name === METER.frame), fp = frame?.props || {};
    const dx = METER_AT.x - (fp[0] || 0), dy = METER_AT.y - (fp[1] || 0);
    const bar = meter.elements.find((e) => e.name === METER.bar), inBar = new Set([bar?.bar?.background, bar?.bar?.fill]);   // placed by the bar widget
    for (const e of meter.elements) {
      const p = { ...(e.props || {}) }, top = (!e.parent && !inBar.has(e.name)) || e.name === METER.value;
      if (e.name === METER.value) { p[0] = METER_AT.x + 196; p[1] = METER_AT.y + 2; p[13] = 255; }   // right of the frame, as "1.0" sits
      else if (top) { p[0] = (p[0] || 0) + dx; p[1] = (p[1] || 0) + dy; }
      const out = { ...e, index: e.index + 2000, props: p, meter: true };
      if (e.name === METER.value) delete out.parent;
      elements.push(out);
    }
    Object.assign(animations, meter.animations);
    events.push(...meter.events.map((ev) => ({ ...ev, meter: true })));
  }
  return { ...title, elements, animations, events };
}
