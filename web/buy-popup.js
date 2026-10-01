// The lodge's buy popups as the PS2 draws them: cUIStateBuyPopup (0x1CABA8, "UITRICKBUY") shows FE.LUI 139buy_popup (0x1CAC30)
// for Buy Attributes (0x1CAFC0), Ubertrick Setup (initBuyTrick 0x1CAF58) and the Rewards rooms; buyAttribs. The layout
// (UI/character-select.json '139buy_popup', tools/export_character_select.py) plays its own timeline: frame 1 the box grows
// from 20 % (five shapes, 25 frames) and the veil fades in (8 frames), frame 25 the texts fade in (5 frames), frame 30 the
// timeline stops and the menu takes over: the focused item's frame (Yes 35, No 40: its text white, the Cross icon beside it).
// The code fills the widgets (onWidgetCreate 0x1CAC98): 'item type', 'item name', 'question', 'cost_answer', 'you have_answer'
// (0x198AF0 money), and hides 'group credit' (or 'group cash' for a song credit). docs/career-events.md "Buy Attributes".
const SY = 448 / 480;
export const BUY_POPUP_NAMES = Object.freeze({
  type: '0bf76125', name: '0bf7a975', question: '0bcaa35e', cost: '05d17882', have: '0bdab522', credits: '06fe3bfe',
  groupCredit: '0b7a6344', groupCash: '0c2b4a58', veil: '0885e124', yes: '00007fc3', no: '0000074f', box: '00376566' });

// Yes / No focus frames (the labels named after the items) and the last frame of the intro (the stop at 30).
export function buyPopupModel(screen) {
  const N = BUY_POPUP_NAMES, frames = [N.yes, N.no].map((n) => screen.labels.find((l) => l.name === n)?.frame);
  if (frames.some((f) => f == null)) return null;
  const first = Math.min(...frames);
  return { frames, intro: Math.max(0, ...screen.events.filter((ev) => ev.frame < first).map((ev) => ev.frame)) };
}

// The live events at `frame` (frames since the popup opened), `index` 0 Yes / 1 No.
export function buyPopupEvents(screen, model, frame, index) {
  const out = [];
  for (const ev of screen.events) {
    if (ev.frame <= model.intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
    else if (frame >= model.intro && ev.frame === model.frames[index]) out.push({ ev, start: model.intro });
  }
  return out;
}

// Draw on the 640x448 UI canvas. lui: a LuiScreen of 139buy_popup (shapeScale: the box's grow; flagWrap: only 'item name' wraps).
export function drawBuyPopup(c, lui, { frame, index = 0, type = '', name = '', question = '', cost = '', have = '', credits = null }) {
  const N = BUY_POPUP_NAMES, model = lui.buyModel ?? (lui.buyModel = buyPopupModel(lui.screen));
  if (!model) return false;
  c.save(); c.scale(1, SY);
  lui.draw(c, buyPopupEvents(lui.screen, model, frame, index), frame, (e) => {
    switch (e.name) {
      case N.type: return { text: type };
      case N.name: return { text: name };
      case N.question: return { text: question };
      case N.cost: return { text: cost };
      case N.have: return { text: have };
      case N.credits: return { text: credits == null ? '' : String(credits) };
      case N.groupCredit: return credits == null ? { hidden: true } : null;
      case N.groupCash: return credits == null ? null : { hidden: true };
      // the veil's vertices and element both animate to A 175; the GS draws it once at 175 >> 1 (PS2 fits 0.681 / 0.678)
      case N.veil: return { alpha: 255 };
      // the box's shapes: the same (vertex A and element A animate together to 150 / 200; the GS applies the vertex A once: PS2
      // fit inside the box, docs/career-events.md)
      default: return e.kind === 'shape' ? { alpha: 255 } : null;
    }
  });
  c.restore();
  return true;
}
