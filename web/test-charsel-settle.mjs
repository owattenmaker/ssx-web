// pv charselSettle (docs/characters.md "Select Character: cursor and preview load"), without a browser:
//  - web/downloads.js: a shared /assets download with callers that pass a signal stops once all of them aborted, and runs on
//    while any caller (or one without a signal) still wants it; with the switch off a signalled fetch bypasses the share;
//  - web/character-select.js: the cursor moves on the press; the rider commit (ui.cb.rider) starts once frame > first change + 4
//    (19E588 +0xCD4 / 19F138); a move while it waits only retargets; a later commit aborts the one in flight; coming back to the
//    shown rider cancels the wait; Cross and leaving the screen commit at once;
//  - pv charselEntryOutfit: entering the screen reloads a rider whose saved outfit is not prepared yet.
import assert from 'node:assert/strict';
import { setPv } from './pv-flags.js';

globalThis.location = new URL('http://game.test/');
// ---- downloads.js -----------------------------------------------------------------------------------------------
const native = [];
globalThis.fetch = async (url, init) => {
  const call = { url: String(url), signal: init?.signal ?? null, aborted: false };
  native.push(call);
  let push = null;
  const stream = new ReadableStream({
    start(c) {
      push = c;
    }
  });
  call.finish = () => {
    push.enqueue(new Uint8Array([1, 2, 3]));
    push.close();
  };
  call.signal?.addEventListener('abort', () => {
    call.aborted = true;
    push.error(Object.assign(Error('aborted'), { name: 'AbortError' }));
  });
  return new Response(stream, { headers: { 'content-length': '3' } });
};
console.warn = () => {};
const { downloadState } = await import('./downloads.js');
const tick = () => new Promise((r) => setTimeout(r, 5));
const isAbort = (e) => e?.name === 'AbortError';

setPv('charselSettle', true);
{
  // 1. two signalled callers: one aborts, the download runs on for the other
  const a = new AbortController(),
    b = new AbortController();
  const pa = fetch('/assets/x.tex', { signal: a.signal }),
    pb = fetch('/assets/x.tex', { signal: b.signal });
  await tick();
  assert.equal(native.length, 1, 'one shared download');
  a.abort();
  await assert.rejects(pa, isAbort);
  assert.equal(native[0].aborted, false, 'still wanted by the second caller');
  native[0].finish();
  assert.deepEqual([...new Uint8Array(await (await pb).arrayBuffer())], [1, 2, 3]);
}
{
  // 2. the only caller aborts: the download stops and a later request starts a new one
  native.length = 0;
  const a = new AbortController();
  const pa = fetch('/assets/y.tex', { signal: a.signal });
  await tick();
  a.abort();
  await assert.rejects(pa, isAbort);
  await tick();
  assert.equal(native[0].aborted, true, 'the download stopped');
  assert.equal(downloadState.active, 0);
  const again = fetch('/assets/y.tex');
  await tick();
  assert.equal(native.length, 2, 'a new download after the abort');
  native[1].finish();
  await again;
}
{
  // 3. a caller without a signal keeps the download to its end
  native.length = 0;
  const a = new AbortController();
  const pa = fetch('/assets/z.tex', { signal: a.signal }),
    plain = fetch('/assets/z.tex');
  await tick();
  a.abort();
  await assert.rejects(pa, isAbort);
  assert.equal(native[0].aborted, false);
  native[0].finish();
  await plain;
}
setPv('charselSettle', false);
{
  // 4. switch off: a signalled fetch is not shared (the page's fetch as before)
  native.length = 0;
  const a = new AbortController();
  const p = fetch('/assets/w.tex', { signal: a.signal });
  await tick();
  assert.equal(native[0].signal, a.signal, 'the caller signal goes to the native fetch');
  native[0].finish();
  await p;
}

