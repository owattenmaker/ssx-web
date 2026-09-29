// MicroTalk bank patches and MicroTalk music streams ("Screw Up"): every patch / segment decodes to finite samples, and
// web/audio-decode.js matches the original decoder (tools/test_microtalk_native.py -> local/reference/microtalk: the
// recompiled 0x3CDE68 for bank patches, the recompiled stream voice read 0x3C96F0 for music segments, run on the same
// bytes) sample for sample. node test-microtalk.mjs  (the oracle comparison is skipped when the reference is absent).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { decodeBankPatch, decodeMusicSample, decodeStats } from './audio-decode.js';

const BANKS = new URL('./public/assets/AUDIO/banks/', import.meta.url);
const MUSIC = new URL('./public/assets/AUDIO/music/', import.meta.url);
const REF = new URL('../local/reference/microtalk/', import.meta.url);
if (!fs.existsSync(BANKS)) { console.log('microtalk: no exported banks, skipped'); process.exit(0); }
const warn = console.warn; let warned = 0; console.warn = (...a) => { warned++; warn(...a); };

// 1. Every MicroTalk patch of every bank decodes finite, within [-1, 1].
let patches = 0;
const jsonOf = (name) => JSON.parse(fs.readFileSync(new URL(name + '.json', BANKS)));
const bytesOf = (file) => new Uint8Array(fs.readFileSync(new URL(file, BANKS)));
for (const f of fs.readdirSync(BANKS).filter((n) => n.endsWith('.json'))) {
  const doc = jsonOf(f.slice(0, -5)); if (!doc.entries) continue;
  let bnk = null;
  for (const e of doc.entries) for (const p of e?.patches ?? []) {
    if (p.codec !== 4) continue;
    bnk ??= bytesOf(doc.file);
    const before = decodeStats.nonFinite, d = decodeBankPatch(bnk, p);
    assert.equal(decodeStats.nonFinite, before, `${f} entry ${e.index}: non-finite decoder output`);
    for (const ch of d.data) for (let i = 0; i < ch.length; i++) assert.ok(ch[i] >= -1 && ch[i] <= 1, `${f} entry ${e.index}: sample ${i} = ${ch[i]}`);
    patches++;
  }
}
console.log(`microtalk: ${patches} bank patches decode finite`);

// 1b. Every MicroTalk music segment decodes finite, within [-1, 1], at a music level (not noise: before 2026-09-25 the
// stream path either threw or read the segments as EA-wrapped bank data, RMS ~0.8 at full scale).
const songs = new Map(); // song -> { doc, bytes: Map(file -> Uint8Array), decoded: Map(sample index -> decoded) }
const songBytes = (song, file) => {
  if (!song.bytes.has(file)) song.bytes.set(file, new Uint8Array(fs.readFileSync(new URL(file, MUSIC))));
  return song.bytes.get(file);
};
const decodeSegment = (song, s) => {
  if (!song.decoded.has(s.index)) {
    const track = song.doc.tracks.find((t) => t.index === s.track);
    song.decoded.set(s.index, decodeMusicSample(songBytes(song, track.file), null, s));
  }
  return song.decoded.get(s.index);
};
let segments = 0;
if (fs.existsSync(MUSIC)) for (const f of fs.readdirSync(MUSIC).filter((n) => n.endsWith('.json'))) {
  const doc = JSON.parse(fs.readFileSync(new URL(f, MUSIC)));
  const mt = (doc.samples ?? []).filter((s) => s.kind === 'stream' && s.codec === 'microtalk');
  if (!mt.length) continue;
  const song = { doc, bytes: new Map(), decoded: new Map() };
  songs.set(f.slice(0, -5), song);
  let sq = 0, n = 0;
  for (const s of mt) {
    const before = decodeStats.nonFinite, d = decodeSegment(song, s);
    assert.equal(decodeStats.nonFinite, before, `${f} segment ${s.index}: non-finite decoder output`);
    assert.equal(d.length, s.sampleCount, `${f} segment ${s.index}: length`);
    for (const ch of d.data) for (let i = 0; i < ch.length; i++) {
      assert.ok(ch[i] >= -1 && ch[i] <= 1, `${f} segment ${s.index}: sample ${i} = ${ch[i]}`);
      sq += ch[i] * ch[i]; n++;
    }
    segments++;
  }
  const rms = Math.sqrt(sq / n);
  assert.ok(rms > 0.05 && rms < 0.5, `${f}: MicroTalk music RMS ${rms.toFixed(3)} (EA-XA songs ~0.2-0.3)`);
  console.log(`microtalk: ${f} ${mt.length} music segments decode finite (RMS ${rms.toFixed(3)})`);
}

