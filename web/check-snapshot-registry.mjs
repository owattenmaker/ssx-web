// Build check of the rider-context snapshot (docs/replay.md §2a; web/world_snapshot.hpp): every thread-local variable of the core
// (the link map's .tdata / .tbss) is registered (web/generated/snapshot/registry.json), or a function-local static (the info
// exports' output buffers, kept as they are: listed here once reviewed), or the snapshot's own state. A new unregistered one would
// keep its end-of-race value through a replay restart or a Transport restore. Usage: check-snapshot-registry.mjs core.map
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const FILT = path.join(ROOT, 'local/vendor/emsdk/upstream/bin/llvm-cxxfilt');
// Function-local statics that hold state (restored by moving them to file scope, not listed here). Every other function-local
// static is an info export's output buffer: reviewed 2026-09-30 (the list printed by this check).
const OWN = /^(\(anonymous namespace\)::)?(snapshotContext|snapshotReport)$|^ssx_snapshot::/;
// Engine thread_locals kept as they are: the input map variant (a run's configuration, set_input_map at its start) and the EE
// rounding mode (set around each rounded operation).
const ENGINE_KEPT = /^ssx::(\(anonymous namespace\)::)?(inputMapVariant|originalRoundingMode)$/;
const map = fs.readFileSync(process.argv[2], 'utf8');
const mangled = [];
for (const line of map.split('\n')) { const m = line.match(/\s(\S+\.o):\(\.t(?:data|bss)\.([^)]+)\)$/); if (m) mangled.push(m[2].replace(/\.\d+$/, '')); }
const demangled = mangled.length ? execFileSync(FILT, { input: mangled.join('\n') }).toString().split('\n') : [];
const registry = JSON.parse(fs.readFileSync(path.join(ROOT, 'web/generated/snapshot/registry.json'), 'utf8'));
const names = new Set(registry.map((r) => r.name)), base = new Set(registry.map((r) => r.name.split('::').pop()));
const missing = [], locals = [];
demangled.forEach((d, k) => {
  if (!d) return; const n = d.replace(/^\(anonymous namespace\)::/, '');
  if (/^_ZZ|^_ZGVZ/.test(mangled[k])) { locals.push(d); return; } // function-local
  if (/^_ZGV/.test(mangled[k])) return; // a guard variable
  if (OWN.test(d) || ENGINE_KEPT.test(d) || names.has(n) || base.has(n.split('::').pop())) return;
  if (/^(__|_ZTH|_ZTW)/.test(mangled[k]) || /^std::|^__cxx|^emscripten|^tlsSize|^tlsTemplate/.test(d)) return; // runtime
  missing.push(d);
});
if (process.argv.includes('--locals')) console.log(locals.sort().join('\n'));
if (missing.length) {
  console.error(`check-snapshot-registry: ${missing.length} thread-local variable(s) the rider-context snapshot (docs/replay.md §2a) does not cover:\n  ${missing.join('\n  ')}\n` +
    'A file-scope RIDER_LOCAL is registered by web/generate-snapshot-registry.mjs: if one is listed, its declaration was not parsed (fix the generator). ' +
    'Otherwise classify it in web/snapshot-policy.mjs (SNAPSHOT_KEEP with its reason, SNAPSHOT_CURRENT, SNAPSHOT_OWN), or give a function-local static that holds state file scope.');
  process.exit(1);
}
console.log(`snapshot registry: ${registry.length} registered, ${locals.length} function-local output buffers, every TLS variable covered`);
