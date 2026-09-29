// Off-thread bank patch decoding (web/sfx.js): the EE-exact MicroTalk decoder costs ~0.4 us per sample, so a
// location bank (1-2 M samples) would block the main thread for most of a second; MicroTalk music bars
// (web/pathfinder.js musicDecodeWorker, ~40 ms each) too. The job is web/audio-decode-job.js; web/worker-guard.js
// starts this worker (build handshake, main-thread fallback, docs/workers.md).
import { serveWorker } from './worker-guard-child.js';
import { decodeAudioJob } from './audio-decode-job.js';
serveWorker('audio-decode', decodeAudioJob);
