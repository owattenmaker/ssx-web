// GPU device loss recovery (docs/mobile.md). Field reports: iOS Safari loses the WebGPU device with reason "destroyed"
// (while the tab is in the background, or during a heavy load), Android Chrome with "A valid external Instance reference
// no longer exists" after a minute in the background. three.js r186 ignores a "destroyed" loss entirely (WebGPUBackend
// returns early), so every later frame threw "GPUDevice.createCommandEncoder: Unable to make command encoder" (77 errors
// in a second on one iPhone) and the tab stayed black; other reasons stop drawing for good (renderer._isDeviceLost).
//
// Here: any loss of the renderer's current device stops three's drawing at once (renderer._isDeviceLost, which render,
// compile and compute check), then, as soon as the page is visible, the same WebGPURenderer gets a new adapter + device
// and fresh backend/manager state (renderer.init() again); scene objects, materials, textures and render targets keep
// their CPU data, so they upload again on the next frames and every pipeline recompiles. The animation loop moves over.
// Nothing outside three holds GPU objects (main.js only reads device.queue), so the modules that captured `renderer`
// keep working. If the new device cannot be made (3 tries), or the device keeps getting lost (3 losses in 2 minutes),
// the page reloads (at most twice in 5 minutes, sessionStorage), and after that onFailed() shows the game's own message.
// A WebGL backend (?backend=webgl or the fallback) reloads on a lost context.
const RELOAD_KEY = 'ssx3.gpu-reloads';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const visible = () => typeof document === 'undefined' || document.visibilityState === 'visible';
const whenVisible = () => visible() ? Promise.resolve() : new Promise((r) => { const on = () => { if (visible()) { document.removeEventListener('visibilitychange', on); r(); } }; document.addEventListener('visibilitychange', on); });

// Fresh device-bound state on the existing renderer, then renderer.init() makes the new device and managers.
// beforeResume: awaited once the new device exists and before drawing resumes (web/gpu-copies.js restoreGpuCopies: the arrays the page
// dropped after their upload); a throw is a failed attempt (retried, then the page reloads).
export async function reinitRenderer(renderer, { beforeResume = null } = {}) {
  const backend = renderer.backend, loop = renderer._animation?.getAnimationLoop?.() ?? null;
  renderer._animation?.dispose?.(); // the old requestAnimationFrame chain stops; the new Animation takes the loop
  // The old managers stay subscribed to 'dispose' on every geometry / texture / material / render target they saw, and
  // would clean up through the shared backend, whose data is about to be the new device's (a terrain patch disposed after
  // a recovery threw "reading 'destroy'"). Give them a backend that does nothing and an Info of their own.
  const tomb = new Proxy({ get: () => ({}), has: () => false, delete: () => null }, { get: (t, k) => (k in t ? t[k] : k === 'then' ? undefined : () => undefined) });
  const tombInfo = renderer.info?.constructor ? new renderer.info.constructor() : null;
  for (const key of ['_nodes', '_attributes', '_background', '_geometries', '_textures', '_pipelines', '_bindings', '_objects', '_renderContexts']) {
    const manager = renderer[key]; if (!manager) continue;
    if ('backend' in manager) manager.backend = tomb;
    if ('info' in manager && tombInfo) manager.info = tombInfo;
  }
  for (const key of ['utils', 'attributeUtils', 'bindingUtils', 'capabilities', 'pipelineUtils', 'textureUtils']) if (backend[key]) backend[key] = new backend[key].constructor(backend);
  backend.data = new WeakMap(); backend.device = null; backend.defaultRenderPassdescriptor = null; backend.occludedResolveCache = new Map();
  if (backend.timestampQueryPool) for (const k of Object.keys(backend.timestampQueryPool)) backend.timestampQueryPool[k] = null;
  renderer.info?.dispose?.(); // the memory counters restart with the new managers (web/diagnostics.js reads them)
  renderer._initialized = false; renderer._initPromise = null;
  // no WebGL fallback here: this canvas already has a WebGPU context, and three would swap renderer.backend for it
  const fallback = renderer._getFallback; renderer._getFallback = null;
  try { await renderer.init(); } // throws when no adapter / device can be had
  catch (e) { renderer.backend = backend; throw e; }
  finally { renderer._getFallback = fallback; }
  if (beforeResume) await beforeResume();
  renderer._isDeviceLost = false;
  if (loop) await renderer.setAnimationLoop(loop);
  return backend.device;
}

function reloadBudget(storage) {
  try { const now = Date.now(), recent = JSON.parse(storage?.getItem(RELOAD_KEY) || '[]').filter((t) => now - t < 300000); return { recent, ok: recent.length < 2, note() { storage?.setItem(RELOAD_KEY, JSON.stringify([...recent, now])); } }; }
  catch { return { recent: [], ok: false, note() {} }; }
}

// hooks: onLost(info), onRecovered({ms, attempts, device}), onFailed(reason), report(kind, data) (diagnostics).
export function installDeviceRecovery(renderer, { onLost, onRecovered, onFailed, beforeResume = null, report = () => {}, reload = () => location.reload(), storage = globalThis.sessionStorage } = {}) {
  const backend = renderer?.backend; if (!backend) return null;
  const state = { losses: [], recovering: false, recoveries: 0, failed: false, device: null };
  const fail = (reason) => {
    if (state.failed) return; state.failed = true;
    const budget = reloadBudget(storage);
    report('gpu-recovery-failed', { reason, reload: budget.ok, reloadsRecently: budget.recent.length });
    if (budget.ok) { budget.note(); whenVisible().then(() => reload()); return; }
    onFailed?.(reason);
  };
  if (!backend.isWebGPUBackend) { // WebGL: three sets _isDeviceLost on webglcontextlost; a reload is the clean way back
    renderer.onDeviceLost = (info) => { renderer._isDeviceLost = true; onLost?.(info); report('gpu-device-lost', { reason: info?.reason ?? null, message: String(info?.message ?? '').slice(0, 300), backend: 'webgl' }); fail('webgl context lost'); };
    return state;
  }
  renderer.onDeviceLost = () => {}; // three's default logs and gives up; the watcher below handles every reason
  const watch = (device) => {
    if (!device || device === state.device) return; state.device = device;
    device.lost?.then((info) => { if (device === backend.device && !renderer.__ssxDisposed) lost(info); });
  };
  async function recover() {
    if (state.recovering) return; state.recovering = true;
    try {
      await whenVisible(); await sleep(250); // mobile browsers restore the GPU process when the tab comes back
      const t0 = performance.now();
      for (let attempt = 1; attempt <= 3; attempt++) {
        try {
          const device = await reinitRenderer(renderer, { beforeResume }); watch(device); state.recoveries++;
          report('gpu-recovered', { ms: Math.round(performance.now() - t0), attempt });
          onRecovered?.({ ms: performance.now() - t0, attempts: attempt, device });
          return;
        } catch (e) { report('gpu-recovery-attempt', { attempt, message: String(e?.message ?? e).slice(0, 300) }); await sleep(1000 * attempt); await whenVisible(); }
      }
      fail('no new device');
    } finally { state.recovering = false; }
  }
  function lost(info) {
    renderer._isDeviceLost = true; // render / compile / compute return at once: no calls on the dead device
    const now = Date.now(); state.losses = [...state.losses.filter((t) => now - t < 120000), now];
    onLost?.(info);
    if (state.losses.length >= 3) { fail('device lost 3 times in 2 minutes'); return; }
    recover();
  }
  watch(backend.device);
  return { state, simulateLoss: () => backend.device?.destroy() };
}
