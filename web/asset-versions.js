// Versioned game-file URLs (pv versionedAssets, docs/hosting.md "Versioned asset URLs"). No imports: used by the page
// (web/downloads.js), the workers (web/worker-guard-child.js), the boot title and node (web/asset-manifest.js, tests).
// The build writes a manifest of web/public/assets, {v: 1, dirs: {dir: {name: hash}}} (hash = the first 10 hex of the
// file's md5; '' = the top level), served as /assets/asset-manifest-<hash8>.json. A game file /assets/<dir>/<name> is then
// asked for as /assets/<dir>/<name>?v=<hash>: the origin answers immutable only when that is the file it has
// (web/server/mp-server.mjs), so a repeat visit asks nothing for unchanged files. A file the manifest does not name, a URL
// with a query, another origin: unchanged.
export const VERSION_RE = /^[0-9a-f]{10}$/;
export const MANIFEST_FORMAT = 1;

/** The manifest's hash of a path under /assets/ (decoded, no leading slash), or null. */
export function assetVersion(doc, rel) {
  if (!doc || !doc.dirs || typeof rel !== 'string') return null;
  const i = rel.lastIndexOf('/'), files = doc.dirs[i < 0 ? '' : rel.slice(0, i)];
  const v = files && Object.prototype.hasOwnProperty.call(files, rel.slice(i + 1)) ? files[rel.slice(i + 1)] : null;
  return typeof v === 'string' && VERSION_RE.test(v) ? v : null;
}

// The path under /assets/ of a same-origin game-file URL without a query, else null.
function assetPath(href, base) {
  let url, origin;
  try { url = new URL(href, base); origin = new URL(base).origin; } catch { return null; }
  if (url.origin !== origin || url.search || !url.pathname.startsWith('/assets/')) return null;
  try { return { url, rel: decodeURIComponent(url.pathname.slice('/assets/'.length)) }; } catch { return null; }
}

/** href with ?v=<hash> when the manifest names the file (an absolute URL), else null. */
export function versionedHref(href, doc, base) {
  if (!doc) return null;
  const a = assetPath(href, base); if (!a) return null;
  const v = assetVersion(doc, a.rel); if (!v) return null;
  a.url.search = `?v=${v}`;
  return a.url.href;
}

/** The part of the manifest a worker request needs (web/worker-guard.js): every directory at or under the directory of each
 * game-file URL in `hrefs` (a root such as /assets/RIDER_ZOE/ takes its whole tree, a file its directory). */
export function versionSubset(doc, hrefs, base) {
  if (!doc?.dirs) return null;
  const prefixes = new Set();
  for (const h of hrefs) {
    const a = typeof h === 'string' ? assetPath(h, base) : null; if (!a) continue;
    const i = a.rel.lastIndexOf('/'); prefixes.add(i < 0 ? '' : a.rel.slice(0, i));
  }
  if (!prefixes.size) return null;
  const dirs = {};
  for (const p of prefixes) for (const d of Object.keys(doc.dirs)) if (p === '' ? d === '' : d === p || d.startsWith(p + '/')) dirs[d] = doc.dirs[d];
  return Object.keys(dirs).length ? { v: MANIFEST_FORMAT, dirs } : null;
}

/** The game-file URLs in a worker request: its string fields and those of its `urls` object (web/peak-world-prepare.js root /
 * env / urls, web/fe-preview-prepare.js root; an env path's .json / .bin are in its directory). */
export function requestAssetUrls(data) {
  const out = [];
  if (!data || typeof data !== 'object') return out;
  const add = (s) => { if (typeof s === 'string' && s.includes('/assets/')) out.push(s); };
  for (const v of Object.values(data)) add(v);
  if (data.urls && typeof data.urls === 'object') for (const v of Object.values(data.urls)) add(v);
  return out;
}
