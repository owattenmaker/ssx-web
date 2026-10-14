// The TAS pad of a replay file (docs/native.md "N3", visual parity on any course): the recorded pad stream (web/replay.js) back to
// the DualShock bytes tools/tas/pad-format.mjs writes, so native/tools/browser-shot.mjs can play the replay's run live on the page
// (a Single Event with the replay's course, rider and lineup) and ssx3_render_shot draw the same frame from the file.
// Every channel web/pad-input.js builds is a pad byte (towardZero(byte / 255); the sticks two channels a byte: stickChannels),
// so the conversion is lossless; a channel that is no pad byte (a QA float) is refused.
//   node native/tools/replay-tas.mjs RUN.ssxr OUT.tas
// Prints {course, rider, lineup, ticks} (browser-shot's --course / --rider / --lineup).
import fs from 'node:fs';
import zlib from 'node:zlib';
import { serialize } from '../../tools/tas/pad-format.mjs';
import { stickChannels } from '../../web/pad-input.js';
import { createRecording } from '../../web/replay.js';
import { decodeReplayFile } from '../../web/server/replay-file.mjs';

const [input, out] = process.argv.slice(2);
if (!input || !out) throw new Error('usage: replay-tas.mjs RUN.ssxr OUT.tas');
let bytes = new Uint8Array(fs.readFileSync(input));
// an uploaded file is deflate-raw compressed
if (String.fromCharCode(...bytes.subarray(0, 4)) !== 'SSXR') bytes = new Uint8Array(zlib.inflateRawSync(bytes));
const { meta, pad } = decodeReplayFile(bytes);
const rec = createRecording();
rec.importBytes(pad, meta.ticks, meta.events ?? []);
// the pressure byte of a channel value, and the stick byte of a channel pair
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
const towardZero = (x) => {
  const f = new Float32Array([x]);
  if (Math.abs(f[0]) > Math.abs(x)) new Uint32Array(f.buffer)[0] -= 1;
  return f[0];
};
const pressure = new Map();
for (let b = 0; b < 256; b++) pressure.set(towardZero(b * R255), b);
// several bytes give the same channels (the dead zone): the one nearest the centre stands for them
const stick = new Map();
const nearCentre = Array.from({ length: 256 }, (_, b) => b).sort((x, y) => Math.abs(x - 128) - Math.abs(y - 128) || x - y);
for (const b of nearCentre) {
  const [n, p] = stickChannels(b);
  const key = `${Math.fround(n)},${Math.fround(p)}`;
  if (!stick.has(key)) stick.set(key, b);
}
const frames = [];
const channels = new Float32Array(24);
const cursor = {};
for (let t = 0; t < meta.ticks; t++) {
  rec.pad(t, channels, cursor);
  const buttons = new Uint8Array(16);
  for (let i = 0; i < 16; i++) {
    if (i < 4) {
      buttons[i] = channels[i] ? 255 : 0;
      continue;
    }
    const b = pressure.get(channels[i]);
    if (b === undefined) throw new Error(`tick ${t}: channel ${i} is no pad byte (${channels[i]})`);
    buttons[i] = b;
  }
  // the right stick (16..19), then the left stick (20..23), x before y
  const bytesOf = (k) => {
    const b = stick.get(`${channels[16 + 2 * k]},${channels[17 + 2 * k]}`);
    if (b === undefined) throw new Error(`tick ${t}: stick channels ${16 + 2 * k} are no stick byte`);
    return b;
  };
  const [rx, ry, lx, ly] = [0, 1, 2, 3].map(bytesOf);
  frames.push({ buttons, sticks: [lx, ly, rx, ry] });
}
fs.writeFileSync(out, serialize(frames, [`from ${input}`]));
console.log(JSON.stringify({ course: meta.course, rider: meta.rider?.id, lineup: meta.lineup ?? null, ticks: meta.ticks }));
