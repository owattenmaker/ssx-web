// SSX 3 (PS2) audio decoders: EA SCxl streams, BNKl v5 banks, EA-XA, PS-ADPCM (VAG),
// signed 8-bit PCM and EA MicroTalk (UTK). Pure JS, no DOM: runs in node, workers and
// AudioWorklets. Everything decodes to Float32Array PCM in [-1, 1).
//
// Ground truth (see docs/audio-formats.md):
//   PT tag reader            EE 0x3C7388, stream header 0x3BB820, bank patch 0x3BAAC0
//   EA-XA frame decoder      SNDDRV.IRX decxa16c (0x9BA0), coefficient table xafp (0xACC0)
//   MicroTalk decoder        EE 0x3CD6F0 (init/header), 0x3CD878 (frame), 0x3CD2B0 (excitation),
//                            0x3CD590 (rc->lpc), 0x3CE410 (synthesis), 0x3CDE68 (EA PCM patches),
//                            tables at EE 0x44E8E8 (rc), 0x44E9E8 (codebooks), 0x44EBE8 (commands)

// EE scalar FPU arithmetic for the MicroTalk decoder (same semantics as web/ee-scalar-float.js: MUL.S chop, ADD.S /
// SUB.S one guard bit then chop, ADDA/MADDA one chop of the exact sum), specialised for speed: the decoder runs ~10k
// of these per 432-sample frame. Checked sample for sample against the original decoder (web/test-microtalk.mjs).
const FB = new Float32Array(2), UB = new Uint32Array(FB.buffer);
const TWO_M24 = 5.9604644775390625e-8, MIN_NORMAL = 1.1754943508222875e-38;
// The float32 next to r toward zero (r a nonzero float32).
function towardZero(r) {
  if (r > MIN_NORMAL || r < -MIN_NORMAL) return r === Infinity ? 3.4028234663852886e38 : r === -Infinity ? -3.4028234663852886e38 : Math.fround(r - r * TWO_M24);
  FB[0] = r; UB[0] -= 1; return FB[0];
}
function eeMul(a, b) { // the double product of two float32 values is exact
  const p = a * b, r = Math.fround(p);
  return r === p || r === 0 ? r : (r > 0 ? r > p : r < p) ? towardZero(r) : r;
}
function accAdd(a, b) { // one chop of the exact sum (TwoSum error term)
  const s = a + b, r = Math.fround(s);
  if (r !== s) return r !== 0 && (r > 0 ? r > s : r < s) ? towardZero(r) : r;
  const bb = s - a, err = (a - (s - bb)) + (b - bb);
  return err !== 0 && r !== 0 && (err < 0) === (r > 0) ? towardZero(r) : r;
}
function guardAddSub(a, b, subtract) {
  FB[0] = a; FB[1] = b; let x = UB[0], y = UB[1];
  const d = ((x >>> 23) & 255) - ((y >>> 23) & 255);
  if (d >= 25) y &= 0x80000000; else if (d <= -25) x &= 0x80000000;
  else if (d > 0) y &= (0xffffffff << (d - 1)); else if (d < 0) x &= (0xffffffff << (-d - 1));
  UB[0] = x; UB[1] = y;
  const e = subtract ? FB[0] - FB[1] : FB[0] + FB[1], r = Math.fround(e); // exact in a double after the mask
  return r === e || r === 0 ? r : (r > 0 ? r > e : r < e) ? towardZero(r) : r;
}
const eeAdd = (a, b) => guardAddSub(a, b, false), eeSub = (a, b) => guardAddSub(a, b, true);

export const CODEC = Object.freeze({
  PCM16BE: 0x07, PCM16LE: 0x08, PCM8: 0x09, EAXA: 0x0a, VAG: 0x05, MICROTALK: 0x04,
});
export const CODEC_NAMES = { 4: 'microtalk', 5: 'ps-adpcm', 7: 'pcm16be', 8: 'pcm16le', 9: 'pcm8', 10: 'ea-xa' };

// Defaults applied by the EE when a tag is absent.
//   SCHl stream header (0x3BB820): 22050 Hz, 1 channel, codec2 5.
//   BNKl patch (0x3BAAC0):         22050 Hz, 1 channel, codec2 5, loop start/end -1.
export const STREAM_DEFAULTS = Object.freeze({ sampleRate: 22050, channels: 1, codec: 5 });
export const BANK_DEFAULTS = Object.freeze({ sampleRate: 22050, channels: 1, codec: 5 });

function fourcc(u8, p) {
  return String.fromCharCode(u8[p], u8[p + 1], u8[p + 2], u8[p + 3]);
}
function u16le(u8, p) { return u8[p] | (u8[p + 1] << 8); }
function u32le(u8, p) { return (u8[p] | (u8[p + 1] << 8) | (u8[p + 2] << 16) | (u8[p + 3] << 24)) >>> 0; }
function s16be(u8, p) { const v = (u8[p] << 8) | u8[p + 1]; return v & 0x8000 ? v - 0x10000 : v; }

