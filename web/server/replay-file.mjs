// A portable run replay (docs/online-records.md "The replay file"): what web/replay.js keeps for a run (the recorded pad stream and
// the start state main.js snapshot() takes) plus what another machine needs to set the same run up: the event, the human rider
// (character, skin base, outfit, attribute bytes, uber rows), the computer riders' lineup and the build identity. Shared by the
// page (upload, Watch Replay) and web/server/records.mjs (tier-0 checks); it lives in web/server/ because the server is deployed
// on its own and imports only from web/server/ and node:. Uploaded deflate-raw compressed.
//
//   bytes: 'SSXR', u8 version (1), u32 meta length, meta (UTF-8 JSON), u32 pad length, pad stream (web/replay.js createRecording)
//   meta:  {v, event "<mode>:<COURSE>", mode, course, round, name, character, rider {id, base, outfit, attributes, uber}, claim
//          {ticks} | {score}, ticks (recorded ticks), finishTick, events [{tick, kind, value}], snapshot (main.js), lineup, core, build,
//          terrain, giveUp}
const MAGIC = [0x53, 0x53, 0x58, 0x52], VERSION = 1;
export const REPLAY_FILE_VERSION = VERSION;

export function encodeReplayFile({ meta, pad }) {
  const m = new TextEncoder().encode(JSON.stringify({ ...meta, v: VERSION })), p = pad instanceof Uint8Array ? pad : new Uint8Array(pad ?? 0);
  const out = new Uint8Array(4 + 1 + 4 + m.length + 4 + p.length), dv = new DataView(out.buffer);
  out.set(MAGIC, 0); out[4] = VERSION; dv.setUint32(5, m.length, true); out.set(m, 9); dv.setUint32(9 + m.length, p.length, true); out.set(p, 13 + m.length);
  return out;
}

export function decodeReplayFile(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (b.length < 13 || MAGIC.some((x, i) => b[i] !== x) || b[4] !== VERSION) throw new Error('not a replay file');
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength), ml = dv.getUint32(5, true);
  if (9 + ml + 4 > b.length) throw new Error('replay file truncated');
  const meta = JSON.parse(new TextDecoder().decode(b.subarray(9, 9 + ml)));
  const pl = dv.getUint32(9 + ml, true);
  if (13 + ml + pl !== b.length) throw new Error('replay file length');
  if (!meta || typeof meta !== 'object') throw new Error('replay meta');
  return { meta, pad: b.slice(13 + ml) };
}

// Walk a pad stream (web/replay.js record layout: varint tick delta, 3-byte channel mask, 3-byte float mask, a byte per changed
// channel or 4 for a float one). null when it is malformed; else its record count and the tick of its last record.
export function padStreamInfo(pad) {
  let pos = 0, tick = 0, records = 0;
  while (pos < pad.length) {
    let dt = 0, sh = 0, b;
    do { if (pos >= pad.length || sh > 28) return null; b = pad[pos++]; dt += (b & 0x7F) * 2 ** sh; sh += 7; } while (b & 0x80);
    if (pos + 6 > pad.length) return null;
    const mask = pad[pos] | (pad[pos + 1] << 8) | (pad[pos + 2] << 16), floats = pad[pos + 3] | (pad[pos + 4] << 8) | (pad[pos + 5] << 16); pos += 6;
    if (!mask || (floats & ~mask)) return null;
    for (let i = 0; i < 24; i++) if (mask & (1 << i)) pos += floats & (1 << i) ? 4 : 1;
    if (pos > pad.length) return null;
    tick += dt; records++;
  }
  return { records, lastTick: records ? tick : -1 };
}
