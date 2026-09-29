// Field diagnostics for real devices (docs/mobile.md): phones have no visible console, so the page reports what went
// wrong to the host (POST /mp/diag -> ~/ssx-host/logs/diag.log, behind the password gate, web/server/mp-server.mjs):
// errors, unhandled rejections, console.error/warn, WebGPU uncaptured errors, pipeline failures and device loss, the
// adapter's limits, screen changes (loading -> game ...), a frame-time / memory heartbeat, and a crash marker: the last
// known state and the last few events are kept in localStorage while the page runs, so after the browser kills the tab
// (iOS out of memory, a discarded background tab) the next load reports where and how the previous session ended.
// Nothing personal is sent (no ids beyond a random per-load session, no input, no names); ?diag=0 turns it off.
//
// Every event carries the screen (#stage data-screen) and the course (?course / &peakCourse / &peakMode). Repeats of
// one error are sent 3 times, then counted ('repeat' events), so a broken frame loop cannot use up the budget.
import { wgslMemory, WGSL_PRIVATE_LIMIT, WGSL_FUNCTION_LIMIT } from './wgsl-budget.js';
import { onPads, connectedPads, padSummary, activeEntry } from './gamepad.js';
import { pv } from './pv-flags.js';
import { audioStatsSnapshot } from './audio-stats.js';