// ---------------------------------------------------------------------------------------------
// PT header ("PT", platform u8, 0) : tag stream as read by EE 0x3C7388.
//   0xFC            padding, skipped
//   0xFF            end of header
//   0xFD / 0xFE     sub-header markers (0xFD: sample part follows, 0xFE: next patch)
//   tag, len, data  value = big-endian integer of len bytes (len 0xFF -> 4-byte length follows)
// Returns { platform, patches: [{ params:{tag:value}, sample:{tag:value} }], end }.
// Tags < 0x80 are patch parameters, 0x80+ are sample tags (0x80 version, 0x82 channels,
// 0x84 rate, 0x85 samples, 0x86/0x87 loop start/end, 0x88/0x89.. per-channel data offset,
// 0xA0 codec2, ...). Values longer than 4 bytes are returned as Uint8Array.
export function parsePT(u8, pos) {
  if (u8[pos] !== 0x50 || u8[pos + 1] !== 0x54) throw new Error(`PT header expected at ${pos}`);
  const platform = u8[pos + 2];
  let p = pos + 4;
  const patches = [];
  let cur = { params: {}, sample: {} };
  let inSample = false;
  for (;;) {
    if (p >= u8.length) throw new Error('PT header runs past the buffer');
    const tag = u8[p++];
    if (tag === 0xfc) continue;
    if (tag === 0xff) break;
    if (tag === 0xfd) { inSample = true; continue; }
    if (tag === 0xfe) { patches.push(cur); cur = { params: {}, sample: {} }; inSample = false; continue; }
    let len = u8[p++];
    if (len === 0xff) { len = ((u8[p] << 24) | (u8[p + 1] << 16) | (u8[p + 2] << 8) | u8[p + 3]) >>> 0; p += 4; }
    let value;
    if (len <= 4) {
      value = 0;
      for (let i = 0; i < len; i++) value = value * 256 + u8[p + i];
    } else {
      value = u8.subarray(p, p + len);
    }
    p += len;
    (inSample || tag >= 0x80 ? cur.sample : cur.params)[tag] = value;
  }
  patches.push(cur);
  return { platform, patches, end: p };
}

function sampleInfo(sample, defaults) {
  const loopStart = sample[0x86];
  const loopEnd = sample[0x87];
  const offsets = [];
  for (let t = 0x88; t <= 0x8b; t++) if (sample[t] !== undefined && t !== 0x8a) offsets.push(sample[t]);
  return {
    version: sample[0x80] ?? 0,
    channels: sample[0x82] ?? defaults.channels,
    sampleRate: sample[0x84] ?? defaults.sampleRate,
    sampleCount: sample[0x85] ?? 0,
    loopStart: loopStart === undefined ? -1 : loopStart,
    loopEnd: loopEnd === undefined ? -1 : loopEnd,
    codec: sample[0xa0] ?? defaults.codec,
    dataOffsets: offsets,
  };
}

// ---------------------------------------------------------------------------------------------
// EA-XA (EA ADPCM, "xa" in SNDDRV.IRX). 15-byte frame = header byte + 14 bytes = 28 samples.
// header: high nibble = coefficient pair index (0..3), low nibble = shift - 8.
// Exact integer math of IRX decxa16c (0x9BA0): no rounding, no clamping of the history.
const XA_C1 = [0, 240, 460, 392];
const XA_C2 = [0, 0, -208, -220];

// Decodes `count` samples starting at frame boundary `pos`. state = {h1, h2} (int).
// Writes float samples to out[outPos + i*stride]. Returns the new byte position.
export function decodeEAXA(u8, pos, count, state, out, outPos = 0, stride = 1) {
  let h1 = state.h1 | 0, h2 = state.h2 | 0;
  let o = outPos;
  let left = count;
  while (left > 0) {
    const hdr = u8[pos];
    const c1 = XA_C1[(hdr >> 4) & 3], c2 = XA_C2[(hdr >> 4) & 3];
    const shift = (hdr & 0x0f) + 8;
    const n = left < 28 ? left : 28;
    for (let i = 0; i < n; i++) {
      const byte = u8[pos + 1 + (i >> 1)];
      const nib = (i & 1) ? byte & 0x0f : byte >> 4;
      const s = (((nib << 28) >> shift) + h1 * c1 + h2 * c2) >> 8;
      h2 = h1; h1 = s;
      out[o] = (s > 32767 ? 32767 : s < -32768 ? -32768 : s) / 32768;
      o += stride;
    }
    left -= n;
    pos += 15;
  }
  state.h1 = h1; state.h2 = h2;
  return pos;
}

