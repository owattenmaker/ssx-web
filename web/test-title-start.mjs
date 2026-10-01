// docs/audio-menus.md "Title Press START"): the title's Start plays SSX3Menu snd 7 (FE event 15, cFEStateTitle's notify
// 0x1946A8) and snd 3 (event 0, the menu's UINext accept) on one frame (PS2 local/ps2-capture/menus/title-start), on the press that also
// unlocks the browser's audio: the bank is fetched and decoded before that press, so both voices start in the press's own handler.
// node test-title-start.mjs (skips without web/public/assets/AUDIO)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { sourceOf } from './test-source.mjs';

const root = new URL('./public', import.meta.url).pathname;
if (!fs.existsSync(root + '/assets/AUDIO/banks/SSX3Menu.json')) { console.log('title start: skipped (no web/public/assets/AUDIO)'); process.exit(0); }
const mod = await import('./game-audio.js').catch((e) => { if (/pathfinder/.test(String(e))) return null; throw e; });
if (!mod) { console.log('title start: skipped (web/pathfinder.js not present)'); process.exit(0); }
const { createGameAudio, UI_SOUND } = mod;

// A Web Audio stand-in: created by the unlock, suspended until resume() (as a first gesture leaves it for a moment).
const param = (v = 1) => ({ value: v, setValueAtTime(x) { this.value = x; }, linearRampToValueAtTime() {}, cancelScheduledValues() {}, cancelAndHoldAtTime() {}, setValueCurveAtTime() {}, setTargetAtTime() {} });
const node = (extra = {}) => ({ connect() {}, disconnect() {}, ...extra });
const made = [];
globalThis.AudioContext = class {
  constructor() { made.push(this); this.currentTime = 0; this.state = 'suspended'; this.destination = node(); this.sources = []; this.listeners = []; }
  addEventListener(t, f) { if (t === 'statechange') this.listeners.push(f); }
  resume() { return Promise.resolve().then(() => { this.state = 'running'; for (const f of this.listeners) f(); }); }
  suspend() { this.state = 'suspended'; return Promise.resolve(); }
  createGain() { return node({ gain: param() }); }
  createBiquadFilter() { return node({ type: '', frequency: param(), Q: param() }); }
  createStereoPanner() { return node({ pan: param(0) }); }
  createBuffer(ch, len, rate) { const data = Array.from({ length: ch }, () => new Float32Array(len)); return { numberOfChannels: ch, length: len, sampleRate: rate, duration: len / rate, copyToChannel(x, c) { data[c].set(x); }, getChannelData(c) { return data[c]; } }; }
  createBufferSource() { const s = node({ buffer: null, loop: false, detune: param(0), playbackRate: param(1), onended: null, start(when) { s.when = when; }, stop() {} }); this.sources.push(s); return s; }
};

async function session() {
  const fetched = [];
  const ga = createGameAudio({ fetchJson: async (p) => { fetched.push(p.replace(/^.*AUDIO\//, '')); return JSON.parse(fs.readFileSync(root + p, 'utf8')); },
    fetchBytes: async (p) => { fetched.push(p.replace(/^.*AUDIO\//, '')); return new Uint8Array(fs.readFileSync(root + p)); } });
  await ga.whenReady();
  for (let i = 0; i < 200 && !fetched.includes('banks/SSX3Menu.bnk'); i++) await new Promise((r) => setTimeout(r, 5));
  await new Promise((r) => setTimeout(r, 300));                       // the bank's decode (in-thread here)
  const locked = !ga.debug().unlocked;
  // the title's Start: the unlock (audio-engine.js installAudioUnlock, capture phase) and then, in the same handler, ui.js leaveTitle
  ga.unlock(); ga.ui(15); ga.ui(0);
  const tags = ga.debug().byTag || {}, stateAtPlay = made.at(-1)?.state;
  return { fetchedBefore: fetched.includes('banks/SSX3Menu.bnk'), locked, tags, stateAtPlay };
}

assert.equal(UI_SOUND[15], 7, '294F78: event 15 -> snd 7'); assert.equal(UI_SOUND[0], 3, 'event 0 -> snd 3');
const on = await session();
assert.ok(on.locked && on.fetchedBefore, 'SSX3Menu is loaded before the first gesture');
assert.ok((on.tags['ui:0/7'] ?? 0) >= 1 && (on.tags['ui:0/3'] ?? 0) >= 1, 'both voices start on the unlocking press: ' + JSON.stringify(on.tags));
assert.equal(on.stateAtPlay, 'suspended', 'started while the context was still resuming (they sound once it runs)');
const ui = sourceOf('ui.js'), menu = fs.readFileSync(new URL('./audio-menu.js', import.meta.url), 'utf8');
assert.ok(/at:performance\.now\(\)\};try\{this\.gameAudio\?\.ui\?\.\(15\);this\.gameAudio\?\.ui\?\.\(0\);\}catch\{\}/.test(ui), 'ui.js leaveTitle plays events 15 and 0');
assert.ok(/s === 'title'\) return;/.test(menu), 'audio-menu.js leaves the title to leaveTitle (no second accept)');
console.log('title start: SSX3Menu before the first gesture, snd 7 + snd 3 on the unlocking press');
