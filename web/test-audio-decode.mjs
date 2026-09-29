// Audio decoder checks: hand-worked PS-ADPCM / EA-XA reference frames, then real items from
// the exported assets (python3 tools/export_audio.py): a DJ speech line, character speech,
// bank sounds of every codec (MicroTalk, PS-ADPCM, PCM8, EA-XA), a music stream segment and a
// loop-bank segment, validated against their headers and the Pathfinder sample table.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import * as A from './audio-decode.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(here, 'public/assets/AUDIO');
let failures = 0, checks = 0;
function ok(cond, msg) { checks++; if (!cond) { failures++; console.error('FAIL', msg); } }
function eq(a, b, msg) { ok(a === b, `${msg}: got ${a}, want ${b}`); }

function sanity(d, label, { minRms = 0.005 } = {}) {
  let peak = 0, e = 0, bad = 0;
  for (const ch of d.data) {
    ok(ch.length === d.length, `${label}: channel length ${ch.length} != ${d.length}`);
    for (const v of ch) { if (!Number.isFinite(v)) bad++; else { peak = Math.max(peak, Math.abs(v)); e += v * v; } }
  }
  const rms = Math.sqrt(e / Math.max(1, d.length * d.channels));
  eq(bad, 0, `${label}: non-finite samples`);
  ok(peak <= 1, `${label}: peak ${peak} outside [-1, 1]`);
  ok(rms > minRms, `${label}: no energy (rms ${rms})`);
  return { peak, rms };
}

// ---------------------------------------------------------------------------------------------
// 1. PS-ADPCM, worked by hand. Frame header 0x24 = filter 2 (f0 115, f1 -52), shift 4.
//    Nibbles (low first) 1,7 | 15(-1),0 | 8(-8),8(-8) | 0 ... , history 0:
//    s0 = (1<<12)>>4 = 256                          + (0 + 32)>>6                 = 256
//    s1 = (7<<12)>>4 = 1792 + (256*115 + 32)>>6 = 29472>>6 = 460              -> 2252
//    s2 = -4096>>4 = -256 + (2252*115 - 256*52 + 32)>>6 = 245700>>6 = 3839   -> 3583
//    s3 = 0 + (3583*115 - 2252*52 + 32)>>6 = 294973>>6 = 4608                -> 4608
//    s4 = -32768>>4 = -2048 + (4608*115 - 3583*52 + 32)>>6 = 343636>>6 = 5369 -> 3321
//    s5 = -2048 + (3321*115 - 4608*52 + 32)>>6 = 142331>>6 = 2223            -> 175
//    s6 = 0 + (175*115 - 3321*52 + 32)>>6 = -152535>>6 = -2384 (floor)       -> -2384
{
  const frame = new Uint8Array(16);
  frame.set([0x24, 0x00, 0x71, 0x0f, 0x88]);
  const out = new Float32Array(28);
  const st = { h1: 0, h2: 0 };
  A.decodePSADPCM(frame, 0, 28, st, out);
  const want = [256, 2252, 3583, 4608, 3321, 175, -2384];
  want.forEach((w, i) => eq(Math.round(out[i] * 32768), w, `PS-ADPCM hand sample ${i}`));
  // clamp: header 0x40 = filter 4 (122, -60), shift 0, nibble 7 on a 32000/32000 history -> 32767
  const f2 = new Uint8Array(16); f2.set([0x40, 0, 0x77]);
  const o2 = new Float32Array(2);
  A.decodePSADPCM(f2, 0, 2, { h1: 32000, h2: 32000 }, o2);
  eq(Math.round(o2[0] * 32768), 32767, 'PS-ADPCM clamps to +32767');
}

// 2. EA-XA (IRX decxa16c), worked by hand. Header 0x14 = coefficient pair 1 (240, 0), shift 12.
//    Nibbles (high first) 7,15(-1) | 8(-8),0:
//    s0 = ((7<<28)>>12 + 0) >> 8 = 458752>>8 = 1792
//    s1 = ((-1<<28)>>12 + 1792*240) >> 8 = (-65536 + 430080)>>8 = 1424
//    s2 = ((-8<<28)>>12 + 1424*240) >> 8 = (-524288 + 341760)>>8 = -713
//    s3 = (0 + -713*240) >> 8 = -171120>>8 = -669 (floor)
{
  const frame = new Uint8Array(15); frame.set([0x14, 0x7f, 0x80]);
  const out = new Float32Array(28);
  A.decodeEAXA(frame, 0, 28, { h1: 0, h2: 0 }, out);
  [1792, 1424, -713, -669].forEach((w, i) => eq(Math.round(out[i] * 32768), w, `EA-XA hand sample ${i}`));
}