// ---------------------------------------------------------------------------------------------
// PS-ADPCM (SPU "VAG"). 16-byte frame: [filter<<4 | shift][flags][14 bytes] = 28 samples,
// low nibble first. Standard SPU2 decode: s = (nib<<12 >> shift) + ((h1*f0 + h2*f1 + 32) >> 6),
// clamped to 16 bits. flags bit0 = loop end, bit1 = repeat, bit2 = loop start.
const VAG_F0 = [0, 60, 115, 98, 122];
const VAG_F1 = [0, 0, -52, -55, -60];
export function decodePSADPCM(u8, pos, count, state, out, outPos = 0, stride = 1) {
  let h1 = state.h1 | 0, h2 = state.h2 | 0;
  let o = outPos, left = count;
  while (left > 0) {
    const hdr = u8[pos];
    let filter = hdr >> 4; if (filter > 4) filter = 0;
    const shift = hdr & 0x0f;
    const f0 = VAG_F0[filter], f1 = VAG_F1[filter];
    const n = left < 28 ? left : 28;
    for (let i = 0; i < n; i++) {
      const byte = u8[pos + 2 + (i >> 1)];
      const nib = (i & 1) ? byte >> 4 : byte & 0x0f;
      let s = ((nib << 28) >> 16) >> shift;
      s += (h1 * f0 + h2 * f1 + 32) >> 6;
      if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
      h2 = h1; h1 = s;
      out[o] = s / 32768;
      o += stride;
    }
    left -= n;
    pos += 16;
  }
  state.h1 = h1; state.h2 = h2;
  return pos;
}

export function decodePCM8(u8, pos, count, out, outPos = 0, stride = 1) {
  for (let i = 0; i < count; i++) out[outPos + i * stride] = ((u8[pos + i] << 24) >> 24) / 128;
  return pos + count;
}

// ---------------------------------------------------------------------------------------------
// EA MicroTalk (UTK), EA variant with PCM patches (EE 0x3CDDE8 / 0x3CDE68, ctx+0xD64 = 1).
// Stream layout: [flag byte] header bits, frame bits, (patch if flag == 0xEE), [flag byte],
// frame bits, ... Each frame decodes 432 samples; after every frame the bit reader restarts on
// the next byte. A patch is BE16 offset, BE16 count, count BE16 samples written over the frame.
const UTK_RC = new Float32Array([
  0, -0.99677598476409912109375, -0.99032700061798095703125, -0.983879029750823974609375,
  -0.977430999279022216796875, -0.970982015132904052734375, -0.964533984661102294921875,
  -0.958085000514984130859375, -0.9516370296478271484375, -0.930754005908966064453125,
  -0.904959976673126220703125, -0.879167020320892333984375, -0.853372991085052490234375,
  -0.827579021453857421875, -0.801786005496978759765625, -0.775991976261138916015625,
  -0.75019800662994384765625, -0.724404990673065185546875, -0.6986110210418701171875,
  -0.6706349849700927734375, -0.61904799938201904296875, -0.567460000514984130859375,
  -0.515873014926910400390625, -0.4642859995365142822265625, -0.4126980006694793701171875,
  -0.361110985279083251953125, -0.309523999691009521484375, -0.257937014102935791015625,
  -0.20634900033473968505859375, -0.1547619998455047607421875, -0.10317499935626983642578125,
  -0.05158700048923492431640625,
  0, 0.05158700048923492431640625, 0.10317499935626983642578125, 0.1547619998455047607421875,
  0.20634900033473968505859375, 0.257937014102935791015625, 0.309523999691009521484375,
  0.361110985279083251953125, 0.4126980006694793701171875, 0.4642859995365142822265625,
  0.515873014926910400390625, 0.567460000514984130859375, 0.61904799938201904296875,
  0.6706349849700927734375, 0.6986110210418701171875, 0.724404990673065185546875,
  0.75019800662994384765625, 0.775991976261138916015625, 0.801786005496978759765625,
  0.827579021453857421875, 0.853372991085052490234375, 0.879167020320892333984375,
  0.904959976673126220703125, 0.930754005908966064453125, 0.9516370296478271484375,
  0.958085000514984130859375, 0.964533984661102294921875, 0.970982015132904052734375,
  0.977430999279022216796875, 0.983879029750823974609375, 0.99032700061798095703125,
  0.99677598476409912109375,
]);
// Codebooks (EE 0x44E9E8): 256-entry Huffman lookup for the normal and large-pulse models.
const UTK_CB = (() => {
  const a = '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,17,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,21,'
    + '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,18,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,25,'
    + '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,17,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,22,'
    + '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,18,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,0,'
    + '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,17,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,21,'
    + '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,18,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,26,'
    + '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,17,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,22,'
    + '4,6,5,9,4,6,5,13,4,6,5,10,4,6,5,18,4,6,5,9,4,6,5,14,4,6,5,10,4,6,5,2';
  const b = '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,23,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,27,'
    + '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,24,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,1,'
    + '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,23,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,28,'
    + '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,24,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,3,'
    + '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,23,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,27,'
    + '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,24,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,1,'
    + '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,23,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,28,'
    + '4,11,7,15,4,12,8,19,4,11,7,16,4,12,8,24,4,11,7,15,4,12,8,20,4,11,7,16,4,12,8,3';
  return [Uint8Array.from(a.split(',').map(Number)), Uint8Array.from(b.split(',').map(Number))];
})();
// Commands (EE 0x44EBE8): {next model, code size, pulse value}.
const UTK_CMD_NEXT = [1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1];
const UTK_CMD_SIZE = [8, 7, 8, 7, 2, 2, 2, 3, 3, 4, 4, 3, 3, 5, 5, 4, 4, 6, 6, 5, 5, 7, 7, 6, 6, 8, 8, 7, 7];
const UTK_CMD_PULSE = [0, 0, 0, 0, 0, -1, 1, -1, 1, -2, 2, -2, 2, -3, 3, -3, 3, -4, 4, -4, 4, -5, 5, -5, 5, -6, 6, -6, 6];

