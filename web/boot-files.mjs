// The files the title screen waits for on a first visit (docs/first-load.md): web/boot-files.json, read by
// web/boot-plugin.js, which inlines their sizes into index.html so the title's load meter knows its totals up front.
//   node boot-files.mjs record     headless Chrome + a private Vite server: load the site, list every /assets file
//                                  requested before the title is ready, write boot-files.json
//   node boot-files.mjs check      every listed file exists in public/assets (when the game data is there)
// Refresh it when the boot path loads different files (the title's meter then counts what it really waits for;
// files missing from the list still load, the meter just does not know them in advance).
import fs from 'node:fs'; import path from 'node:path';
const web = path.dirname(new URL(import.meta.url).pathname), OUT = path.join(web, 'boot-files.json');

export const BOOT_PHASES = [
  ['code', 'Loading game...'], ['core', 'Loading game engine...'], ['fe', 'Loading menus...'], ['gpu', 'Starting graphics...'],
  ['course', 'Loading %s...'], ['riders', 'Loading riders...'], ['final', 'Starting...'],
];
// Compute steps: [id, typical ms (Chrome, M-series Mac, measured 2026-09-26), phase]; see boot-progress.js BYTES_PER_MS.
export const BOOT_STEPS = [['js', 250, 'code'], ['core', 400, 'core'], ['gpu', 250, 'gpu'], ['world', 2500, 'course'], ['ai', 1100, 'riders'], ['final', 500, 'final']];

export function bootPhase(p) {
  if (/^\/assets\/[^/]+-[A-Za-z0-9_-]{8}\.wasm$/.test(p)) return 'core';
  if (/^\/assets\/[^/]+-[A-Za-z0-9_-]{8}\.(js|css)$/.test(p)) return 'code';
  if (/^\/assets\/UI\/rival-(exclaim|beam)\.png$/.test(p)) return 'riders';   // the computer riders' icons (web/rival-beam.js): loaded with the course
  if (/^\/assets\/(UI|CAREER|LOADING|AUDIO|MOVIES)\//.test(p) || /^\/assets\/(riders|courses)\.json$/.test(p)) return 'fe';
  if (/^\/assets\/(RIDER_[A-Z_]+|WARDROBE)\//.test(p)) return 'riders';
  return 'course';
}

export function bootList(paths, course = 'ARA1') {
  const seen = new Set(), files = {};
  for (const p of paths) {
    if (!p.startsWith('/assets/') || seen.has(p) || /^\/assets\/[^/]+-[A-Za-z0-9_-]{8}\.(js|css|wasm|webmanifest|png)$/.test(p)) continue;   // hashed build files: from the bundle
    seen.add(p); (files[bootPhase(p)] ??= []).push(p);
  }
  return { course, phases: BOOT_PHASES, steps: BOOT_STEPS, files };
}

if (process.argv[1] === new URL(import.meta.url).pathname) {
  const cmd = process.argv[2];
  if (cmd === 'check') {
    const list = JSON.parse(fs.readFileSync(OUT, 'utf8')), pub = path.join(web, 'public');
    const missing = Object.values(list.files).flat().filter((p) => !fs.existsSync(path.join(pub, p)));
    console.log(missing.length ? `missing: ${missing.join(' ')}` : 'all boot files present'); process.exit(missing.length ? 1 : 0);
  }
  if (cmd === 'record') {
    const { startBrowser, startServer } = await import('./headless-chrome.mjs');
    const server = await startServer(), browser = await startBrowser({ init: 'performance.setResourceTimingBufferSize(5000)' });
    if (!browser) { console.error('no Chrome'); process.exit(1); }
    try {
      await browser.goto(server.origin + '/?perf=1&diag=0&pv=-lazyCourse');   // the whole first load (with pv lazyCourse the title waits for the front end only)
      await browser.waitFor('window.__perfUI && window.__perfUI().ready', 180000);
      const paths = await browser.evaluate(`performance.getEntriesByType('resource').filter((e) => e.startTime < performance.now()).map((e) => new URL(e.name).pathname)`);
      const list = bootList(paths.filter((p) => !p.startsWith('/assets/') || fs.existsSync(path.join(web, 'public', p))));   // not the optional files a course lacks (404)
      fs.writeFileSync(OUT, JSON.stringify(list, null, 1) + '\n');
      console.log(`boot-files.json: ${Object.entries(list.files).map(([k, v]) => `${k} ${v.length}`).join(', ')}`);
    } finally { await browser.close(); await server.close(); }
  }
}