// ---- character-select.js -------------------------------------------------------------------------------------------
setPv('charselSettle', true);
let now = 0;
Object.defineProperty(performance, 'now', { value: () => now, configurable: true });
const frame = (f) => {
  now = (f * 1000) / 60 + 1;
};
const { CharacterSelect } = await import('./character-select.js');
const riders = ['zoe', 'moby', 'psymon', 'griff'].map((id, i) => ({ id, name: id, package: 'RIDER_' + id.toUpperCase(), screen_index: i }));
const picks = [];
const ui = {
  riders,
  riderIndex: 0,
  ready: true,
  screen: 'character',
  get rider() {
    return this.riders[this.riderIndex];
  },
  sync() {},
  cb: {
    rider(r, opts) {
      const p = { id: r.id, signal: opts?.signal ?? null };
      picks.push(p);
      return new Promise((resolve) => {
        p.resolve = resolve;
      });
    }
  }
};
const cs = new CharacterSelect(ui);
const settle = () => new Promise((r) => setTimeout(r, 0));
const run = (f) => {
  frame(f);
  cs.showPreview('character', false);
};
// the press moves the cursor at once; frames 1..4 wait, frame 5 commits (frame > stamp 0 + 4)
frame(0);
cs.cycle(1);
assert.equal(ui.rider.id, 'moby', 'the cursor moved on the press');
assert.equal(cs.previewPending(), true, 'the model is hidden');
for (let f = 1; f <= 4; f++) run(f);
await settle();
assert.equal(picks.length, 0, 'no commit before frame > stamp');
run(5);
await settle();
assert.deepEqual(picks.map((p) => p.id), ['moby']);
// a burst: the stamp of the first move holds, the later move only retargets
frame(10);
cs.cycle(1);
frame(12);
cs.cycle(1);
assert.equal(ui.rider.id, 'griff');
run(14);
await settle();
assert.equal(picks.length, 1, 'frame 14 is not past stamp 14');
run(15);
await settle();
assert.deepEqual(picks.map((p) => p.id), ['moby', 'griff'], 'the target at the frame, the stamp of the first move');
assert.equal(picks[0].signal.aborted, true, 'the commit in flight is superseded');
picks[1].resolve();
await settle();
await settle();
assert.equal(cs.previewPending(), false, 'committed: the preview loads');
// back to the rider whose model is the preview's: the wait is cancelled, nothing commits
cs.preview3d.root = '/assets/RIDER_GRIFF/fe/';
frame(20);
cs.cycle(-1);
frame(21);
cs.cycle(1);
assert.equal(cs.reloadAt, -1);
run(30);
await settle();
assert.equal(picks.length, 2, 'no commit for a return to the shown rider');
// Cross while the wait runs commits at once
frame(40);
cs.cycle(1);
cs.choose();
await settle();
assert.deepEqual(picks.map((p) => p.id).slice(2), ['zoe'], 'Cross commits the rider under the cursor');
assert.ok(cs.flash && cs.picking, 'the flash waits for the commit');
picks[2].resolve();
await settle();
await settle();
assert.equal(cs.picking, null);
// leaving the screen before the frame commits too
cs.flash = null;
frame(60);
cs.cycle(1);
ui.screen = 'main';
frame(61);
cs.showPreview('main', false);
await settle();
assert.deepEqual(picks.map((p) => p.id).slice(3), ['moby'], 'Triangle commits the rider under the cursor');
picks[3].resolve();
await settle();
// pv charselEntryOutfit: entering the screen with the rider's outfit not prepared reloads it (stamp, then the commit); prepared: nothing
setPv('charselEntryOutfit', true);
{
  ui.screen = 'character';
  ui.riderIndex = 0;
  const fresh = new CharacterSelect(ui);
  fresh.outfitPrepared = () => false;
  frame(100);
  fresh.onEnter();
  assert.equal(fresh.previewPending(), true, 'entry: the model waits for the saved outfit');
  frame(104);
  fresh.showPreview('character', false);
  await settle();
  assert.equal(picks.length, 4, 'no commit before frame > stamp');
  frame(105);
  fresh.showPreview('character', false);
  await settle();
  assert.deepEqual(picks.map((p) => p.id).slice(4), ['zoe'], 'entry commits the rider under the cursor (its saved outfit)');
  picks[4].resolve();
  await settle();
  await settle();
  assert.equal(fresh.previewPending(), false);
  const known = new CharacterSelect(ui);
  known.outfitPrepared = () => true;
  known.onEnter();
  assert.equal(known.previewPending(), false, 'entry with the outfit prepared: shown at once');
}
setPv('charselEntryOutfit', null);
setPv('charselSettle', null);
console.log('charsel settle OK: shared downloads abort per caller; cursor at the press, commit at frame > first change + 4, superseded');
