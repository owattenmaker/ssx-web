// Worker side of web/worker-guard.js (docs/workers.md): the first message a worker sends is its hello with the build it
// was built from (web/build-id.js), so the page can refuse a worker that is not from its own build; pings are answered;
// requests ({__ssxw:'req', id, data}) run the handler and answer {__ssxw:'res', id, data | error}.
//   serveWorker(name, handler)               request mode: handler(data) -> reply | withTransfer(reply, transfer) | Promise
//   serveWorker(name, factory, {raw: true})  raw mode: factory(send) -> receive(event); the worker's own message protocol
// The same handlers run in the page when the worker is unavailable (the guard's local fallback), so they must not
// touch anything worker-only beyond fetch / createImageBitmap / timers.
import { BUILD_ID, expectedWorkerBuild } from './build-id.js';

const TRANSFER = Symbol.for('ssx.worker.transfer');
export const withTransfer = (reply, transfer = []) => ({ [TRANSFER]: true, reply, transfer });
export const unwrapReply = (out) => (out && out[TRANSFER] ? out : { reply: out, transfer: [] });

// Firefox's Error.stack has only the frames, not the message.
export const errorText = (e) =>
  e && typeof e === 'object' && 'message' in e
    ? String(e.stack ?? '').includes(e.message)
      ? String(e.stack)
      : `${e.name ?? 'Error'}: ${e.message}${e.stack ? '\n' + e.stack : ''}`
    : String(e);

export function serveWorker(name, handler, { raw = false } = {}) {
  const post = (message, transfer) => self.postMessage(message, transfer ?? []);
  const receive = raw ? handler((message, transfer) => post(message, transfer)) : null;
  self.onmessage = (event) => {
    const d = event.data;
    if (d && typeof d === 'object' && d.__ssxw) {
      if (d.__ssxw === 'ping') post({ __ssxw: 'pong', seq: d.seq });
      else if (d.__ssxw === 'req') run(d.id, d.data);
      return;
    }
    receive?.(event);
  };
  async function run(id, data) {
    try { const { reply, transfer } = unwrapReply(await handler(data)); post({ __ssxw: 'res', id, data: reply }, transfer); }
    catch (error) { post({ __ssxw: 'res', id, error: errorText(error) }); }
  }
  post({ __ssxw: 'hello', name, build: expectedWorkerBuild(name), page: BUILD_ID });
}
