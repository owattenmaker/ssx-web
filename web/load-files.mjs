// The files each kind of load downloads (pv loadMeter, docs/loading-screen.md "The meter is the load"): web/load-files.json, read by
// main.js at an event / course / world load so the load screen's meter knows its download total up front (bytes as downloads.js
// counts them: the decoded body).
//   node load-files.mjs record    headless Chrome + a private Vite server: an ARA1 Single Event picked after the course is in (the event
//                                 files), a BRA2 Single Event after it (the course files), a new career (the PEAK1 world); writes
//                                 the templates and the sums
//   node load-files.mjs build     the sums again from the recorded templates and public/assets (after an asset export)
//   node load-files.mjs check     exit 1 when the sums no longer match public/assets (web/test-load-files.mjs); skipped without the
//                                 game data
// Templates: a course's code in a path is {course}; a rider package's files are one template ({rider}/...) summed per package. Files
// the manifest does not know still load: the meter adds them as they arrive.
import fs from 'node:fs';
import path from 'node:path';
const web = path.dirname(new URL(import.meta.url).pathname);
const OUT = path.join(web, 'load-files.json');
const PUB = path.join(web, 'public');

const RIDER = /^\/assets\/(RIDER_[A-Z0-9_]+)\/(.+)$/;
// a rider's outfit textures (web/wardrobe.js): WARDROBE/<id>, counted with its package RIDER_<id>
const WARDROBE = /^\/assets\/WARDROBE\/([A-Z0-9_]+)\/(.+)$/;
// streamed audio: the music (.mus) and the speech banks' .dat are read by range as they play (web/audio-speech.js, the music
// streamer), not downloaded whole (a dev server answers a range with the whole file)
const STREAMED = /^\/assets\/AUDIO\/(music\/.+\.mus|speech\/.+\.dat)$/;
// a path with the course's code (its directory, AUDIO/world/<code>.json, the <code>_slotN banks, scdat_<code>) as a template
export function courseTemplate(p, code) {
  return p.split(code).join('{course}');
}
// recorded paths of one load -> { files: fixed paths, templates: course templates, rider: rider-relative paths }
export function classify(paths, code, human) {
  const out = { files: new Set(), templates: new Set(), rider: new Set() };
  for (const p of paths) {
    if (!p.startsWith('/assets/') || /\.(js|css|wasm)$/.test(p) || STREAMED.test(p)) continue;
    const r = RIDER.exec(p);
    if (r) {
      if (r[1] !== human) out.rider.add(r[2]);
      continue;
    }
    const w = WARDROBE.exec(p);
    if (w) {
      if ('RIDER_' + w[1] !== human) out.rider.add('WARDROBE:' + w[2]);
      continue;
    }
    if (p.includes(code)) out.templates.add(courseTemplate(p, code));
    else out.files.add(p);
  }
  return { files: [...out.files].sort(), templates: [...out.templates].sort(), rider: [...out.rider].sort() };
}

const size = (p) => {
  try {
    return fs.statSync(path.join(PUB, p)).size;
  } catch {
    return 0;
  }
};
const dirs = (re) => {
  try {
    return fs.readdirSync(path.join(PUB, 'assets')).filter((d) => re.test(d)).sort();
  } catch {
    return [];
  }
};
// the sums: per course for the course and event kinds, per rider package, the world kind
export function sums(list) {
  const courses = dirs(/^[A-Z]{3}\d$/);
  const sumKind = (kind, code) => kind.files.reduce((a, p) => a + size(p), 0) + kind.templates.reduce((a, t) => a + size(t.split('{course}').join(code)), 0);
  const out = { course: {}, event: {}, rider: {}, world: {} };
  for (const code of courses) {
    out.course[code] = sumKind(list.kinds.course, code);
    out.event[code] = sumKind(list.kinds.event, code);
  }
  const riderFile = (pkg, rel) => (rel.startsWith('WARDROBE:') ? `/assets/WARDROBE/${pkg.slice(6)}/${rel.slice(9)}` : `/assets/${pkg}/${rel}`);
  for (const pkg of dirs(/^RIDER_[A-Z0-9_]+$/)) out.rider[pkg] = list.kinds.rider.reduce((a, rel) => a + size(riderFile(pkg, rel)), 0);
  for (const [world, kind] of Object.entries(list.kinds.world)) out.world[world] = sumKind(kind, world);
  return out;
}