const ENDPOINT = '/mp/diag', FLUSH_MS = 3000, MAX_EVENTS = 400, RESERVED = 40, MAX_BATCH = 48000, MARK = 'ssx3.diag.last', RING = 12;
const search = globalThis.location?.search ?? '';
// On the hosted site (https) unless ?diag=0; locally with ?diag=1.
const enabled = typeof window !== 'undefined' && !/[?&]diag=0\b/.test(search) && (location.protocol === 'https:' || /[?&]diag=1\b/.test(search));
const session = Math.random().toString(36).slice(2, 10), started = Date.now(), queue = [], ring = [], repeats = new Map();
// PRIORITY kinds still go out when the budget is nearly spent (the end of a long session is where crashes are).
const PRIORITY = new Set(['pagehide', 'previous-session-died', 'gpu-device-lost', 'gpu-recovered', 'gpu-recovery-failed', 'gpu-recovery-attempt', 'pipeline-failed', 'shader-over-budget', 'repeat', 'error', 'rejection', 'course-failed', 'download-failed']);
const ERRORS = new Set(['error', 'rejection', 'console.error', 'console.warn', 'gpu-error']);
let sent = 0, currentScreen = '', frames = [], lastFrame = 0, flushing = false, unloading = false, hiddenAt = 0, dropped = 0, diagRenderer = null;
const now = () => Math.round((Date.now() - started) / 100) / 10;
const clip = (s, n = 600) => String(s ?? '').slice(0, n);
// The course: the last loaded course's key (every load reports a 'course' event; an event picked from the menus never
// puts it in the URL), else the URL's ?course / &peakCourse / &peakMode.
let liveCourse = '';
function courseKey() {
  if (liveCourse) return liveCourse;
  try { const q = new URLSearchParams(location.search), c = q.get('course'); if (!c) return ''; const sub = q.get('peakCourse') ?? q.get('peakMode'); return sub != null ? `${c}/${sub}` : c; } catch { return ''; }
}
// Distance ridden (field stats): the human's position per game tick from game-tick.js (host.rideTick; rider_state 0..2 are
// browser metres), summed, skipping placements, rescues and teleports (and any step over 30 m, which only a placement makes). Reported as distM per heartbeat
// and distTotalM at pagehide; replays don't count.
let rideM = 0, rideTotalM = 0, lastRidePos = null;
export function diagRide(state, jump) {
  const x = state[0], y = state[1], z = state[2];
  if (!lastRidePos || jump) { lastRidePos = [x, y, z]; return; }
  const d = Math.hypot(x - lastRidePos[0], y - lastRidePos[1], z - lastRidePos[2]); lastRidePos[0] = x; lastRidePos[1] = y; lastRidePos[2] = z;
  if (d > 0 && d < 30) rideM += d;
}
export const diagRideStats = () => ({ rideM: rideTotalM + rideM, course: courseKey() }); // QA
function push(kind, data = {}) {
  if (!enabled) return;
  if (kind === 'course' && data.key) { liveCourse = String(data.key); lastRidePos = null; }
  const event = { t: now(), kind, screen: currentScreen, course: courseKey(), ...(unloading ? { unloading: true } : {}), ...data }; // unloading: after pagehide (aborted downloads...)
  ring.push({ t: event.t, kind, screen: event.screen, ...(data.message ? { message: clip(data.message, 120) } : data.to ? { to: data.to } : {}) }); if (ring.length > RING) ring.shift();
  if (ERRORS.has(kind)) { // the same error again: counted, reported as one 'repeat' at the next flush
    const key = kind + '|' + clip(data.message, 160), r = repeats.get(key) ?? { n: 0, accounted: 0, kind, message: clip(data.message, 160) };
    r.n++; repeats.set(key, r); if (repeats.size > 500) repeats.delete(repeats.keys().next().value);
    if (r.n > 3) return;
    r.accounted = r.n;
  }
  const used = sent + queue.length;
  if (used >= MAX_EVENTS || (used >= MAX_EVENTS - RESERVED && !PRIORITY.has(kind))) { dropped++; return; }
  queue.push(event);
}
function drainRepeats() {
  for (const r of repeats.values()) if (r.n > r.accounted) { const count = r.n - r.accounted; r.accounted = r.n; push('repeat', { of: r.kind, message: r.message, count, total: r.n }); }
}
function send(events, beacon) {
  const body = JSON.stringify({ session, ua: navigator.userAgent, events });
  if (beacon && navigator.sendBeacon && navigator.sendBeacon(ENDPOINT, new Blob([body], { type: 'application/json' }))) return Promise.resolve();
  return fetch(ENDPOINT, { method: 'POST', headers: { 'content-type': 'application/json' }, body, keepalive: body.length < 60000 }).catch(() => {});
}
// Batches stay under the server's 64 KB cap. The page-hide beacon never waits for a fetch still in flight (it used to
// return early then, so the 'pagehide' event and the clean mark's report were lost).
async function flush(beacon = false) {
  drainRepeats();
  if (!queue.length || (flushing && !beacon)) return;
  const events = queue.splice(0); sent += events.length;
  const batches = []; let batch = [], size = 0;
  for (const e of events) { const n = JSON.stringify(e).length; if (batch.length && size + n > MAX_BATCH) { batches.push(batch); batch = []; size = 0; } batch.push(e); size += n; }
  if (batch.length) batches.push(batch);
  if (beacon) { for (const b of batches) send(b, true); return; }
  flushing = true;
  try { for (const b of batches) await send(b, false); } finally { flushing = false; }
}
function memory() {
  const out = {}, m = performance.memory; // Chrome only; Safari and Firefox report no JS heap
  if (m) { out.heapMB = Math.round(m.usedJSHeapSize / 1048576); out.limitMB = Math.round(m.jsHeapSizeLimit / 1048576); }
  const g = diagRenderer?.info?.memory; // three.js's own GPU accounting (every browser): textures, buffers, programs
  // (texture bytes are exact; attribute bytes are not: each view of a shared interleaved buffer counts the whole buffer)
  if (g?.textures) { out.texMB = Math.round((g.texturesSize ?? 0) / 1048576); out.tex = g.textures; out.geo = g.geometries; out.programs = g.programs; }
  const wasm = globalThis.ssxEffects?.core?.HEAPU8?.byteLength; if (wasm) out.wasmMB = Math.round(wasm / 1048576);
  return out;
}
// The crash marker: where we are, whether the page is hidden (a background tab the OS may drop), and the event ring.
function remember(extra = {}) {
  if (unloading) return; // after pagehide: the auto-pause on hide must not overwrite the clean mark (docs/mobile.md)
  try { localStorage.setItem(MARK, JSON.stringify({ session, screen: currentScreen, course: courseKey(), at: Date.now(), up: now(), clean: false, hidden: document.visibilityState === 'hidden', hiddenAt: hiddenAt || undefined, ...memory(), last: ring.slice(-RING), ...extra })); } catch {}
}