// EE constants (0x3CD6F0 0x3A83126F / 0x3F851EB8, 0x3CD878 0x3D888889, 0x3CD518 0x3F18EE49 / 0xBDEAAEFB / 0x3C93B945).
const f32bits = (b) => new Float32Array(new Uint32Array([b]).buffer)[0];
const F32_0_001 = f32bits(0x3a83126f), F32_1_04 = f32bits(0x3f851eb8), F32_1_15 = f32bits(0x3d888889);
const INTERP = [f32bits(0x3f18ee49), f32bits(0xbdeaaefb), f32bits(0x3c93b945)];

class UtkDecoder {
  // plain: ctx+0xD64 = 0 (0x3CE0D8(ctx, 0)), no flag bytes / PCM patches / byte re-alignment: the SCxl music stream voice
  // when the stream's tag 0x80 is below 3 (0x3C97A8 / 0x3C9844). Default: the EA wrapper of the SND bank voices.
  constructor(u8, pos, forceReducedBw = false, plain = false) {
    this.u8 = u8;
    this.forceReducedBw = forceReducedBw;
    this.plain = plain;
    this.fixedGains = new Float32Array(64);
    this.rc = new Float32Array(12);
    this.synth = new Float32Array(12);
    // adapt_cb (324) immediately followed by the decompressed frame (432), as in the EE ctx.
    this.mem = new Float32Array(324 + 432);
    this.exc = new Float32Array(5 + 108 + 5);
    this.open(pos);
    this.parseHeader();
  }

  // 0x3CDDE8 -> 0x3CD6F0 without the header (ctx+0xD5C = 0): restart the bit reader at pos (after the flag byte in the
  // EA mode) and keep every decoder state (gains, reflection coefficients, synthesis history, adaptive codebook).
  open(pos) {
    if (this.plain) { this.pcmPatch = false; this.initBits(pos); return; }
    this.pcmPatch = this.u8[pos] === 0xee;
    this.initBits(pos + 1);
  }

  initBits(p) { this.ptr = p + 1; this.value = this.u8[p] ?? 0; this.count = 8; }
  readBits(n) {
    const x = this.value & ((1 << n) - 1);
    this.value >>>= n;
    this.count -= n;
    if (this.count < 8) { this.value |= (this.u8[this.ptr++] ?? 0) << this.count; this.count += 8; }
    return x;
  }
  peek8() { return this.value & 0xff; }

  parseHeader() {
    this.reducedBw = this.readBits(1) | (this.forceReducedBw ? 1 : 0);
    this.threshold = 32 - this.readBits(4);
    // 0x3CD6F0 (EE FPU: MUL.S chop, ADD.S guard bit): gains[0] = (1 + v) * 8, base = v * 0.001f + 1.04f.
    this.fixedGains[0] = eeMul(1 + this.readBits(4), 8);
    const base = eeAdd(eeMul(this.readBits(6), F32_0_001), F32_1_04);
    for (let i = 1; i < 64; i++) this.fixedGains[i] = eeMul(this.fixedGains[i - 1], base);
  }

  excitation(multipulse, off, stride) {
    const out = this.exc;
    let i = 0;
    if (multipulse) {
      let model = 0;
      while (i < 108) {
        const cmd = UTK_CB[model][this.peek8()];
        model = UTK_CMD_NEXT[cmd];
        this.readBits(UTK_CMD_SIZE[cmd]);
        if (cmd > 3) {
          out[off + i] = UTK_CMD_PULSE[cmd]; i += stride;
        } else if (cmd > 1) {
          let n = 7 + this.readBits(6);
          if (i + n * stride > 108) n = ((108 - i) / stride) | 0;
          while (n-- > 0) { out[off + i] = 0; i += stride; }
        } else {
          let x = 7;
          while (this.readBits(1)) x++;
          if (!this.readBits(1)) x = -x;
          out[off + i] = x; i += stride;
        }
      }
    } else {
      while (i < 108) {
        const b = this.value & 3;
        if (b === 1) { out[off + i] = -2; this.readBits(2); }
        else if (b === 3) { out[off + i] = 2; this.readBits(2); }
        else { out[off + i] = 0; this.readBits(1); }
        i += stride;
      }
    }
  }

  // 0x3CD590 with the EE FPU operations in the original order.
  rcToLpc(lpc) {
    const rc = this.rc;
    const t2 = new Float32Array(12), t1 = new Float32Array(12);
    for (let i = 10; i >= 0; i--) t2[i + 1] = rc[i];
    t2[0] = 1;
    for (let i = 0; i < 12; i++) {
      let x = eeMul(-rc[11], t2[11]);
      for (let j = 10; j >= 0; j--) {
        x = eeSub(x, eeMul(rc[j], t2[j]));
        t2[j + 1] = eeAdd(eeMul(rc[j], x), t2[j]);
      }
      t2[0] = x; t1[i] = x;
      for (let j = 0; j < i; j++) x = eeSub(x, eeMul(lpc[j], t1[i - 1 - j]));
      lpc[i] = x;
    }
  }

