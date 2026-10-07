// The TAS pad format (docs/tas.md "Pad format"): what a DualShock 2 sends, one line per change.
//
//   # ssx3-tas 1
//   # any comment line
//   <tick> <buttons> <lx> <ly> <rx> <ry>
//
// tick: the 0-based run tick (web/replay.js recording tick) from which the line holds, until the next line.
// buttons: 32 hex digits, two per button, left to right in web/pad-input.js PAD_BUTTONS order (Select, Start, L3, R3,
//   D-right, D-left, D-up, D-down, Triangle, Circle, Cross, Square, L1, R1, L2, R2): the pressure byte 00..ff
//   (Select..R3 are digital: 00 or ff).
// lx, ly, rx, ry: the analog stick bytes 0..255 (128 centre; 0 = left / up), as the pad reports them.
//
// toChannels() turns a line into the 24 channels web/pad-input.js builds from the same bytes (pressureChannel /
// stickChannels: the EE's round-toward-zero products), which are exactly the pad bytes web/replay.js records. So
// a TAS file converts losslessly into the port's replay pad stream, and the same bytes drive tools/ps2_capture.py.
import { stickChannels, towardZero } from '../../web/pad-input.js';

export const BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown',
  'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
export const HEADER = '# ssx3-tas 1';
const RECIPROCAL_255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];

// A pad frame: { buttons: Uint8Array(16) pressure bytes, sticks: [lx, ly, rx, ry] bytes }.
export function neutralFrame() {
  return { buttons: new Uint8Array(16), sticks: [128, 128, 128, 128] };
}

export function cloneFrame(f) {
  return { buttons: Uint8Array.from(f.buttons), sticks: f.sticks.slice() };
}

export function sameFrame(a, b) {
  for (let i = 0; i < 16; i++) {
    if (a.buttons[i] !== b.buttons[i]) return false;
  }
  for (let i = 0; i < 4; i++) {
    if (a.sticks[i] !== b.sticks[i]) return false;
  }
  return true;
}

// The 24 pad channels of a frame (web/pad-input.js buildClassicPad from the same device bytes).
export function toChannels(f, out = new Float32Array(24)) {
  for (let i = 0; i < 16; i++) {
    const b = f.buttons[i];
    if (i < 4) {
      out[i] = b >= 128 ? 1 : 0;
    } else {
      out[i] = towardZero(b * RECIPROCAL_255);
    }
  }
  // the right stick (16..19), then the left stick (20..23), x before y
  const order = [f.sticks[2], f.sticks[3], f.sticks[0], f.sticks[1]];
  for (let k = 0; k < 4; k++) {
    const [n, p] = stickChannels(order[k]);
    out[16 + 2 * k] = n;
    out[17 + 2 * k] = p;
  }
  return out;
}

function frameLine(tick, f) {
  let hex = '';
  for (let i = 0; i < 16; i++) hex += f.buttons[i].toString(16).padStart(2, '0');
  return `${tick} ${hex} ${f.sticks.join(' ')}`;
}

// frames: an array (one frame per tick). Lines only where the frame changes.
export function serialize(frames, comments = []) {
  const lines = [HEADER, ...comments.map((c) => `# ${c}`)];
  let last = null;
  frames.forEach((f, t) => {
    if (!last || !sameFrame(last, f)) {
      lines.push(frameLine(t, f));
      last = f;
    }
  });
  lines.push(`# end ${frames.length}`);
  return lines.join('\n') + '\n';
}

// -> { frames, comments }. The run length is the '# end N' line (else the last line's tick + 1).
export function parse(text) {
  const changes = [];
  const comments = [];
  let end = -1;
  for (const raw of text.split('\n')) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith('#')) {
      const m = /^# end (\d+)$/.exec(line);
      if (m) end = +m[1];
      else if (line !== HEADER) comments.push(line.slice(1).trim());
      continue;
    }
    const [tick, hex, ...sticks] = line.split(/\s+/);
    if (!/^[0-9a-f]{32}$/i.test(hex) || sticks.length !== 4) throw new Error(`bad pad line: ${line}`);
    const buttons = new Uint8Array(16);
    for (let i = 0; i < 16; i++) buttons[i] = parseInt(hex.slice(2 * i, 2 * i + 2), 16);
    const s = sticks.map(Number);
    if (s.some((v) => !Number.isInteger(v) || v < 0 || v > 255)) throw new Error(`bad stick byte: ${line}`);
    changes.push({ tick: +tick, frame: { buttons, sticks: s } });
  }
  if (end < 0) end = changes.length ? changes[changes.length - 1].tick + 1 : 0;
  const frames = [];
  let cur = neutralFrame();
  let k = 0;
  for (let t = 0; t < end; t++) {
    while (k < changes.length && changes[k].tick <= t) cur = changes[k++].frame;
    frames.push(cur);
  }
  return { frames, comments };
}

// Convenience for building frames: press(f, 'Cross') etc.
export function withButton(f, name, pressure = 255) {
  const g = cloneFrame(f);
  const i = BUTTONS.indexOf(name);
  if (i < 0) throw new Error(`unknown button ${name}`);
  g.buttons[i] = i < 4 ? (pressure ? 255 : 0) : pressure;
  return g;
}