// ---- stall attribution (stall / hitch events): what the page did between two frames ----
const gap = { pipes: 0, pipeMs: 0, apipes: 0, shaders: 0, tex: 0, bufs: 0, binds: 0, builds: 0, marks: [] };
const total = { pipes: 0, apipes: 0, shaders: 0, tex: 0, bufs: 0, builds: 0 }; // since the page started (gpu-stall: the probe window's share)
let pendingPipes = 0;
function resetGap() { gap.pipes = gap.apipes = gap.shaders = gap.tex = gap.bufs = gap.binds = gap.builds = 0; gap.pipeMs = 0; gap.marks.length = 0; }
function gapMark(name) { if (gap.marks.length < 6) gap.marks.push(clip(name, 40)); }
function stallCause(busy) {
  const c = { busyMs: Math.round(busy) };
  for (const k of ['pipes', 'apipes', 'shaders', 'tex', 'bufs', 'binds', 'builds']) if (gap[k]) c[k] = gap[k];
  if (gap.pipes) c.pipeMs = Math.round(gap.pipeMs);
  if (pendingPipes) c.compiling = pendingPipes; // async render pipelines still compiling at the end of the gap
  if (gap.marks.length) c.marks = gap.marks.slice();
  try { if (globalThis.__cutscenes?.active) c.cutscene = true; if (globalThis.__freeRide?.stalled?.()) c.streamStalled = true; } catch {}
  return c;
}
function hookGpuCounters() {
  const D = globalThis.GPUDevice?.prototype; if (!D || D.__ssxDiag) return; D.__ssxDiag = true;
  const wrap = (k, f) => { const g = D[k]; if (typeof g === 'function') D[k] = function (...a) { return f.call(this, g, a); }; };
  const count = (k, field) => wrap(k, function (g, a) { gap[field]++; if (field in total) total[field]++; return g.apply(this, a); });
  wrap('createRenderPipeline', function (g, a) { const t0 = performance.now(); try { return g.apply(this, a); } finally { gap.pipes++; total.pipes++; gap.pipeMs += performance.now() - t0; } });
  wrap('createRenderPipelineAsync', function (g, a) { gap.apipes++; total.apipes++; pendingPipes++; const p = g.apply(this, a); const done = () => { pendingPipes--; }; p.then(done, done); return p; });
  count('createShaderModule', 'shaders'); count('createTexture', 'tex'); count('createBuffer', 'bufs'); count('createBindGroup', 'binds');
}