  // 0x3CD690 -> 0x3CE410: ADDA.S / MADDA.S accumulate in the original (scheduled) order, each step rounded once
  // toward zero: for output j >= 1 lpc[1..j-1] against the older outputs of this run, lpc[j..11] against h[0..],
  // and lpc[0] against the newest output last.
  synthesize(offset, blocks) {
    const lpc = new Float32Array(12);
    this.rcToLpc(lpc);
    const h = this.synth, m = this.mem;
    let p = 324 + offset;
    for (let b = 0; b < blocks; b++) {
      for (let j = 0; j < 12; j++) {
        let x = m[p];
        for (let k = 1; k < j; k++) x = accAdd(x, eeMul(lpc[k], h[k - j + 12]));
        for (let k = j; k < 12; k++) x = accAdd(x, eeMul(lpc[k], h[k - j]));
        if (j > 0) x = accAdd(x, eeMul(lpc[0], h[12 - j]));
        h[11 - j] = x;
        m[p++] = x;
      }
    }
  }

  decodeFrame() {
    const delta = new Float32Array(12);
    let idx = this.readBits(6);
    delta[0] = eeMul(eeSub(UTK_RC[idx], this.rc[0]), 0.25);
    const multipulse = idx < this.threshold;
    for (let i = 1; i < 4; i++) { idx = this.readBits(6); delta[i] = eeMul(eeSub(UTK_RC[idx], this.rc[i]), 0.25); }
    for (let i = 4; i < 12; i++) { idx = this.readBits(5); delta[i] = eeMul(eeSub(UTK_RC[idx + 16], this.rc[i]), 0.25); }
    const m = this.mem, e = this.exc;
    for (let sf = 0; sf < 4; sf++) {
      const lag = this.readBits(8);
      const pitchGain = eeMul(this.readBits(4), F32_1_15); // cvt.s.w x 0x3D888889
      let fixedGain = this.fixedGains[this.readBits(6)];
      if (!this.reducedBw) {
        this.excitation(multipulse, 5, 1);
      } else {
        const align = this.readBits(1);
        const zero = this.readBits(1);
        this.excitation(multipulse, 5 + align, 2);
        if (zero) {
          for (let k = 1 - align; k < 108; k += 2) e[5 + k] = 0;
        } else {
          for (let k = 0; k < 5; k++) { e[k] = 0; e[5 + 108 + k] = 0; }
          for (let k = 1 - align; k < 108; k += 2) {
            const q = 5 + k;
            // 0x3CD518
            e[q] = eeAdd(eeAdd(eeMul(eeAdd(e[q - 1], e[q + 1]), INTERP[0]), eeMul(eeAdd(e[q - 3], e[q + 3]), INTERP[1])),
              eeMul(eeAdd(e[q - 5], e[q + 5]), INTERP[2]));
          }
          fixedGain = eeMul(fixedGain, 0.5);
        }
      }
      const src = sf * 108 + 216 - lag;
      const dst = 324 + sf * 108;
      // Lags above 216 in the first subframe read up to 39 floats before adapt_cb. In the EE context (init 0x3CD6F0)
      // those are the fields laid out just before it: synth history ctx+0x144 (-12..-1), rc ctx+0x114 (-24..-13)
      // and the tail of fixed_gains ctx+0x14 (-39..-25); reading them as the EE does keeps every stream finite.
      for (let k = 0; k < 108; k++) {
        const i = src + k;
        const past = i >= 0 ? m[i] : i >= -12 ? this.synth[12 + i] : i >= -24 ? this.rc[24 + i] : this.fixedGains[88 + i];
        m[dst + k] = eeAdd(eeMul(pitchGain, past), eeMul(fixedGain, e[5 + k]));
      }
    }
    m.copyWithin(0, 324 + 108, 324 + 432);
    for (let i = 0; i < 12; i++) this.rc[i] = eeAdd(this.rc[i], delta[i]);
    this.synthesize(0, 1);
    for (let i = 0; i < 12; i++) this.rc[i] = eeAdd(this.rc[i], delta[i]);
    this.synthesize(12, 1);
    for (let i = 0; i < 12; i++) this.rc[i] = eeAdd(this.rc[i], delta[i]);
    this.synthesize(24, 1);
    for (let i = 0; i < 12; i++) this.rc[i] = eeAdd(this.rc[i], delta[i]);
    this.synthesize(36, 33);
    // Plain mode (ctx+0xD64 = 0, SCxl music streams): the bitstream simply continues into the next frame (0x3CDF34).
    if (this.plain) return m.subarray(324, 324 + 432);
    // EA PCM patch + byte re-alignment (0x3CDF40..0x3CE028).
    let pos = this.ptr - 1;
    if (this.pcmPatch) {
      const off = s16be(this.u8, pos), cnt = s16be(this.u8, pos + 2);
      // 0x3CDE68 writes the patch without bounds: a patch outside the 432-sample frame (only reached by the few intros
      // whose header does not describe their coding, e.g. MineShaft channel 1) overwrites the context fields after the
      // frame (ctx+0xD44.. remaining count, output pointer), and the voice's output is lost from this frame on. The
      // decoder reports that as `lost` and the caller writes silence, which is what the original leaves in the buffer.
      if (cnt > 0 && (off < 0 || off + cnt > 432)) this.lost = true;
      else for (let i = 0; i < cnt; i++) m[324 + off + i] = s16be(this.u8, pos + 4 + 2 * i);
      pos += 4 + 2 * cnt;
    }
    this.pcmPatch = this.u8[pos] === 0xee;
    this.initBits(pos + 1);
    return m.subarray(324, 324 + 432);
  }
}