// 3. PT header parsing (tag reader 0x3C7388): FC padding, FD/FE markers, 0xFF long length.
{
  const pt = new Uint8Array([0x50, 0x54, 5, 0, 0x06, 1, 0x65, 0xfd, 0x85, 3, 0x05, 0x5c, 0x74, 0x82, 1, 2,
    0xfc, 0xfe, 0x07, 1, 0x3c, 0xfd, 0x84, 2, 0x7d, 0x00, 0xff]);
  const r = A.parsePT(pt, 0);
  eq(r.platform, 5, 'PT platform');
  eq(r.patches.length, 2, 'PT patch count');
  eq(r.patches[0].params[0x06], 0x65, 'PT param 0x06');
  eq(r.patches[0].sample[0x85], 351348, 'PT 0x85 sample count');
  eq(r.patches[0].sample[0x82], 2, 'PT 0x82 channels');
  eq(r.patches[1].sample[0x84], 32000, 'PT 0x84 rate');
  eq(r.end, pt.length, 'PT end');
}

// ---------------------------------------------------------------------------------------------
if (!fs.existsSync(path.join(ROOT, 'catalog.json'))) {
  console.error(`missing ${ROOT}/catalog.json - run: python3 tools/export_audio.py`);
  process.exit(1);
}
const rd = (p) => new Uint8Array(fs.readFileSync(path.join(ROOT, p)));
const js = (p) => JSON.parse(fs.readFileSync(path.join(ROOT, p), 'utf8'));
const catalog = js('catalog.json');
ok(catalog.songs.length >= 45, 'catalog songs');
ok(catalog.playlist[0].songs.length === 35, 'default playlist has 35 songs');
eq(catalog.mixes.length, 3, 'MIX.INF presets');

// 4. Speech: DJ Atomika (stereo) and a character line (mono).
for (const name of ['DJ_Aggression_eng', 'Aggression_Response_ari']) {
  const idx = js(`speech/${name}.json`);
  const dat = rd(`speech/${name}.dat`);
  ok(idx.lines.length > 10, `${name}: line count`);
  for (const line of [idx.lines[0], idx.lines[idx.lines.length - 1]]) {
    const info = A.parseStream(dat, line.offset);
    eq(info.end - line.offset, line.size, `${name}#${line.index}: stream size`);
    eq(info.decodedSamples, info.sampleCount, `${name}#${line.index}: SCDl samples == header 0x85`);
    const d = A.decodeSpeechLine(dat, line);
    eq(d.length, line.sampleCount, `${name}#${line.index}: length`);
    eq(d.sampleRate, 22050, `${name}#${line.index}: rate`);
    eq(d.channels, line.channels, `${name}#${line.index}: channels`);
    sanity(d, `${name}#${line.index}`, { minRms: 0.02 });
  }
}

// 5. Banks: MicroTalk (grunts + a looped ambience + a stereo loop), PS-ADPCM (SPU), PCM8, EA-XA.
function bankCase(file, want) {
  const u8 = rd(`banks/${file}`);
  const bank = A.parseBank(u8);
  const meta = js(`banks/${file.replace(/\.[^.]+$/, '')}.json`);
  eq(bank.count, meta.count, `${file}: entry count`);
  const entries = bank.entries.filter(Boolean);
  let n = 0;
  for (const e of entries) {
    for (const p of e.patches) {
      if (want && A.CODEC_NAMES[p.codec] !== want) continue;
      const d = A.decodeBankPatch(u8, p);
      eq(d.length, p.sampleCount, `${file}#${e.index}: length`);
      eq(d.channels, p.channels, `${file}#${e.index}: channels`);
      sanity(d, `${file}#${e.index}`, { minRms: 0.001 });
      if (p.codec === A.CODEC.MICROTALK && p.loopStart < 0) {
        // bitstream consistency: the last frame ends within the padding before the next sound
        const end = A.decodeMicroTalk(u8, p.dataOffsets[0], p.sampleCount, new Float32Array(p.sampleCount));
        const next = Math.min(u8.length, ...entries.flatMap((x) => x.patches.flatMap((q) => q.dataOffsets)).filter((o) => o > p.dataOffsets[0]));
        ok(end <= next && next - end <= 16, `${file}#${e.index}: MicroTalk ends at ${end}, next data at ${next}`);
      }
      if (p.codec === A.CODEC.VAG) {
        const last = p.dataOffsets[0] + (Math.ceil(p.sampleCount / 28) - 1) * 16;
        ok((u8[last + 1] & 1) === 1, `${file}#${e.index}: last VAG frame carries the end flag`);
      }
      n++;
    }
  }
  ok(n > 0, `${file}: decoded ${n} ${want ?? ''} patches`);
}
bankCase('GRNT_ZOE.BNK', 'microtalk');
bankCase('Bear_1.bnk', 'microtalk');
bankCase('MineShaft.bnk', 'microtalk');
bankCase('zboardSPU.bnk', 'ps-adpcm');
bankCase('SSX3Menu.bnk', 'ps-adpcm');
bankCase('zboard.bnk', 'pcm8');
bankCase('Transport.bnk', null);

