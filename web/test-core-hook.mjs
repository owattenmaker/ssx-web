// web/ai-racers.js coreHook / coreHost: a page hook the core calls from inside a rider context its own rider manager switched to
// (pv coreRiderManager: web/rider_manager.cpp inContext sets __tls_base without the page's views) must leave that context current
// when it returns, also when it calls a view. Without the wrapper the view's stale mirror put the human's block back, and the rest of
// a computer rider's race_end ran in the human's context (a Single Event slope style's live run: the opponent's finish zeroed the
// human's boost meter, docs/HANDOFF.md 2026-10-13).
import assert from 'node:assert/strict';
import { coreHook, coreHost, riderContextCore, riderTlsCurrent } from './ai-racers.js';

const coreUrl = process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js';
const createCore = (await import(coreUrl)).default;
const core = await createCore();
if (!core._rider_context_create || !core.___tls_base) {
  console.log('core-hook: this core has no rider contexts (skipped)');
  process.exit(0);
}
const human = core.___tls_base.value;
riderTlsCurrent(core);
const block = core._rider_context_create();
const view = riderContextCore(core, block);
// what the core's rider manager does: the computer rider's block current, the page's mirror still naming the human's
const managerSwitch = () => {
  core.___tls_base.value = block;
};
const hook = (wrapped) => {
  managerSwitch();
  wrapped();
  return core.___tls_base.value;
};
// unwrapped: the view's restore puts the human's block back (the bug)
assert.equal(hook(() => view._rider_state()), human, 'the stale mirror without the wrapper');
core.___tls_base.value = human;
// wrapped: the computer rider's block stays current, and the mirror names the human's again after the hook
assert.equal(hook(coreHook(core, () => view._rider_state())), block, 'coreHook keeps the switched context');
assert.equal(core.__riderTls.current, human, 'the mirror back to the block current before the hook');
core.___tls_base.value = human;
// a host object: every method wrapped, arguments and this passed on
const seen = [];
const host = coreHost(core, {
  tag: 7,
  place(a, b, c, d, e, f) {
    seen.push([this.tag, a, b, c, d, e, f]);
    view._rider_state();
    return 3;
  }
});
managerSwitch();
assert.equal(host.place(1, 2, 3, 4, 5, 6), 3);
assert.equal(core.___tls_base.value, block, 'coreHost keeps the switched context');
assert.deepEqual(seen, [[7, 1, 2, 3, 4, 5, 6]]);
core.___tls_base.value = human;
console.log('core-hook: hooks keep the rider context the core switched to');