async function record() {
  const { startBrowser, startServer } = await import('./headless-chrome.mjs');
  const server = await startServer();
  const init = 'try{performance.setResourceTimingBufferSize(20000)}catch{}';
  const res = (b, from, to) =>
    b.evaluate(`performance.getEntriesByType('resource').filter((e)=>e.startTime>=${from}&&e.startTime<=${to}).map((e)=>new URL(e.name).pathname)`);
  const now = (b) => b.evaluate('performance.now()');
  const enter = async (b) => {
    for (const type of ['keyDown', 'keyUp']) await b.send('Input.dispatchKeyEvent', { type, code: 'Enter', key: 'Enter', windowsVirtualKeyCode: 13 });
  };
  const screen = `document.getElementById('stage').dataset.screen`;
  const open = async (b) => {
    await b.goto(server.origin + '/?perf=1&diag=0&mute=1&rider=zoe');
    await b.waitFor(`!!performance.getEntriesByName('boot:ready')[0]`, 240000);
    await enter(b);
    await b.waitFor(`performance.getEntriesByName('course:live').length>0`, 600000);
  };
  const event = async (b, code) => {
    const t0 = await now(b);
    await b.evaluate(`(()=>{const ui=__perfUI();ui.startSingleEvent(ui.courses.find((c)=>c.code===${JSON.stringify(code)}));return 1})()`);
    await b.waitFor(`${screen}==='loading'`, 20000);
    await b.waitFor(`${screen}!=='loading'`, 600000);
    const t1 = await now(b);
    // the course build's last milestone (main.js courseMilestone): the course files before it, the event's after
    const live = await b.evaluate(`Math.round(performance.getEntriesByName('course:riders').at(-1)?.startTime ?? 0)`);
    return { t0, t1, live };
  };
  const list = { kinds: { course: null, event: null, rider: [], world: {} } };
  let b = await startBrowser({ init, width: 1280, height: 720 });
  try {
    await open(b);
    const ara = await event(b, 'ARA1');
    const ev = classify(await res(b, ara.t0, ara.t1), 'ARA1', 'RIDER_ZOE');
    const bra = await event(b, 'BRA2');
    const course = classify(await res(b, bra.t0, bra.live), 'BRA2', 'RIDER_ZOE');
    const ev2 = classify(await res(b, bra.live, bra.t1), 'BRA2', 'RIDER_ZOE');
    // a file both loads fetched counts with the course (an event right after a switch would count it twice)
    const inCourse = new Set([...course.files, ...course.templates]);
    list.kinds.event = {
      files: ev.files.filter((p) => !inCourse.has(p)),
      templates: [...new Set([...ev.templates, ...ev2.templates])].filter((p) => !inCourse.has(p)).sort()
    };
    list.kinds.course = { files: course.files, templates: course.templates };
    list.kinds.rider = [...new Set([...ev.rider, ...ev2.rider, ...course.rider])].sort();
  } finally {
    await b.close();
  }
  b = await startBrowser({ init, width: 1280, height: 720 });
  try {
    await b.goto(server.origin + '/?perf=1&diag=0&mute=1&rider=zoe');
    await b.waitFor(`!!performance.getEntriesByName('boot:ready')[0]`, 240000);
    await enter(b);
    await b.waitFor(`${screen}==='main'`, 20000);
    // the page's first course in first (a career started at once abandons it: what it fetched is not the world's)
    await b.waitFor(`performance.getEntriesByName('course:live').length>0`, 600000);
    const t0 = await now(b);
    await b.evaluate(`(()=>{const ui=__perfUI();ui.careerMode=true;ui.onlineMode=false;ui.careerUI.enter();return 1})()`);
    await b.waitFor(`${screen}==='loading'`, 20000);
    await b.waitFor(`${screen}!=='loading'`, 900000);
    // the world's course code (the CTM world: MOUNTAIN), its files templated on it
    const code = await b.evaluate('__perfUI().course.code');
    const w = classify(await res(b, t0, await now(b)), code, 'RIDER_ZOE');
    list.kinds.world[code] = { files: w.files, templates: w.templates };
  } finally {
    await b.close();
    await server.close();
  }
  return list;
}

if (process.argv[1] === new URL(import.meta.url).pathname) {
  const cmd = process.argv[2];
  if (cmd === 'record' || cmd === 'build') {
    const list = cmd === 'record' ? await record() : JSON.parse(fs.readFileSync(OUT, 'utf8'));
    const out = { kinds: list.kinds, sums: sums(list) };
    fs.writeFileSync(OUT, JSON.stringify(out) + '\n');
    const k = out.kinds;
    console.log(`load-files.json: course ${k.course.files.length}+${k.course.templates.length}, event ${k.event.files.length}+${k.event.templates.length}, rider ${k.rider.length}, world ${Object.keys(k.world).join(' ')}; ARA1 course ${(out.sums.course.ARA1 / 1e6).toFixed(1)} MB`);
  } else if (cmd === 'check') {
    if (!fs.existsSync(path.join(PUB, 'assets/ARA1'))) {
      console.log('load files: skipped (no game data)');
      process.exit(0);
    }
    const list = JSON.parse(fs.readFileSync(OUT, 'utf8'));
    const now = sums(list), bad = [];
    for (const kind of Object.keys(now))
      for (const [k, v] of Object.entries(now[kind])) if (list.sums[kind]?.[k] !== v) bad.push(`${kind}.${k} ${list.sums[kind]?.[k]} -> ${v}`);
    if (bad.length) {
      console.error(`load-files.json is stale (run: node load-files.mjs build): ${bad.slice(0, 12).join(', ')}${bad.length > 12 ? ' ...' : ''}`);
      process.exit(1);
    }
    console.log('load files: the sums match public/assets');
  } else console.log('usage: node load-files.mjs record|build|check');
}
