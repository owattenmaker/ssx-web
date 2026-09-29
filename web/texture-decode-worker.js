// Off-thread BC texture decode (web/texture-decode-job.js): the Xbox HD rider textures (web/texture-archive.js) are decoded or
// given their BC mip chain here, so an outfit load never costs a main-thread decode. Started through web/worker-guard.js
// (build handshake, main-thread fallback, docs/workers.md).
import { serveWorker } from './worker-guard-child.js';
import { decodeTextureJob } from './texture-decode-job.js';
serveWorker('texture-decode', decodeTextureJob);
