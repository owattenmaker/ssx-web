// Post-link step of web/build-core.sh for the rider contexts (web/rider_local.hpp, web/rider_tls.S): wasm-ld keeps a static TLS block (.tdata) for a
// non-shared memory, but the __tls_base we define (mutable, so a rider context switch is one global.set) starts at 0.
// Patch its init to the .tdata address and rider_tls_size's init to the block size, both read from the link map.
import fs from 'node:fs';
const [wasmPath, jsPath, mapPath] = process.argv.slice(2);
const map = fs.readFileSync(mapPath, 'utf8');
const m = map.match(/^\s*([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\.tdata$/m);
if (!m) throw new Error('no .tdata in the link map');
const va = parseInt(m[1], 16), size = parseInt(m[2], 16);
const js = fs.readFileSync(jsPath, 'utf8');
const exportOf = (name) => { const r = js.match(new RegExp(name.replace(/[$]/g, '\\$') + '=Module\\["' + name + '"\\]=wasmExports\\["([^"]+)"\\]')); if (!r) throw new Error('no export mapping for ' + name); return r[1]; };
const targets = { [exportOf('___tls_base')]: va, [exportOf('_rider_tls_size')]: size };
const buf = fs.readFileSync(wasmPath);
let p = 8; const sections = [];
const uleb = (b, at) => { let r = 0, s = 0, x; do { x = b[at++]; r |= (x & 0x7f) << s; s += 7; } while (x & 0x80); return [r >>> 0, at]; };
const encU = (v) => { const o = []; do { let x = v & 0x7f; v >>>= 7; if (v) x |= 0x80; o.push(x); } while (v); return o; };
const encS = (v) => { const o = []; for (;;) { const x = v & 0x7f; v >>= 7; if ((v === 0 && !(x & 0x40)) || (v === -1 && (x & 0x40))) { o.push(x); break; } o.push(x | 0x80); } return o; };
while (p < buf.length) { const id = buf[p]; let [len, q] = uleb(buf, p + 1); sections.push({ id, start: p, body: q, end: q + len }); p = q + len; }
const sec = (id) => sections.find((s) => s.id === id);
let importedGlobals = 0;
const imp = sec(2);
if (imp) { let [n, q] = uleb(buf, imp.body); for (let i = 0; i < n; i++) { let l; [l, q] = uleb(buf, q); q += l; [l, q] = uleb(buf, q); q += l; const kind = buf[q++];
  if (kind === 0) [, q] = uleb(buf, q); else if (kind === 1) { q++; const f = buf[q++]; [, q] = uleb(buf, q); if (f & 1) [, q] = uleb(buf, q); } else if (kind === 2) { const f = buf[q++]; [, q] = uleb(buf, q); if (f & 1) [, q] = uleb(buf, q); } else if (kind === 3) { q += 2; importedGlobals++; } else if (kind === 4) { q++; [, q] = uleb(buf, q); } else throw new Error('import kind ' + kind); } }
const exp = sec(7); const want = {}; { let [n, q] = uleb(buf, exp.body); for (let i = 0; i < n; i++) { let l; [l, q] = uleb(buf, q); const name = buf.subarray(q, q + l).toString(); q += l; const kind = buf[q++]; let idx; [idx, q] = uleb(buf, q); if (kind === 3 && name in targets) want[idx] = targets[name]; } }
if (Object.keys(want).length !== 2) throw new Error('TLS globals not exported');
const gs = sec(6); let [n, q] = uleb(buf, gs.body); const out = [...encU(n)]; let patched = 0;
for (let i = 0; i < n; i++) { const start = q; q += 2; while (buf[q] !== 0x0b) { const op = buf[q++]; if (op === 0x41 || op === 0x42) [, q] = uleb(buf, q); else if (op === 0x23) [, q] = uleb(buf, q); else if (op === 0x43) q += 4; else if (op === 0x44) q += 8; else if (op === 0xd0) q++; else if (op === 0xd2) [, q] = uleb(buf, q); else throw new Error('init op ' + op); } q++;
  const gi = importedGlobals + i;
  if (gi in want) { if (buf[start] !== 0x7f || buf[start + 1] !== 1) throw new Error('TLS global not a mutable i32'); out.push(0x7f, 1, 0x41, ...encS(want[gi]), 0x0b); patched++; }
  else out.push(...buf.subarray(start, q)); }
if (patched !== 2) throw new Error('patched ' + patched);
const body = Buffer.from(out), head = Buffer.from([6, ...encU(body.length)]);
fs.writeFileSync(wasmPath, Buffer.concat([buf.subarray(0, gs.start), head, body, buf.subarray(gs.end)]));
console.log(`rider TLS: .tdata 0x${va.toString(16)} size ${size}`);
