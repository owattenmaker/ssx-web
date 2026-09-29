// Host device -> original PS2 channel mapping (pad-input.js) checks.
import { buildPad, axisByte, stickChannels, PAD_BUTTONS, createKeyboardContext } from './pad-input.js';
const fail = (m) => { console.error(m); process.exit(1); };
const none = () => false;
const pad = (codes) => buildPad((c) => codes.includes(c), null);
// Neutral: all zero.
if (pad([]).some((v) => v !== 0)) fail('neutral pad not zero');
// Keyboard extremes equal a fully deflected PS2 stick/button.
const one = stickChannels(0)[0];
if (one !== 1) fail('full stick response ' + one);
const cross = pad(['Space']);
if (cross[10] !== 1) fail('Space is not Cross ' + cross[10]);
const expect = { KeyC: 9, ShiftLeft: 11, KeyQ: 12, KeyE: 13, KeyZ: 14, KeyX: 15, Backspace: 0, KeyL: 4, KeyJ: 5, KeyI: 6, KeyK: 7, KeyY: 8, KeyV: 3 };
for (const [code, index] of Object.entries(expect)) { const v = pad([code]); if (v[index] !== 1 || v.filter((x) => x).length !== 1) fail(`${code} -> ${PAD_BUTTONS[index]} ${Array.from(v)}`); }
const stick = (codes, index) => { const v = pad(codes); if (v[index] !== 1 || v.filter((x) => x).length !== 1) fail(`${codes} stick ${index} ${Array.from(v)}`); };
stick(['KeyA'], 20); stick(['KeyD'], 21); stick(['KeyW'], 22); stick(['KeyS'], 23);
stick(['ArrowLeft'], 20); stick(['ArrowRight'], 21); stick(['ArrowUp'], 22); stick(['ArrowDown'], 23);
stick(['KeyF'], 16); stick(['KeyH'], 17); stick(['KeyT'], 18); stick(['KeyG'], 19);
// Original axial dead zone: bytes 79..176 are zero, so ~38% deflection does nothing.
for (const v of [0, 0.3, -0.37, 0.37]) { const b = axisByte(v); const [n, p] = stickChannels(b); if (n || p) fail(`dead zone ${v} byte ${b} -> ${n},${p}`); }
if (!(stickChannels(axisByte(0.6))[1] > 0)) fail('0.6 deflection should leave the dead zone');
// Gamepad: standard layout, +y down; Circle (b1) is Handplant, never brake.
const gp = { buttons: Array.from({ length: 17 }, (_, i) => ({ pressed: i === 1, value: i === 1 ? 1 : 0 })), axes: [0, 0, 0, -1] };
const g = buildPad(none, gp);
if (g[9] !== 1 || g[18] !== 1 || g.filter((x) => x).length !== 2) fail('gamepad Circle/right-stick-up ' + Array.from(g));
// Simple keyboard mode: movement keys are the stick on the ground and the D-pad in the air, except a
// direction held through the takeoff keeps steering until released.
{
  const kb = createKeyboardContext('Simple'); const at = (codes, air) => { kb.air = air; return buildPad((c) => codes.includes(c), null, kb); };
  let v = at(['ArrowLeft'], false); if (v[20] !== 1 || v[5] !== 0) fail('simple ground left should be stick');
  v = at(['ArrowLeft'], true); if (v[20] !== 1 || v[5] !== 0) fail('left held through takeoff should keep steering');
  v = at([], true); v = at(['ArrowLeft'], true); if (v[5] !== 1 || v[20] !== 0) fail('left pressed in air should be D-pad left');
  v = at(['ArrowLeft', 'KeyW'], true); if (v[6] !== 1 || v[5] !== 1) fail('W pressed in air should be D-pad up');
  v = at(['Space', 'KeyD'], false); if (v[10] !== 1 || v[21] !== 1) fail('simple ground keeps Cross and stick');
  v = at(['KeyL'], true); if (v[4] !== 1) fail('IJKL D-pad still works in simple mode');
  // 540 spinboost / D-pad spin + opposite stick: a spin pressed in the air, then the opposite direction while it is
  // held, is D-pad left + left stick right in the air and stays so through the landing (like a DualShock).
  const sb = createKeyboardContext('Simple'); const sp = (codes, air) => { sb.air = air; return buildPad((c) => codes.includes(c), null, sb); };
  const dualshock = (dpad, lx) => { const g = new Float32Array(24); if (dpad) g[dpad] = 1; if (lx) { const [n, p] = stickChannels(axisByte(lx)); g[20] = n; g[21] = p; } return Array.from(g); };
  const same = (a, b, m) => { if (Array.from(a).some((x, k) => x !== b[k])) fail(`${m}: ${Array.from(a)} vs ${b}`); };
  sp([], false); sp([], true);
  same(sp(['ArrowLeft'], true), dualshock(5, 0), 'air D-pad left spin');
  same(sp(['ArrowLeft', 'ArrowRight'], true), dualshock(5, 1), 'air opposite press is the stick (D-pad left + stick right)');
  same(sp(['ArrowLeft', 'ArrowRight'], false), dualshock(5, 1), 'landing keeps D-pad left + stick right');
  same(sp(['ArrowRight'], false), dualshock(0, 1), 'releasing the spin key leaves the stick');
  same(sp(['ArrowRight', 'ArrowLeft'], false), dualshock(0, -1), 'ground: the newest stick direction wins');
  // Prewind-buffered spin: a D-pad spin held through the landing with Cross stays PrewindSpin (D-pad).
  const pb = createKeyboardContext('Simple'); const pw = (codes, air) => { pb.air = air; return buildPad((c) => codes.includes(c), null, pb); };
  pw([], true); pw(['KeyA'], true); const land = pw(['KeyA', 'Space'], false);
  if (land[5] !== 1 || land[10] !== 1 || land[20] !== 0) fail('D-pad spin held through the landing must stay D-pad with Cross ' + Array.from(land));
  // Same-direction presses in the air stay D-pad (flip + spin chords); a steering key held from the ground is not an opposite spin.
  const st = createKeyboardContext('Simple'); const sa = (codes, air) => { st.air = air; return buildPad((c) => codes.includes(c), null, st); };
  sa(['ArrowRight'], false); same(sa(['ArrowRight', 'ArrowLeft'], true), dualshock(5, 1), 'held steering right + new left press = D-pad left, stick right');
  const classic = createKeyboardContext('Classic'); classic.air = true; const c = buildPad((k) => k === 'ArrowLeft', null, classic); if (c[20] !== 1 || c[5] !== 0) fail('classic mode keeps arrows on the stick in the air');
}
console.log('Pad input mapping matches the original PS2 channel layout and analog response.');
