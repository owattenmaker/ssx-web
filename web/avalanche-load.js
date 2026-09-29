// The core's avalanche playback data (web/avalanche_gameplay.inc, docs/avalanche.md): one tools/export_avalanches.py document per
// location with recorded avalanches, <location root>/avalanches.json. The first document found replaces the definitions, the next
// ones are added in front (a later location's first, as 0x2D92D0 inserts at the head); none found clears them.
export const AVALANCHE_LOCATIONS = new Set(['ABC1', 'DBC2', 'DRA4', 'ERA5', 'ESS3', 'EBA3', 'EBC3']);
export async function loadAvalanches(core, locations, fetchText = (url) => fetch(url).then((r) => (r.ok ? r.text() : null)).catch(() => null)) {
  if (!core._init_avalanches) return 0;
  let n = 0, first = true;
  for (const { code, root } of locations) {
    if (!AVALANCHE_LOCATIONS.has(code)) continue;
    const text = await fetchText(root + 'avalanches.json'); if (!text) continue;
    const b = new TextEncoder().encode(text + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p);
    try { n += core._init_avalanches(p, first ? 0 : 1); first = false; } finally { core._free(p); }
  }
  if (first) core._avalanche_clear?.();
  return n;
}
