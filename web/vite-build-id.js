// Vite plugin (vite.config.js, also in worker.plugins): the build identity the page and its workers compare
// (web/build-id.js, web/worker-guard.js, docs/workers.md).
// - build: one BUILD_ID per `vite build` process, written into web/build-id.js of the page bundle and of every worker
//   bundle, and emitted as dist/build.json (the deploy check of web/build-check.js reads it, never cached).
// - dev server: BUILD_ID = dev-<server start>; WORKER_BUILDS = per worker a hash of its module graph (the worker file
//   and its local imports), recomputed after any file change (the module is invalidated), so a page loaded before a
//   worker's code changed detects the fresh worker.
//   /build.json is served by a middleware.
import fs from 'node:fs'; import path from 'node:path'; import crypto from 'node:crypto';

const web = path.dirname(new URL(import.meta.url).pathname);
const BUILD_FILE = path.join(web, 'build-id.js');
// Worker name (web/worker-guard.js createGuardedWorker / serveWorker) -> entry file.
export const WORKER_ENTRIES = { 'peak-world': 'peak-world-worker.js', terrain: 'terrain-worker.js', 'fe-preview': 'fe-preview-worker.js', 'audio-decode': 'audio-decode-worker.js', 'texture-decode': 'texture-decode-worker.js' };
const stamp = new Date().toISOString().replace(/[-:T]/g, '').slice(0, 14);
export const PROCESS_BUILD_ID = `b${stamp}-${crypto.randomBytes(3).toString('hex')}`;
const DEV_ID = `dev-${Date.now().toString(36)}`;

// Content hash of a worker's module graph (dev). Files are hashed once per (size, mtime).
const fileHashes = new Map();
function fileHash(file) {
  let st; try { st = fs.statSync(file); } catch { return 'missing'; }
  const key = `${st.size}:${st.mtimeMs}`, hit = fileHashes.get(file);
  if (hit?.key === key) return hit.hash;
  const hash = crypto.createHash('sha1').update(fs.readFileSync(file)).digest('hex').slice(0, 12);
  fileHashes.set(file, { key, hash }); return hash;
}
const IMPORT_RE = /(?:^|[;\s}])(?:import|export)\s*(?:[^'";]*?\bfrom\s*)?['"](\.{1,2}\/[^'"?#]+)['"]|import\(\s*['"](\.{1,2}\/[^'"?#]+)['"]\s*\)/g;
export function graphFiles(entry, root = web) {
  const seen = new Set(), todo = [path.join(root, entry)];
  while (todo.length) {
    const file = todo.pop(); if (seen.has(file)) continue; seen.add(file);
    if (!/\.m?js$/.test(file)) continue;
    let code; try { code = fs.readFileSync(file, 'utf8'); } catch { continue; }
    for (const m of code.matchAll(IMPORT_RE)) todo.push(path.resolve(path.dirname(file), m[1] ?? m[2]));
  }
  return [...seen].sort();
}
export function graphHash(entry, root = web) {
  const h = crypto.createHash('sha1');
  for (const f of graphFiles(entry, root)) h.update(path.relative(root, f) + '\0' + fileHash(f) + '\0');
  return 'dev-' + h.digest('hex').slice(0, 12);
}
const workerBuilds = (root = web) => Object.fromEntries(Object.entries(WORKER_ENTRIES).filter(([, f]) => fs.existsSync(path.join(root, f))).map(([name, f]) => [name, graphHash(f, root)]));

// The two marked values of web/build-id.js.
export function fillBuildId(code, buildId, builds) {
  return code.replace("/*@ssx-build-id*/'unbuilt'", JSON.stringify(buildId)).replace('/*@ssx-worker-builds*/null', builds ? JSON.stringify(builds) : 'null');
}

export default function buildIdPlugin({ worker = false } = {}) {
  let command = 'serve';
  return {
    name: 'ssx-build-id',
    configResolved(config) { command = config.command; },
    transform(code, id) {
      if (path.resolve(id.split('?')[0]) !== BUILD_FILE) return null;
      return { code: command === 'build' ? fillBuildId(code, PROCESS_BUILD_ID, null) : fillBuildId(code, DEV_ID, workerBuilds()), map: null };
    },
    configureServer(server) {
      // Any change: the build module is transformed again on its next request (a worker starting later gets the new hashes).
      const invalidate = () => {
        for (const graph of [server.environments?.client?.moduleGraph, server.moduleGraph]) {
          for (const mod of graph?.getModulesByFile?.(BUILD_FILE) ?? []) graph.invalidateModule(mod);
        }
      };
      server.watcher.on('change', invalidate); server.watcher.on('add', invalidate); server.watcher.on('unlink', invalidate);
      server.middlewares.use((req, res, next) => {
        if ((req.url ?? '').split('?')[0] !== '/build.json') return next();
        res.setHeader('content-type', 'application/json'); res.setHeader('cache-control', 'no-store');
        res.end(JSON.stringify({ id: DEV_ID, workers: workerBuilds() }));
      });
    },
    generateBundle() {
      if (worker || command !== 'build') return;
      this.emitFile({ type: 'asset', fileName: 'build.json', source: JSON.stringify({ id: PROCESS_BUILD_ID, built: new Date().toISOString() }) + '\n' });
    },
  };
}
