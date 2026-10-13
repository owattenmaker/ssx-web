// Cases for native/tests/pad_input_test.cpp: web/pad-input.js buildPad on random keyboards, standard pads and Simple-mode key
// sequences (a fixed seed), each frame's 24 channels as float32 bits.
//   node pad-cases.mjs OUT.json          write the cases
//   node pad-cases.mjs --run TEST_EXE    write them to a temporary file and run the native test on them (ctest frontend_pad_input)
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { buildPad, createKeyboardContext } from '../../web/pad-input.js';

let seed = 0x5eed1234;
const rand = () => {
  seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
  return seed / 4294967296;
};
const KEYS = ['KeyA', 'KeyD', 'KeyW', 'KeyS', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'KeyI', 'KeyJ', 'KeyK', 'KeyL', 'KeyT', 'KeyF',
  'KeyG', 'KeyH', 'Space', 'ShiftLeft', 'ShiftRight', 'KeyC', 'KeyY', 'KeyQ', 'KeyE', 'KeyZ', 'KeyX', 'KeyV', 'Backspace', 'Enter'];
const value = () => {
  const r = rand();
  if (r < 0.3) return 0;
  if (r < 0.45) return 1;
  if (r < 0.5) return -1 + rand() * 3;
  return rand();
};
const bits = (f) => new Uint32Array(Float32Array.from(f).buffer);
const cases = [];
for (let c = 0; c < 400; c++) {
  const simple = rand() < 0.5;
  const kb = createKeyboardContext(simple ? 'Simple' : 'Classic');
  const frames = [];
  let held = new Set();
  for (let f = 0; f < 12; f++) {
    for (const k of KEYS) if (rand() < 0.08) held.has(k) ? held.delete(k) : held.add(k);
    kb.air = rand() < 0.4;
    const usePad = rand() < 0.6;
    const pad = usePad
      ? { buttons: Array.from({ length: 17 }, () => ({ pressed: rand() < 0.3, value: value() })), axes: Array.from({ length: 4 }, () => (rand() < 0.2 ? 0 : -1.2 + rand() * 2.4)) }
      : null;
    const out = buildPad((code) => held.has(code), pad, kb);
    frames.push({ keys: [...held], air: kb.air, pad, out: Array.from(bits(out)) });
  }
  cases.push({ mode: simple ? 'Simple' : 'Classic', frames });
}
if (process.argv[2] === '--run') {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-pad-'));
  const file = path.join(dir, 'pad-cases.json');
  fs.writeFileSync(file, JSON.stringify(cases));
  const r = spawnSync(process.argv[3], [file], { stdio: 'inherit' });
  fs.rmSync(dir, { recursive: true, force: true });
  process.exit(r.status ?? 1);
}
fs.writeFileSync(process.argv[2], JSON.stringify(cases));
console.log(`${cases.length} cases`);