// QA: samples the MicroTalk decoder had to replace (must stay 0; test-audio-decode.mjs).
export const decodeStats = { nonFinite: 0 };

export function decodeMicroTalk(u8, pos, count, out, outPos = 0, stride = 1, forceReducedBw = false) {
  const dec = new UtkDecoder(u8, pos, forceReducedBw);
  utkRun(dec, count, out, outPos, stride);
  return dec.ptr - 2; // position of the next frame's flag byte (end of this stream's data)
}

// count samples from dec's next frame on (a partly used last frame is dropped: the next open restarts at a frame, ctx+0xD44 = 0).
function utkRun(dec, count, out, outPos, stride) {
  let done = 0;
  while (done < count) {
    const frame = dec.decodeFrame();
    const n = Math.min(432, count - done);
    if (dec.lost) { for (let i = done; i < count; i++) out[outPos + i * stride] = 0; break; }
    for (let i = 0; i < n; i++) {
      const s = frame[i];
      // A non-finite frame sample must never reach Web Audio: WebKit propagates one NaN through the whole mix
      // and silences it until reload (Chrome zeroes it). docs/audio-formats.md MicroTalk notes.
      if (!Number.isFinite(s)) decodeStats.nonFinite++;
      out[outPos + (done + i) * stride] = Number.isFinite(s) ? (s > 32767 ? 32767 : s < -32768 ? -32768 : s) / 32768 : 0;
    }
    done += n;
  }
}

// ---------------------------------------------------------------------------------------------
// EA SCxl stream: SCHl (PT header) SCCl (block count) SCDl* [SCLl] SCEl. Little-endian sizes.
// SCDl: u32 block size, u32 samples in block, u32 x channels offsets (relative to the end of the
// offset table), then per channel: 4 bytes (two s16 values the decoder does not need; the ADPCM
// history simply continues), then 15-byte EA-XA frames.
// MicroTalk (codec2 4, the IOP-mixed stream voice 0x3C9960 / read 0x3C96F0): per channel one decoder for the whole SCHl.
// Each block's channel data starts with one byte the voice skips (0x3C98CC: 01 on the first block, 00 after); the first
// block of the SCHl opens the decoder with the header (0x3B6EA8 leaves bit 31 of the block count clear), later blocks
// only restart the bit reader (bit 31 set -> 0x3CE0B8 with ctx+0xD5C = 0) and keep the LPC / synthesis / adaptive codebook
// state. Tag 0x80 below 3 (every stream on the disc: 2) selects plain MicroTalk (no flag bytes, no PCM patches).
export function parseStream(u8, offset = 0) {
  if (fourcc(u8, offset) !== 'SCHl') throw new Error(`SCHl expected at ${offset}`);
  let p = offset;
  const hsize = u32le(u8, p + 4);
  const pt = parsePT(u8, p + 8);
  const info = sampleInfo(pt.patches[0].sample, STREAM_DEFAULTS);
  info.params = pt.patches[0].params;
  info.platform = pt.platform;
  p += hsize;
  const blocks = [];
  let blockCount = 0;
  let loopBlock = null;
  for (;;) {
    if (p + 8 > u8.length) throw new Error('stream runs past the buffer');
    const id = fourcc(u8, p);
    const size = u32le(u8, p + 4);
    if (id === 'SCCl') blockCount = u32le(u8, p + 8);
    else if (id === 'SCDl') blocks.push({ offset: p, size, samples: u32le(u8, p + 8) });
    else if (id === 'SCLl') loopBlock = u32le(u8, p + 8);
    else if (id === 'SCEl') { p += size; break; }
    else throw new Error(`unexpected block ${id} at ${p}`);
    if (size < 8) throw new Error(`bad block size at ${p}`);
    p += size;
  }
  info.blocks = blocks;
  info.blockCount = blockCount;
  info.loopBlock = loopBlock;
  info.end = p;
  info.decodedSamples = blocks.reduce((a, b) => a + b.samples, 0);
  return info;
}

