// Build identity of the page and of every worker (docs/workers.md). web/vite-build-id.js fills the two marked values:
// - BUILD_ID: one id per `vite build` (the page, its workers and dist/build.json share it); 'dev-<start>' under the
//   dev server; 'unbuilt' when a module is imported without Vite (node tests).
// - WORKER_BUILDS: dev server only, a content hash of each worker's module graph (its own files and their local
//   imports), recomputed when a file changes. A page that has been open while a worker's code changed then
//   sees a different id from a fresh worker than it expects, which is exactly a stale page / fresh worker pair.
// Both sides import this module: the page when it loads, a worker when it starts (web/worker-guard.js compares them).
export const BUILD_ID = /*@ssx-build-id*/'unbuilt';
export const WORKER_BUILDS = /*@ssx-worker-builds*/null;
export const expectedWorkerBuild = (name) => WORKER_BUILDS?.[name] ?? BUILD_ID;
