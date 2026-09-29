// Off-main-thread preparation of a streamed location's collision package: web/peak-world-prepare.js in a worker
// (web/peak-world.js starts it through web/worker-guard.js: build handshake, main-thread fallback).
import { serveWorker } from './worker-guard-child.js';
import { preparePeakLocation } from './peak-world-prepare.js';
serveWorker('peak-world', preparePeakLocation);