// Decode a whole SCxl stream -> { sampleRate, channels, length, data: Float32Array[] }.
export function decodeStream(u8, offset = 0, info = parseStream(u8, offset)) {
  const ch = info.channels;
  const total = info.sampleCount || info.decodedSamples;
  const data = Array.from({ length: ch }, () => new Float32Array(total));
  const states = Array.from({ length: ch }, () => ({ h1: 0, h2: 0 }));
  const utk = [];
  let at = 0;
  for (const b of info.blocks) {
    const tab = b.offset + 12;
    const base = tab + 4 * ch;
    const n = Math.min(b.samples, total - at);
    if (n <= 0) break;
    for (let c = 0; c < ch; c++) {
      const start = base + u32le(u8, tab + 4 * c);
      if (info.codec === CODEC.EAXA) {
        decodeEAXA(u8, start + 4, n, states[c], data[c], at);
      } else if (info.codec === CODEC.VAG) {
        decodePSADPCM(u8, start, n, states[c], data[c], at);
      } else if (info.codec === CODEC.PCM8) {
        decodePCM8(u8, start, n, data[c], at);
      } else if (info.codec === CODEC.MICROTALK) {
        if (!utk[c]) utk[c] = new UtkDecoder(u8, start + 1, false, info.version < 3);
        else utk[c].open(start + 1);
        utkRun(utk[c], n, data[c], at, 1);
      } else {
        throw new Error(`unsupported stream codec ${info.codec}`);
      }
    }
    at += n;
  }
  return { sampleRate: info.sampleRate, channels: ch, length: total, data };
}

// ---------------------------------------------------------------------------------------------
// BNKl v5 bank. Header: 'BNKl', u8 version (5), u8 ?, u16 entry count, u32 header size (start
// of the sample data), u32 SPU (VAG) data size, u32 main-RAM data size, then u32 x count
// relative offsets (from the table slot itself, 0 = empty) to PT headers. A PT header may hold
// several patches (separated by 0xFE); each patch's sample part gives codec, rate, channels and
// absolute data offsets (0x88 = channel 0, 0x89 = channel 1: planar).
export function parseBank(u8) {
  if (fourcc(u8, 0) !== 'BNKl') throw new Error('BNKl expected');
  const version = u8[4];
  const count = u16le(u8, 6);
  const bank = {
    version, count,
    headerSize: u32le(u8, 8), spuDataSize: u32le(u8, 12), ramDataSize: u32le(u8, 16),
    entries: [],
  };
  for (let i = 0; i < count; i++) {
    const slot = 0x14 + 4 * i;
    const rel = u32le(u8, slot);
    if (!rel) { bank.entries.push(null); continue; }
    const pt = parsePT(u8, slot + rel);
    bank.entries.push({
      index: i,
      patches: pt.patches.map((p) => ({ params: p.params, ...sampleInfo(p.sample, BANK_DEFAULTS) })),
    });
  }
  return bank;
}

// Decode one bank patch (entry.patches[k]) -> { sampleRate, channels, length, data, loopStart, loopEnd }.
export function decodeBankPatch(u8, patch) {
  const n = patch.sampleCount;
  const data = [];
  for (let c = 0; c < patch.channels; c++) {
    const out = new Float32Array(n);
    let pos = patch.dataOffsets[c];
    switch (patch.codec) {
      case CODEC.EAXA: decodeEAXA(u8, pos, n, { h1: 0, h2: 0 }, out); break;
      case CODEC.VAG: decodePSADPCM(u8, pos, n, { h1: 0, h2: 0 }, out); break;
      case CODEC.PCM8:
        // Main-RAM PCM8 samples carry a 16-byte header: 00 09 00 00, loop start, loop end, count.
        if (u8[pos] === 0 && u8[pos + 1] === CODEC.PCM8 && u32le(u8, pos + 12) === n) pos += 16;
        decodePCM8(u8, pos, n, out);
        break;
      case CODEC.MICROTALK: {
        // Looped MicroTalk is stored as two independent streams: an intro of
        // ceil(loopStart/432) frames of which loopStart samples are used, then the loop body
        // with its own flag byte + header at dataOffsets[0] + params 0x1A (channel 0) /
        // 0x26 (channel 1).
        // params come keyed by number from parseBank and by '0x1a' strings from the exported bank JSON
        // (tools/export_audio.py); both must find the body offset, else the intro stream runs into the body's flag
        // byte and header and the decoder diverges (non-finite samples).
        const param = (t) => patch.params[t] ?? patch.params['0x' + t.toString(16).padStart(2, '0')];
        const loopTag = c === 0 ? param(0x1a) : param(0x26);
        if (patch.loopStart >= 0 && loopTag !== undefined) {
          const intro = Math.min(patch.loopStart, n);
          const body = patch.dataOffsets[0] + loopTag;
          // The intro is decoded exactly as the SND voice does (0x3C9420 -> 0x3CDDE8 / 0x3CDE68): header as stored,
          // even for the few intros shorter than a frame whose parse does not end on the body (the original ignores
          // that; the body is addressed by its own offset). Verified sample for sample by web/test-microtalk.mjs.
          if (intro > 0) decodeMicroTalk(u8, pos, intro, out, 0);
          // The body stream holds samples [loopStart, loopEnd] (inclusive); anything after
          // loopEnd (Transport.bnk 16/17) is not stored and stays silent.
          const last = patch.loopEnd >= intro ? Math.min(n, patch.loopEnd + 1) : n;
          if (last > intro) decodeMicroTalk(u8, body, last - intro, out, intro);
        } else {
          decodeMicroTalk(u8, pos, n, out);
        }
        break;
      }
      default: throw new Error(`unsupported bank codec ${patch.codec}`);
    }
    data.push(out);
  }
  return {
    sampleRate: patch.sampleRate, channels: patch.channels, length: n, data,
    loopStart: patch.loopStart, loopEnd: patch.loopEnd,
  };
}

