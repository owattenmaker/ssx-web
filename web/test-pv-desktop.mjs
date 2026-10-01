// pv-flags.js PV_DESKTOP (docs/ctm-events-in-world.md "Turning it on"): the in-world CTM switches are on for the desktop tier only (the old
// mountainRide split: not iOS / Android, not the low quality tier); a phone keeps the event-load path. Node (no page) keeps PV_DEFAULTS.
import assert from 'node:assert/strict';
import { PV_DEFAULTS, PV_DESKTOP, desktopTier, pv } from './pv-flags.js';
const store = { getItem: () => null, setItem() {} };
const env = (ua, w, h, search = '', touch = 0, coarse = false) => ({ location: { search }, localStorage: store, navigator: { userAgent: ua, maxTouchPoints: touch },
  screen: { width: w, height: h }, matchMedia: (q) => ({ matches: coarse && q.includes('coarse') }) });
const mac = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Safari/605.1.15';
assert.equal(desktopTier(env(mac, 1920, 1080)), true, 'desktop Safari');
assert.equal(desktopTier(env('Mozilla/5.0 (Windows NT 10.0) AppleWebKit/537.36 Chrome/130 Safari/537.36', 1920, 1080)), true, 'desktop Chrome');
assert.equal(desktopTier(env(mac, 1920, 1080, '?quality=low')), false, 'the low tier');
assert.equal(desktopTier(env('Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15', 390, 844, '', 5, true)), false, 'iPhone');
assert.equal(desktopTier(env(mac, 1024, 1366, '', 5, true)), false, 'iPad (Macintosh UA with touch points)');
assert.equal(desktopTier(env('Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 Chrome/130 Mobile Safari/537.36', 412, 915, '', 5, true)), false, 'Android');
assert.equal(desktopTier({}), false, 'no page');
for (const k of PV_DESKTOP) { assert.equal(PV_DEFAULTS[k], false, k + ': the phone value'); assert.equal(pv(k), false, k + ': node keeps the default'); }
for (const k of ['nisPreload', 'nisBoneProbe', 'transportFade']) assert.ok(!PV_DESKTOP.includes(k) && PV_DEFAULTS[k] === false, k + ' stays off');
console.log('pv desktop split: ok');