if (enabled) {
  try { hookGpuCounters(); } catch {}
  // The previous session never said goodbye. cause: 'discarded' (Chrome dropped the background tab: document.wasDiscarded),
  // 'background' (the tab was hidden when last seen: the OS evicted it, or it was closed from the app switcher),
  // 'foreground' (visible when last seen: a crash, an out-of-memory kill, or a browser quit).
  try {
    const last = JSON.parse(localStorage.getItem(MARK) || 'null');
    if (last && !last.clean) {
      const cause = document.wasDiscarded ? 'discarded' : last.frozen ? 'frozen' : last.hidden ? 'background' : 'foreground';
      push('previous-session-died', { cause, previous: { session: last.session, screen: last.screen, course: last.course, up: last.up, hidden: !!last.hidden, hiddenS: last.hiddenAt ? Math.round((last.at - last.hiddenAt) / 1000) : undefined, heapMB: last.heapMB, texMB: last.texMB, wasmMB: last.wasmMB }, last: last.last, agoS: Math.round((Date.now() - last.at) / 1000) });
    }
  } catch {}
  push('hello', { display: { w: window.screen.width, h: window.screen.height, dpr: devicePixelRatio, inner: [innerWidth, innerHeight] }, cores: navigator.hardwareConcurrency, deviceMemory: navigator.deviceMemory ?? null, webgpu: !!navigator.gpu, touch: matchMedia('(pointer:coarse)').matches, url: location.pathname + location.search, discarded: document.wasDiscarded || undefined });
  remember();
  addEventListener('error', (e) => push('error', { message: clip(e.message), where: `${clip(e.filename, 120)}:${e.lineno}:${e.colno}`, stack: clip(e.error?.stack, 1200) }));
  addEventListener('unhandledrejection', (e) => push('rejection', { message: clip(e.reason?.message ?? e.reason), stack: clip(e.reason?.stack, 1200) }));
  for (const level of ['error', 'warn']) {
    const original = console[level].bind(console);
    console[level] = (...args) => {
      const message = clip(args.map((a) => (a instanceof Error ? `${a.message}\n${a.stack}` : typeof a === 'object' ? (() => { try { return JSON.stringify(a); } catch { return String(a); } })() : String(a))).join(' '), 1500);
      push(`console.${level}`, { message });
      const failed = /Render pipeline creation failed \(([^)]+)\)/.exec(message); if (failed) pipelineFailed(failed[1], message);
      original(...args);
    };
  }
  addEventListener('pagehide', (e) => {
    rideTotalM += rideM; rideM = 0; push('pagehide', { persisted: e.persisted || undefined, ...memory(), audio: audioStatsSnapshot() ?? undefined, distTotalM: rideTotalM >= 1 ? Math.round(rideTotalM) : undefined }); // the session's audio totals
    try { const m = JSON.parse(localStorage.getItem(MARK) || '{}'); m.clean = true; localStorage.setItem(MARK, JSON.stringify(m)); } catch {}
    unloading = true; flush(true);
  });
  addEventListener('pageshow', (e) => { if (e.persisted) { unloading = false; push('pageshow', { persisted: true }); remember(); } }); // back from the bfcache
  document.addEventListener('freeze', () => { remember({ frozen: true }); flush(true); }); // Chrome: frozen, possibly discarded next
  document.addEventListener('resume', () => { push('resume'); remember(); });
  document.addEventListener('visibilitychange', () => {
    hiddenAt = document.visibilityState === 'hidden' ? Date.now() : 0;
    push('visibility', { state: document.visibilityState }); remember();
    if (document.visibilityState === 'hidden') flush(true);
  });
  // Screen changes (web/ui.js writes #stage data-screen): the loading -> game timeline.
  const watch = () => { const stage = document.getElementById('stage'); if (!stage) return;
    new MutationObserver(() => { const s = stage.dataset.screen || ''; if (s !== currentScreen) { push('screen', { to: s, ...memory() }); bindPoseArm(s, currentScreen); currentScreen = s; remember(); } }).observe(stage, { attributes: true, attributeFilter: ['data-screen'] }); };
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', watch, { once: true }); else watch();
  // Frame-time heartbeat: median / p95 / max over the last interval, long frames counted, JS heap / GPU / wasm memory.
  // Every 10 s for the first 10 minutes, then every minute (a long session used to hit the event cap after ~30 min).
  // A frame gap over a second while the page stays visible is reported on its own ('stall': when and on which screen),
  // e.g. Firefox holding frames behind pipeline builds (docs/firefox-load.md); the heartbeat only has the interval's max.
  // Each stall (and each 'hitch', a 250 ms - 1 s gap) says what happened in it (stallCause): GPU objects created (render
  // pipelines made synchronously and the main-thread ms in those calls, async ones started and still compiling, shader modules,
  // textures, buffers, bind groups), three.js node material builds, performance marks, a cutscene / free-ride stream stall,
  // busyMs: the main thread's time from the frame's first animation callback to its first task after the rendering update
  // (script + rendering, WebKit's WebGPU present wait included) of the frame before the gap, and longTaskMs (Chrome): long
  // tasks in the gap (loading work, message handlers); the rest is the browser or a GPU / compositor backlog.
  let stalls = 0, hitches = 0, shownAt = 0, lastBeat = 0, lastAudio = '', busyMs = 0, tickAt = 0;
  document.addEventListener('visibilitychange', () => { shownAt = performance.now(); });
  const longTasks = []; try { new PerformanceObserver((l) => { for (const e of l.getEntries()) { longTasks.push([e.startTime, e.duration]); if (longTasks.length > 16) longTasks.shift(); } }).observe({ type: 'longtask' }); } catch {}
  const afterFrame = new MessageChannel(); afterFrame.port1.onmessage = () => { busyMs = performance.now() - tickAt; };
  const tick = (t) => {
    if (lastFrame) {
      const gap = t - lastFrame; frames.push(gap);
      if (gap > 250 && document.visibilityState === 'visible' && shownAt < lastFrame) {
        const stall = gap > 1000;
        if (stall ? stalls < 20 : hitches < 30) {
          if (stall) stalls++; else hitches++;
          const cause = stallCause(busyMs); let lt = 0; for (const [a, d] of longTasks) lt += Math.max(0, Math.min(a + d, t) - Math.max(a, lastFrame)); if (lt) cause.longTaskMs = Math.round(lt);
          push(stall ? 'stall' : 'hitch', { ms: Math.round(gap), ...memory(), cause });
        }
      }
    }
    resetGap(); lastFrame = t; tickAt = performance.now(); busyMs = 0; afterFrame.port2.postMessage(0); requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
  // The race warm-up marks (main.js warmupRender: warm:start / slices / gpu / end), relative to the page start; every mark's
  // name also goes to the stall attribution (the marks made during a gap).
  try { new PerformanceObserver((list) => { for (const e of list.getEntries()) { gapMark(e.name); if (/^warm:/.test(e.name)) push('mark', { name: e.name, at: Math.round(e.startTime) }); } }).observe({ type: 'mark', buffered: true }); } catch {}
  setInterval(() => {
    const t = Date.now(), every = t - started < 600000 ? 10000 : 60000;
    if (t - lastBeat >= every - 500) {
      lastBeat = t;
      // audio field counters (web/audio-stats.js: late / missed music bars, stolen / dropped voices, slow decodes...), when they changed
      const a = audioStatsSnapshot(), aKey = a ? JSON.stringify(a) : ''; const audio = aKey !== lastAudio ? a : undefined; lastAudio = aKey;
      if (frames.length) { const f = frames.sort((a, b) => a - b), q = (p) => Math.round(f[Math.min(f.length - 1, Math.floor(f.length * p))]); const distM = rideM >= 1 ? Math.round(rideM) : undefined; rideTotalM += rideM; rideM = 0; push('frames', { n: f.length, p50: q(0.5), p95: q(0.95), max: Math.round(f[f.length - 1]), over50: f.filter((x) => x > 50).length, ...memory(), dropped: dropped || undefined, audio, distM }); frames = []; }
      else push('memory', { ...memory(), audio }); // hidden / no frames: memory only
    }
    remember();
  }, 10000);
  setInterval(() => flush(), FLUSH_MS);
  // Gamepads (web/gamepad.js): which pads players use and how they are read — the model name / vendor:product, the
  // browser's mapping, button / axis counts, the layout applied (standard, a known one, generic, +remap), the slot and
  // whether it can vibrate. Sent a second after a pad is plugged in / out / remapped / made active; at most 8 times.
  let padReports = 0, padTimer = 0;
  onPads((type) => {
    if (type === 'use' || padReports >= 8) return;
    clearTimeout(padTimer);
    padTimer = setTimeout(() => { padReports++; push('gamepads', { why: type, active: activeEntry()?.slot ?? null, pads: connectedPads().map(padSummary) }); }, 1000);
  });
}

