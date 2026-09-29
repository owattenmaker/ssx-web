// Off-main-thread preparation of a front-end preview package: web/fe-preview-prepare.js in a worker (web/fe-preview.js
// starts it through web/worker-guard.js: build handshake, main-thread fallback, docs/workers.md).
import { serveWorker } from './worker-guard-child.js';
import { prepareFrontEndPreview } from './fe-preview-prepare.js';
serveWorker('fe-preview', prepareFrontEndPreview);
