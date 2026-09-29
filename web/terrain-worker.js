// Terrain detail meshes off the main thread (web/terrain-refinement.js, its own message protocol: raw mode of
// web/worker-guard.js, which checks the build and runs the same handler on the main thread when the worker cannot start).
import {serveWorker} from './worker-guard-child.js';
import {createTerrainWorkerHandler} from './terrain-worker-core.js';
serveWorker('terrain',send=>createTerrainWorkerHandler(send),{raw:true});