// ---- bind-pose probe (pv bindPoseProbe; docs/ctm-flow.md "T-poses on the first load") ----
// A rider drawn in its bind pose (T-pose) was reported at the start of Conquer the Mountain on a first load and not reproduced in
// the lab. For 60 s after a load screen opens or closes, a cutscene starts or a ride starts (world load, CTM start), a few of the
// skinned meshes three draws each frame (SkinnedMesh.onBeforeRender) are checked: the CPU skeleton (bone.matrixWorld x
// boneInverse the same rigid matrix for every bone = the bind pose), and at the next frame the bone matrices that went to the
// GPU for that draw (skeleton.boneMatrices after its update: a stale buffer shows the bind pose while the bones are posed). A
// mesh the rider's core palette skins (source skin enabled) is not a three.js skeleton pose and is skipped. The first hit of
// the session is reported once ('bind-pose': screen, the mesh path and model, frames / ms since armed, backend, adapter).
// Presentation-only: reads, never writes; outside the windows the draw hook returns at once.
const BIND_EPS = 2e-3, BIND_WINDOW_MS = 60000, BIND_PER_FRAME = 3;
const bindPose = { armedUntil: 0, armedAt: 0, armedBy: '', armedFrame: 0, frame: 0, sampled: 0, checked: 0, reported: false, installed: false, looping: false, hit: null, gpuQueue: [] };
globalThis.ssxBindPose = bindPose; // QA / the lab probe (read-only state)
// Test hook (web/test-diagnostics-bindpose.mjs): the CPU / GPU-buffer spreads of a skinned mesh (0 = the bind pose).
export function bindPoseSpreads(mesh) { bindMatrix ??= new mesh.matrixWorld.constructor(); return { cpu: cpuSpread(mesh), gpu: gpuSpread(mesh) }; }
let bindMatrix = null;
function bindPoseArm(to, from) {
  if (!pv('bindPoseProbe') || bindPose.reported) return;
  const why = from === 'loading' ? 'load-closed' : to === 'loading' ? 'load-open' : to === 'cutscene' ? 'cutscene' : to === 'game' ? 'ride' : to === 'transition' ? 'transition' : '';
  if (!why) return;
  bindPose.armedAt = performance.now(); bindPose.armedUntil = bindPose.armedAt + BIND_WINDOW_MS; bindPose.armedBy = why; bindPose.armedFrame = bindPose.frame;
  if (!bindPose.installed) { bindPose.installed = true; import('three/webgpu').then(bindPoseInstall).catch(() => {}); }
  else bindPoseLoop();
}
// rigid spread of bone i's skin matrix against bone 0's: 0 in the bind pose (element-wise, rotation / scale rows and translation)
const bindRef = new Float64Array(16);
function bindSpread(get, count) {
  let spread = 0; const r = get(0); if (!r) return Infinity; const r0 = bindRef; for (let k = 0; k < 16; k++) r0[k] = r[k];
  for (let i = 1; i < count; i++) { const e = get(i); if (!e) continue; for (let k = 0; k < 15; k++) { if (k % 4 === 3) continue; const d = Math.abs(e[k] - r0[k]); if (d > spread) spread = d; } }
  return spread;
}
function cpuSpread(mesh) {
  const sk = mesh.skeleton, n = Math.min(sk.bones.length, sk.boneInverses.length); if (n < 6) return Infinity;
  return bindSpread((i) => (sk.bones[i] ? bindMatrix.multiplyMatrices(sk.bones[i].matrixWorld, sk.boneInverses[i]).elements : null), n);
}
function gpuSpread(mesh) {
  const sk = mesh.skeleton, a = sk?.boneMatrices, n = Math.min(sk?.bones?.length ?? 0, (a?.length ?? 0) / 16); if (n < 6) return Infinity;
  return bindSpread((i) => a.subarray(i * 16, i * 16 + 16), n);
}
function paletteSkinned(mesh, material) {
  if (!material?.vertexNode) return false;
  for (let o = mesh; o; o = o.parent) { const s = o.userData?.sourceSkin; if (s) return s.enabledNode?.value === true; }
  return false;
}
function bindPoseName(mesh) {
  const path = []; let model = '';
  for (let o = mesh, n = 0; o && n < 7; o = o.parent, n++) { path.push(o.name || o.type); model ||= o.userData?.previewRoot || o.userData?.riderPackage || ''; }
  return { path: path.join('<'), model: clip(model, 80) };
}
function bindPoseReport(mesh, kind, spread, camera) {
  if (bindPose.reported) return;
  bindPose.reported = true; bindPose.armedUntil = 0;
  let inView = null; try { const p = mesh.getWorldPosition(mesh.position.clone()).project(camera); inView = p.z > -1 && p.z < 1 && Math.abs(p.x) < 1.1 && Math.abs(p.y) < 1.1; } catch {}
  const r = diagRenderer, b = r?.backend, adapter = b?.device?.adapterInfo ?? b?.adapter?.info;
  const gl = !b?.device && b?.gl ? clip(b.gl.getParameter(b.gl.RENDERER), 120) : undefined;
  bindPose.hit = { kind, ...bindPoseName(mesh), spread: +spread.toExponential(2), bones: mesh.skeleton?.bones?.length, inView, armedBy: bindPose.armedBy,
    frames: bindPose.frame - bindPose.armedFrame, ms: Math.round(performance.now() - bindPose.armedAt), visible: mesh.visible,
    backend: b?.isWebGPUBackend ? 'webgpu' : b ? 'webgl' : null, adapter: adapter ? `${adapter.vendor ?? ''} ${adapter.architecture ?? ''} ${adapter.device ?? ''} ${adapter.description ?? ''}`.trim() : gl };
  push('bind-pose', bindPose.hit); flush();
}
function bindPoseInstall(THREE) {
  const proto = THREE?.SkinnedMesh?.prototype; if (!proto || proto.__ssxBindPose) return;
  bindMatrix = new THREE.Matrix4(); proto.__ssxBindPose = true;
  const base = proto.onBeforeRender;
  proto.onBeforeRender = function (renderer, scene, camera, geometry, material, group) {
    if (bindPose.armedUntil) {
      try {
        if (performance.now() > bindPose.armedUntil) bindPose.armedUntil = 0;
        else if (renderer._handleObjectFunction !== renderer._createObjectPipeline && bindPose.sampled < BIND_PER_FRAME && this.skeleton && !paletteSkinned(this, material)) {
          bindPose.sampled++; bindPose.checked++;
          const s = cpuSpread(this);
          if (s < BIND_EPS) bindPoseReport(this, 'skeleton', s, camera);
          else if (bindPose.gpuQueue.length < BIND_PER_FRAME) bindPose.gpuQueue.push([this, camera]);
        }
      } catch {}
    }
    return base.call(this, renderer, scene, camera, geometry, material, group);
  };
  bindPoseLoop();
}
// per animation frame while armed: the frame count and the per-frame budget, then the GPU side of last frame's sampled draws (the
// bone matrices uploaded for them: posed bones with a bind-pose buffer = a stale upload)
function bindPoseLoop() {
  if (bindPose.looping) return; bindPose.looping = true;
  const frame = () => {
    bindPose.frame++; bindPose.sampled = 0;
    for (const [mesh, camera] of bindPose.gpuQueue) { try { const s = gpuSpread(mesh); if (s < BIND_EPS) bindPoseReport(mesh, 'gpu-buffer', s, camera); } catch {} }
    bindPose.gpuQueue.length = 0;
    if (bindPose.armedUntil && performance.now() > bindPose.armedUntil) bindPose.armedUntil = 0;
    if (bindPose.armedUntil) requestAnimationFrame(frame); else bindPose.looping = false;
  };
  requestAnimationFrame(frame);
}