// ---------------------------------------------------------------------------------------------
// Helpers for the exported asset layout (tools/export_audio.py -> web/public/assets/AUDIO/).

// Speech line: index entry { offset, size } into the .dat binary -> decoded stream.
export function decodeSpeechLine(datBytes, line) {
  const u8 = line.size ? datBytes.subarray(line.offset, line.offset + line.size) : datBytes.subarray(line.offset);
  return decodeStream(u8, 0);
}

// Music segment: sample entry { kind:'stream', offset, size } in the song's .mus binary, or
// { kind:'bank', bankIndex, count } in the loop bank (count consecutive bank sounds).
export function decodeMusicSample(musBytes, loopBytes, sample) {
  if (sample.kind === 'stream') return decodeStream(musBytes.subarray(sample.offset, sample.offset + sample.size), 0);
  const bank = parseBank(loopBytes);
  const parts = [];
  for (let i = 0; i < sample.count; i++) {
    const e = bank.entries[sample.bankIndex + i];
    if (e) parts.push(decodeBankPatch(loopBytes, e.patches[0]));
  }
  return concatDecoded(parts);
}

export function concatDecoded(parts) {
  if (!parts.length) return { sampleRate: 22050, channels: 1, length: 0, data: [new Float32Array(0)] };
  const ch = Math.max(...parts.map((p) => p.channels));
  const len = parts.reduce((a, p) => a + p.length, 0);
  const data = Array.from({ length: ch }, () => new Float32Array(len));
  let at = 0;
  for (const p of parts) {
    for (let c = 0; c < ch; c++) data[c].set(p.data[Math.min(c, p.channels - 1)], at);
    at += p.length;
  }
  return { sampleRate: parts[0].sampleRate, channels: ch, length: len, data };
}

// Build a WebAudio AudioBuffer (ctx = AudioContext / OfflineAudioContext / BaseAudioContext).
// Multi-channel music (e.g. 6-channel stems) can be folded with `channelMap`: array of output
// channel lists, e.g. [[0,2,4],[1,3,5]] sums stems into stereo.
// Replace non-finite samples with 0 (in place); returns how many were replaced.
export function sanitizeSamples(data) {
  let bad = 0;
  for (let i = 0; i < data.length; i++) if (!Number.isFinite(data[i])) { data[i] = 0; bad++; }
  return bad;
}
export function toAudioBuffer(ctx, decoded, channelMap = null) {
  const outCh = channelMap ? channelMap.length : decoded.channels;
  const buf = ctx.createBuffer(outCh, Math.max(1, decoded.length), decoded.sampleRate);
  // Last line of defence: one NaN sample poisons the whole WebKit (Safari) mix until reload.
  for (const d of decoded.data) if (sanitizeSamples(d)) globalThis.console?.warn?.('Audio: non-finite samples replaced with silence');
  for (let c = 0; c < outCh; c++) {
    if (!channelMap) { buf.copyToChannel(decoded.data[c], c); continue; }
    const mix = new Float32Array(decoded.length);
    for (const src of channelMap[c]) {
      const d = decoded.data[src];
      for (let i = 0; i < mix.length; i++) mix[i] += d[i];
    }
    buf.copyToChannel(mix, c);
  }
  return buf;
}

// Minimal RIFF/WAVE (16-bit) writer for debugging and tests.
export function encodeWav(decoded) {
  const { channels: ch, length: n, sampleRate: rate } = decoded;
  const buf = new ArrayBuffer(44 + n * ch * 2);
  const v = new DataView(buf);
  const w = (p, s) => { for (let i = 0; i < 4; i++) v.setUint8(p + i, s.charCodeAt(i)); };
  w(0, 'RIFF'); v.setUint32(4, 36 + n * ch * 2, true); w(8, 'WAVE'); w(12, 'fmt ');
  v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, ch, true);
  v.setUint32(24, rate, true); v.setUint32(28, rate * ch * 2, true); v.setUint16(32, ch * 2, true);
  v.setUint16(34, 16, true); w(36, 'data'); v.setUint32(40, n * ch * 2, true);
  let p = 44;
  for (let i = 0; i < n; i++) {
    for (let c = 0; c < ch; c++) {
      const s = Math.max(-1, Math.min(1, decoded.data[c][i]));
      v.setInt16(p, Math.round(s * 32767), true); p += 2;
    }
  }
  return new Uint8Array(buf);
}