// 2. Oracle: the original's raw float output, clamped to s16 like the JS output.
if (fs.existsSync(new URL('jobs.json', REF))) {
  const { banks } = JSON.parse(fs.readFileSync(new URL('jobs.json', REF)));
  let streams = 0, samples = 0, worst = 0;
  for (const [name, bank] of Object.entries(banks)) {
    const doc = jsonOf(name), bnk = bytesOf(bank.file);
    const raw = fs.readFileSync(new URL(bank.reference, REF)), ref = new Float32Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.length));
    const decoded = new Map();
    let at = 0;
    for (const j of bank.jobs) {
      const key = `${j.entry}/${j.patch}`;
      if (!decoded.has(key)) decoded.set(key, decodeBankPatch(bnk, doc.entries[j.entry].patches[j.patch]));
      const out = decoded.get(key).data[j.channel];
      for (let i = 0; i < j.count; i++) {
        const r = ref[at + i], e = Math.fround((r > 32767 ? 32767 : r < -32768 ? -32768 : r) / 32768), g = out[j.start + i];
        const d = Math.abs(e - g); if (d > worst) worst = d;
        if (d !== 0) assert.fail(`${name} entry ${j.entry}/${j.patch} ch ${j.channel} ${j.part}: sample ${j.start + i} original ${r} decoder ${g * 32768}`);
      }
      at += j.count; streams++; samples += j.count;
    }
  }
  console.log(`microtalk: ${streams} streams (${samples} samples) match the original decoder sample for sample`);
  // Music segments: the original stream voice (0x3C96F0) per segment and channel, jobs back to back.
  const music = JSON.parse(fs.readFileSync(new URL('jobs.json', REF))).music ?? {};
  for (const name of songs.keys()) assert.ok(music[name], `${name}: no original reference (rerun python3 tools/test_microtalk_native.py)`);
  let segJobs = 0, segSamples = 0;
  for (const [name, entry] of Object.entries(music)) {
    const song = songs.get(name);
    assert.ok(song, `${name}: reference without MicroTalk segments`);
    const raw = fs.readFileSync(new URL(entry.reference, REF)), ref = new Float32Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.length));
    let at = 0;
    for (const j of entry.jobs) {
      const s = song.doc.samples[j.sample], out = decodeSegment(song, s).data[j.channel];
      assert.equal(out.length, j.count, `${name} segment ${j.sample} ch ${j.channel}: length`);
      for (let i = 0; i < j.count; i++) {
        const r = ref[at + i], e = Math.fround((r > 32767 ? 32767 : r < -32768 ? -32768 : r) / 32768), g = out[i];
        if (e !== g) assert.fail(`${name} segment ${j.sample} ch ${j.channel}: sample ${i} original ${r} decoder ${g * 32768}`);
      }
      at += j.count; segJobs++; segSamples += j.count;
    }
    assert.equal(at, ref.length, `${name}: reference length`);
  }
  console.log(`microtalk: ${segJobs} music segment channels (${segSamples} samples) match the original stream voice sample for sample`);
} else console.log('microtalk: local/reference/microtalk missing (python3 tools/test_microtalk_native.py), oracle comparison skipped');
assert.equal(warned, 0, 'the non-finite sample warning fired');