// ---- the renderer (main.js after renderer.init(), and again after a device recovery, web/gpu-recovery.js) ----
// Backend, adapter limits that matter on phones, uncaptured errors, device loss, a GPU backlog probe, and per pipeline
// the owning material / object and the WGSL private-memory estimate (web/wgsl-budget.js): a shader over WebKit's 8 KB
// limit is reported from every browser ('shader-over-budget'), not only from the iPhones where it fails.
const owners = new Map(), programSizes = new WeakMap(), overBudget = new Set(), devices = new WeakSet();
function programMemory(program) {
  if (!program?.code) return null;
  let m = programSizes.get(program); if (!m) { try { m = wgslMemory(program.code); m.chars = program.code.length; } catch { m = { privateBytes: -1, functionBytes: -1 }; } programSizes.set(program, m); }
  return m;
}
function ownerOf(renderObject) {
  const { object, material, geometry, pipeline } = renderObject, name = (o) => o ? (o.name || o.type || '') : '';
  const vs = programMemory(pipeline?.vertexProgram), fs = programMemory(pipeline?.fragmentProgram);
  return { material: `${material?.name || material?.type}#${material?.id}`, object: name(object), parent: name(object?.parent), geometry: geometry?.name || undefined,
    vsPrivate: vs?.privateBytes, fsPrivate: fs?.privateBytes, vsChars: vs?.chars, fsChars: fs?.chars, fsFunction: fs?.functionBytes };
}
function pipelineFailed(label, message) { push('pipeline-failed', { label, ...(owners.get(label) ?? {}), message: clip(message, 300) }); }
function watchPipelines(renderer) {
  const utils = renderer.backend?.pipelineUtils; if (!utils?.createRenderPipeline || utils.__ssxDiag) return;
  const create = utils.createRenderPipeline.bind(utils); utils.__ssxDiag = true;
  utils.createRenderPipeline = (renderObject, promises) => {
    try {
      const m = renderObject.material, label = `renderPipeline_${m?.name || m?.type}_${m?.id}`, owner = ownerOf(renderObject);
      owners.set(label, owner); if (owners.size > 600) owners.delete(owners.keys().next().value);
      const worst = Math.max(owner.vsPrivate ?? 0, owner.fsPrivate ?? 0), fn = owner.fsFunction ?? 0;
      if ((worst > WGSL_PRIVATE_LIMIT || fn > WGSL_FUNCTION_LIMIT) && !overBudget.has(label)) { overBudget.add(label); push('shader-over-budget', { label, ...owner }); }
    } catch {}
    return create(renderObject, promises);
  };
}
export function diagnoseRenderer(renderer, { recovered = false } = {}) {
  if (!enabled || !renderer) return;
  diagRenderer = renderer;
  const backend = renderer.backend, device = backend?.device;
  const info = { backend: backend?.isWebGPUBackend ? 'webgpu' : 'webgl', drawingBuffer: [renderer.domElement?.width, renderer.domElement?.height], recovered: recovered || undefined };
  watchPipelines(renderer);
  if (device && !devices.has(device)) {
    devices.add(device);
    const l = device.limits, pick = ['maxTextureDimension2D', 'maxBufferSize', 'maxStorageBufferBindingSize', 'maxUniformBufferBindingSize', 'maxSampledTexturesPerShaderStage', 'maxSamplersPerShaderStage', 'maxStorageBuffersPerShaderStage', 'maxUniformBuffersPerShaderStage', 'maxBindGroups', 'maxVertexBuffers', 'maxVertexAttributes', 'maxInterStageShaderVariables', 'maxColorAttachmentBytesPerSample', 'maxComputeWorkgroupStorageSize'];
    if (!recovered) { info.limits = Object.fromEntries(pick.map((k) => [k, l[k]])); info.features = [...device.features].sort(); }
    const adapter = device.adapterInfo ?? backend.adapter?.info; if (adapter) info.adapter = { vendor: adapter.vendor, architecture: adapter.architecture, device: adapter.device, description: adapter.description };
    device.addEventListener?.('uncapturederror', (e) => push('gpu-error', { type: e.error?.constructor?.name, message: clip(e.error?.message, 1500) }));
    device.lost?.then((l) => { push('gpu-device-lost', { reason: l.reason, message: clip(l.message, 800), hidden: document.visibilityState === 'hidden', ...memory() }); remember(); flush(true); });
  } else if (!device) {
    const gl = backend?.gl; if (gl) info.gl = { renderer: clip(gl.getParameter(gl.RENDERER), 120), maxTexture: gl.getParameter(gl.MAX_TEXTURE_SIZE) };
  }
  push('renderer', info);
  if (renderer.__ssxDiagProbe) return; renderer.__ssxDiagProbe = true;
  try { const cache = renderer._nodes?.nodeBuilderCache; if (cache && typeof cache.set === 'function' && !cache.__ssxDiag) { const set = cache.set.bind(cache); cache.set = (k, v) => { gap.builds++; total.builds++; return set(k, v); }; cache.__ssxDiag = true; } } catch {} // node material builds (stall attribution)
  // GPU backlog: the frame loop can run while the GPU is seconds behind (pipeline builds queue up in Firefox's GPU
  // process), which shows as a frozen or black canvas. Probe the queue every 2 s and report a completion over a second.
  let probing = false, gpuStalls = 0, probeBase = null;
  setInterval(() => {
    const d = renderer.backend?.device;
    if (!d || renderer._isDeviceLost || probing || gpuStalls >= 20 || document.visibilityState !== 'visible') return;
    probing = true; const t0 = performance.now(), before = { ...total };
    // cause: what the page created from 2 s before the probe to its completion (pipelines are built in order on the GPU timeline in
    // Firefox; WebKit and Chrome compile async ones aside), and the async compiles still pending then
    const since = { ...(probeBase || total) }; probeBase = before;
    d.queue.onSubmittedWorkDone().then(() => { const ms = performance.now() - t0; if (ms > 1000) { gpuStalls++; const cause = {}; for (const k in total) if (total[k] - since[k]) cause[k] = total[k] - since[k]; if (pendingPipes) cause.compiling = pendingPipes; push('gpu-stall', { ms: Math.round(ms), cause }); } }, () => {}).finally(() => { probing = false; });
  }, 2000);
}
export function diagnose(kind, data) { push(kind, data); }
