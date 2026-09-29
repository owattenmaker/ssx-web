// Deploy check and service-worker cleanup (docs/workers.md). Installed by web/worker-guard.js (imported at startup).
// - The page knows its build (web/build-id.js); the host serves the deployed build's id at /build.json (never cached:
//   fetched with cache: 'no-store'). A tab kept open (or restored from the back/forward cache) across a deploy runs the
//   old code; its workers still come from its own hashed files (or run on the main thread when those are gone), so it
//   keeps working, and it reloads into the new build the next time it sits on the title screen (visible, nothing in
//   progress there). Checked on a back/forward restore, when the page is shown after 5+ minutes hidden, on 'online',
//   every 30 minutes, and when a worker reports another build. Hosted pages only (https), or ?buildcheck=1.
// - The game never registers a service worker. Any registration on this origin (another app on a shared dev origin, a
//   leftover) would serve stale files: unregistered, its Cache Storage cleared, and a page it controlled reloads once.
import { BUILD_ID } from './build-id.js';
import { diagnose } from './diagnostics.js';

const search = globalThis.location?.search ?? '';
const inPage = typeof window !== 'undefined' && typeof document !== 'undefined';
const enabled = inPage && !/[?&]buildcheck=0\b/.test(search) && BUILD_ID !== 'unbuilt' && (location.protocol === 'https:' || /[?&]buildcheck=1\b/.test(search));
const SAFE_SCREENS = new Set(['title']);
const RELOADED = 'ssx3.build.reloaded';
let serverBuild = null, stale = false, lastCheck = 0, inFlight = null, hiddenAt = 0;
const store = (k, v) => { try { if (v === undefined) return sessionStorage.getItem(k); sessionStorage.setItem(k, v); } catch { return null; } };

export const buildState = () => ({ page: BUILD_ID, server: serverBuild, stale, enabled });
export function checkBuild(reason = 'manual', { force = false } = {}) {
  if (!enabled) return Promise.resolve(buildState());
  if (inFlight) return inFlight;
  if (!force && reason !== 'worker-mismatch' && performance.now() - lastCheck < 60000) return Promise.resolve(buildState());
  lastCheck = performance.now();
  inFlight = (async () => {
    try {
      const r = await fetch(`/build.json?t=${Date.now().toString(36)}`, { cache: 'no-store', credentials: 'same-origin' });
      if (!r.ok) return buildState(); // an older host without it, or the gate's session expired
      const id = (await r.json())?.id;
      if (typeof id !== 'string' || !id) return buildState();
      serverBuild = id;
      if (id !== BUILD_ID && !stale) { stale = true; diagnose('build-stale', { page: BUILD_ID, server: id, reason }); document.documentElement.dataset.staleBuild = id; }
      maybeReload();
    } catch {} finally { inFlight = null; }
    return buildState();
  })();
  return inFlight;
}
function maybeReload() {
  if (!stale || document.visibilityState !== 'visible') return;
  const screen = document.getElementById('stage')?.dataset.screen;
  if (!SAFE_SCREENS.has(screen) || store(RELOADED) === serverBuild) return; // once per new build (no reload loop)
  store(RELOADED, serverBuild);
  diagnose('build-reload', { page: BUILD_ID, server: serverBuild, screen });
  location.reload();
}

async function cleanServiceWorkers() {
  let sw = null; try { sw = navigator.serviceWorker; } catch {} // SecurityError in some private modes
  if (!sw?.getRegistrations || /[?&]sw=keep\b/.test(search)) return;
  try {
    const registrations = await sw.getRegistrations(), controlled = !!sw.controller;
    if (!registrations.length && !controlled) return;
    diagnose('service-worker-removed', { count: registrations.length, scopes: registrations.slice(0, 4).map((r) => r.scope), controlled });
    await Promise.all(registrations.map((r) => r.unregister().catch(() => false)));
    if (globalThis.caches?.keys) for (const key of await caches.keys()) await caches.delete(key).catch(() => false);
    if (controlled && store('ssx3.sw.reloaded') !== '1') { store('ssx3.sw.reloaded', '1'); location.reload(); }
  } catch {}
}

if (inPage) {
  setTimeout(cleanServiceWorkers, 3000);
  if (enabled) {
    addEventListener('pageshow', (e) => { if (e.persisted) checkBuild('pageshow', { force: true }); });
    addEventListener('online', () => checkBuild('online'));
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'hidden') { hiddenAt = Date.now(); return; }
      if (hiddenAt && Date.now() - hiddenAt >= 5 * 60000) checkBuild('visible', { force: true }); else maybeReload();
      hiddenAt = 0;
    });
    setInterval(() => { if (document.visibilityState === 'visible') checkBuild('interval', { force: true }); }, 30 * 60000);
    const watch = () => { const stage = document.getElementById('stage'); if (stage) new MutationObserver(maybeReload).observe(stage, { attributes: true, attributeFilter: ['data-screen'] }); };
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', watch, { once: true }); else watch();
    setTimeout(() => checkBuild('start', { force: true }), 20000); // a page served from an old HTTP / bfcache copy at start
  }
}
globalThis.ssxBuild = { state: buildState, check: (reason = 'qa') => checkBuild(reason, { force: true }) }; // QA