// 6. Music: the Pathfinder graph of "Go" and its audio.
{
  const song = js('music/Go.json');
  eq(song.title, 'Go', 'Go title');
  eq(song.counts.nodes, song.nodes.length, 'Go node count');
  eq(song.validation.sampleDurationMismatches, 0, 'Go sample table durations match the stream headers');
  const stream = rd(`music/${song.tracks[0].file}`);
  const loops = rd(`music/${song.tracks[1].file}`);
  // the first node that plays audio from the stream track
  const node = song.nodes.find((n) => n.sample > 0 && song.samples[n.sample - 1].kind === 'stream');
  const s = song.samples[node.sample - 1];
  const d = A.decodeMusicSample(stream, loops, s);
  eq(d.length, s.sampleCount, 'Go stream segment length');
  eq(d.sampleRate, 32000, 'Go stream rate');
  eq(Math.floor(d.length * 1000 / d.sampleRate), s.ms, 'Go segment ms from the .mpf');
  // bar length at 135 BPM, 4 beats
  ok(Math.abs(d.length / d.sampleRate - 4 * 60 / song.inf.BPM) < 0.001, 'Go segment is one 4-beat bar at the INF BPM');
  sanity(d, 'Go stream segment', { minRms: 0.01 });
  const b = song.samples.find((x) => x.kind === 'bank');
  const db = A.decodeMusicSample(stream, loops, b);
  eq(db.length, b.sampleCount, 'Go loop-bank segment length');
  ok(Math.abs(Math.floor(db.length * 1000 / db.sampleRate) - b.ms) <= 2, 'Go loop-bank segment ms');
  sanity(db, 'Go loop-bank segment', { minRms: 0.01 });
  // every branch target is a valid node
  const bad = song.nodes.flatMap((n) => n.branches).filter((br) => br.node >= song.nodes.length);
  eq(bad.length, 0, 'Go branch targets in range');
}

// 7. "Screw Up": the one song whose stream bars are MicroTalk (codec2 4, tag 0x80 = 2). The stream voice (0x3C96F0)
// skips each block's first byte, reads the header once per SCHl and keeps the decoder state across SCDl blocks in plain
// MicroTalk mode (no EA flag bytes); bit-exactness against the original is test-microtalk.mjs.
{
  const song = js('music/Screw_Up.json');
  const stream = rd(`music/${song.tracks[0].file}`);
  const node = song.nodes.find((n) => n.sample > 0 && song.samples[n.sample - 1].kind === 'stream');
  const s = song.samples[node.sample - 1];
  eq(s.codec, 'microtalk', 'Screw Up stream codec');
  const info = A.parseStream(stream.subarray(s.offset, s.offset + s.size), 0);
  eq(info.version, 2, 'Screw Up stream tag 0x80 (plain MicroTalk below 3)');
  const d = A.decodeMusicSample(stream, null, s);
  eq(d.length, s.sampleCount, 'Screw Up stream segment length');
  eq(d.sampleRate, 32000, 'Screw Up stream rate');
  eq(d.channels, 2, 'Screw Up stream channels');
  const { rms } = sanity(d, 'Screw Up stream segment', { minRms: 0.05 });
  // Music, not decoder noise: sample-to-sample steps are a small fraction of the level (the EA-wrapped misread gave
  // RMS ~0.8 with steps ~0.25), and no SCDl block boundary (2160 samples) jumps more than the bar's largest steps.
  let step = 0, maxStep = 0, joinMax = 0;
  for (const ch of d.data) for (let i = 1; i < ch.length; i++) {
    const x = Math.abs(ch[i] - ch[i - 1]); step += x;
    if (i % 2160 === 0) joinMax = Math.max(joinMax, x); else maxStep = Math.max(maxStep, x);
  }
  step /= d.channels * (d.length - 1);
  ok(rms < 0.5 && step < rms / 4, `Screw Up segment: rms ${rms.toFixed(3)}, mean step ${step.toFixed(4)}`);
  ok(joinMax <= maxStep, `Screw Up segment: block joins (max ${joinMax.toFixed(4)}) within the bar's steps (${maxStep.toFixed(4)})`);
}

console.log(`${checks - failures}/${checks} audio checks passed`);
if (failures) process.exit(1);
