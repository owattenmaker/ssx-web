// Vite plugin (vite.config.js): the title's first paint and load meter inlined into index.html (docs/first-load.md).
// - web/boot-screen.js (+ boot-progress.js, title-screen.js, lui-player.js) bundled into one classic script (rolldown,
//   IIFE, minified) at the end of <body>, so it runs while the page parses, before the 2 MB game bundle has arrived;
//   lui-player's input-glyph and loading-screen imports are cut to stubs (no key caps on the title; no download hook).
// - window.__SSX_BOOT__: the title screens (FE.LUI 06title, the FE snow loop, the Select Character meter), FEFONT
//   metrics, course names, and the boot manifest: web/boot-files.json with each file's size and its size on the wire
//   (the gzip copy the host serves, web/server/precompress.mjs rules), plus the built code bundle and core.
// - <link rel=preload> for the title's pictures and FEFONT, high priority.
// The game data comes from web/public/assets at build time (git-ignored; nothing of it is in the repo). Without it
// (a checkout without the data) the plugin injects nothing and the page starts as before.
import fs from 'node:fs'; import path from 'node:path'; import zlib from 'node:zlib';
import { titleScreenData, TITLE_PAGES } from './title-data.js';

const web = path.dirname(new URL(import.meta.url).pathname), pub = path.join(web, 'public'), UI = path.join(pub, 'assets/UI');
const SKIP = new Set(['.png', '.jpg', '.jpeg', '.mp4', '.mus', '.gz', '.br', '.woff2']), MIN_GZ = 16 * 1024, MAX_RATIO = 0.9;   // = server/precompress.mjs
const CACHE = path.join(web, 'node_modules/.cache/ssx-boot/wire.json');
const STUBS = {
  'input-glyphs.js':
    'export const LUI_STRETCH=(640/512)/(480/448);export function inputDevice(){return "gamepad"}export function drawGlyphAsKey(){return false}export function glyphButton(){return false}export function drawKeyCap(){}',
  'downloads.js': 'export function downloadProgress(){return {active:false,fraction:1,expected:0,received:0,files:0,busyMs:0}}'
};

// Bytes on the wire of a file of the game data: its gzip copy when the host keeps one, else the file.
let wireCache = null;
export function wireSize(file, rel) {
  let st; try { st = fs.statSync(file); } catch { return null; }
  const ext = path.extname(file).toLowerCase();
  if (SKIP.has(ext) || st.size < MIN_GZ) return { size: st.size, wire: st.size };
  wireCache ??= (() => { try { return JSON.parse(fs.readFileSync(CACHE, 'utf8')); } catch { return {}; } })();
  const key = `${rel}|${st.size}|${Math.floor(st.mtimeMs)}`;
  if (wireCache[key] == null) { const gz = zlib.gzipSync(fs.readFileSync(file), { level: 6 }).length; wireCache[key] = gz <= st.size * MAX_RATIO ? gz : st.size; wireCache.dirty = true; }
  return { size: st.size, wire: wireCache[key] };
}
function saveWireCache() {
  if (!wireCache?.dirty) return;
  delete wireCache.dirty;
  try {
    fs.mkdirSync(path.dirname(CACHE), { recursive: true });
    fs.writeFileSync(CACHE, JSON.stringify(wireCache));
  } catch {}
}
const readJson = (f) => { try { return JSON.parse(fs.readFileSync(f, 'utf8')); } catch { return null; } };

// The inline data (without the code bundle entries, which only a build knows).
export function bootData() {
  const list = readJson(path.join(web, 'boot-files.json')), feMenus = readJson(path.join(UI, 'fe-menus.json')), charSelect = readJson(path.join(UI, 'character-select.json'));
  const glyphs = readJson(path.join(UI, 'FEFONT-glyphs.json')), courses = readJson(path.join(pub, 'assets/courses.json'));
  const title = feMenus && charSelect && titleScreenData(feMenus, charSelect);
  if (!list || !title || !glyphs) return null;
  const files = [];
  for (const [phase, paths] of Object.entries(list.files)) for (const p of paths) { const w = wireSize(path.join(pub, p), p); if (w) files.push([p, w.wire, w.size, phase]); }
  saveWireCache();
  const courseNames = Object.fromEntries((courses?.courses || []).filter((c) => c.code && c.name).map((c) => [c.code, c.name]));
  return { course: list.course, courseNames, title, glyphs, manifest: { phases: list.phases, steps: list.steps, files } };
}

let bundled = null;
const stampOf = (files) => files.map((f) => { try { return fs.statSync(f).mtimeMs; } catch { return 0; } }).join();
async function bootScript() {
  if (bundled && stampOf(bundled.files) === bundled.stamp) return bundled.code;   // rebuilt when any bundled module changes
  const { rolldown } = await import('rolldown');
  const bundle = await rolldown({
    input: path.join(web, 'boot-screen.js'),
    logLevel: 'silent',
    plugins: [
      {
        name: 'ssx-boot-stubs',
        resolveId(id) {
          const f = path.basename(id);
          return STUBS[f] && id.startsWith('./') ? `\0boot-stub:${f}` : null;
        },
        load(id) {
          return id.startsWith('\0boot-stub:') ? STUBS[id.slice(11)] : null;
        }
      }
    ]
  });
  const { output } = await bundle.generate({ format: 'iife', minify: true });
  await bundle.close();
  const files = output[0].moduleIds.filter((id) => !id.startsWith('\0') && fs.existsSync(id));
  bundled = { files, stamp: stampOf(files), code: output[0].code.replace(/<\/script/gi, '<\\/script') };
  return bundled.code;
}

const gz = (buf) => zlib.gzipSync(buf, { level: 6 }).length;
export default function bootPlugin() {
  return {
    name: 'ssx-boot-title',
    transformIndexHtml: {
      order: 'post',
      async handler(html, ctx) {
        if (!/(^|\/)index\.html$/.test(ctx.path || ctx.filename || '') && ctx.path !== '/') return html;   // the game page only (not the GPU test pages)
        const data = bootData();
        if (!data) return html;
        if (ctx.bundle) {   // build: the code bundle and the core are part of the first load
          for (const out of Object.values(ctx.bundle)) {
            const p = '/assets/' + path.basename(out.fileName), src = out.type === 'chunk' ? Buffer.from(out.code) : Buffer.from(out.source);
            // the edge compresses it (else its bytes still stream through downloads.js)
            if (out.type === 'chunk' && out.isEntry) { data.mainScript = p; data.manifest.files.unshift([p, gz(src), src.length, 'code']); }
            else if (/\.css$/.test(out.fileName) || (out.type === 'chunk' && /rolldown-runtime/.test(out.fileName))) data.manifest.files.unshift([p, gz(src), src.length, 'code']);
            else if (/^assets\/core-[^/]+\.wasm$/.test(out.fileName)) data.manifest.files.push([p, gz(src), src.length, 'core']);
          }
        }
        const code = await bootScript();
        const json = JSON.stringify(data).replace(/</g, '\\u003c');
        const preload = [...TITLE_PAGES, 'FEFONT-0'].map((p) => ({ tag: 'link', attrs: { rel: 'preload', as: 'image', href: `/assets/UI/${p}.png`, fetchpriority: 'high' }, injectTo: 'head' }));
        return { html, tags: [...preload, { tag: 'script', children: `window.__SSX_BOOT__=${json};`, injectTo: 'body' }, { tag: 'script', children: code, injectTo: 'body' }] };
      },
    },
  };
}
