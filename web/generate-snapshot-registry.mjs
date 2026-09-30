// The rider-context snapshot's registry (docs/replay.md §2a; pv eventReturnInWorld (b)): every file-scope RIDER_LOCAL variable of
// the core's translation units, registered at static initialisation (web/world_snapshot.hpp) with its type, so a context's state
// can be saved and put back: the trivially copyable ones as bytes of the TLS block, the others (containers) by copy. Run by
// web/build-core.sh before the compile; writes web/generated/snapshot/<unit>.inc, which each unit includes at its end.
// Function-local statics are not registered (they are info exports' output buffers, kept as they are: web/check-snapshot-registry.mjs
// checks that against the link map, as it checks that every other TLS variable is registered).
import fs from 'node:fs';
import path from 'node:path';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const OUT = path.join(ROOT, 'web/generated/snapshot');
// The units with RIDER_LOCAL state (web/build-core.sh compiles them; each ends with its registry include).
export const UNITS = ['core', 'animation_bridge', 'environment_bridge', 'world_bridge', 'race_bridge', 'rail_bridge', 'prediction_bridge',
  'rider_lighting_bridge', 'race_world', 'attribute_bridge'];
// Kept as they are at a restore (not in the PS2's 0x26D818 list, or the snapshot's own state): name -> reason. A restore leaves
// their current value. Items the PS2 re-derives after its restore are reset by the restore's own calls (web/world_snapshot.hpp).
import { SNAPSHOT_KEEP, SNAPSHOT_OWN, SNAPSHOT_CURRENT, SNAPSHOT_REDERIVED } from './snapshot-policy.mjs';

function flatten(file, seen = new Set()) {
  if (seen.has(file)) return [];
  seen.add(file);
  const text = fs.readFileSync(file, 'utf8'), out = [];
  const lines = text.split('\n');
  for (let k = 0; k < lines.length; k++) {
    const m = lines[k].match(/^\s*#\s*include\s+"([^"]+)"/);
    if (m) {
      const inc = path.resolve(path.dirname(file), m[1]);
      if (fs.existsSync(inc) && inc.startsWith(path.join(ROOT, 'web')) && !inc.includes('/generated/snapshot/') && /\.(inc|cpp|hpp|h)$/.test(inc)) { out.push(...flatten(inc, seen)); continue; }
    }
    out.push({ file, line: k + 1, text: lines[k] });
  }
  return out;
}

// Strip comments and string / char literals (kept as spaces so columns stay), per line with a block-comment carry.
function strip(lines) {
  let block = false;
  return lines.map((l) => {
    let s = '', t = l.text, i = 0;
    while (i < t.length) {
      if (block) { const e = t.indexOf('*/', i); if (e < 0) { i = t.length; break; } block = false; i = e + 2; continue; }
      const c = t[i], d = t[i + 1];
      if (c === '/' && d === '/') break;
      if (c === '/' && d === '*') { block = true; i += 2; continue; }
      if (c === '"' || c === "'") { const q = c; s += ' '; i++; while (i < t.length && t[i] !== q) { if (t[i] === '\\') i++; i++; } i++; s += ' '; continue; }
      s += c; i++;
    }
    if (/^\s*#/.test(s)) s = ''; // preprocessor lines
    return { ...l, code: s };
  });
}

// Walk the unit: the scope stack (namespace name / '' anonymous / 'C' extern "C" / null other), and each RIDER_LOCAL
// declaration at namespace scope.
function scan(lines) {
  const decls = [];
  let text = '', map = [];
  for (const l of lines) { for (let k = 0; k < l.code.length; k++) map.push(l); text += l.code + '\n'; map.push(l); }
  const stack = []; let head = 0;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '{') {
      const before = text.slice(head, i).trim();
      // (string literals are blanked: `extern "C" {` reads `extern     {`)
      const ns = before.match(/(?:^|\s)namespace(?:\s+([A-Za-z_][\w:]*))?\s*$/), kind = ns ? { ns: ns[1] ?? '' } : /(?:^|\s)extern\s*$/.test(before) ? { c: true } : null;
      stack.push(kind); head = i + 1; continue;
    }
    if (c === '}') { stack.pop(); head = i + 1; continue; }
    if (c === ';') { head = i + 1; continue; }
    if (c === 'R' && text.startsWith('RIDER_LOCAL', i) && !/\w/.test(text[i - 1] ?? ' ') && !/\w/.test(text[i + 11] ?? ' ') || (c === 'R' && text.startsWith('RIDER_LOCAL_LAZY', i) && !/\w/.test(text[i - 1] ?? ' '))) {
      const fileScope = stack.every((s) => s && (s.ns !== undefined || s.c));
      // the declaration: up to its ';' at depth 0
      let j = i, depth = 0; for (; j < text.length; j++) { const d = text[j]; if ('({['.includes(d)) depth++; else if (')}]'.includes(d)) depth--; else if (d === ';' && depth === 0) break; }
      const decl = text.slice(i, j), where = map[i];
      if (fileScope) decls.push({ decl, file: path.relative(ROOT, where.file), line: where.line, ns: stack.filter((s) => s && s.ns).map((s) => s.ns) });
      // a RIDER_LOCAL inside another scope is a function-local static (not registered)
      i = j; head = j + 1;
    }
  }
  return decls;
}

// Names declared by one declaration (skipping extern ones and function declarations).
function names(decl) {
  let d = decl.replace(/^RIDER_LOCAL(_LAZY)?\b/, '').trim();
  if (/^(static\s+)?extern\b/.test(d) || /\bextern\b/.test(d.split(/[=({]/)[0])) return [];
  d = d.replace(/^(static|inline|constexpr|const)\s+/g, '');
  // an unnamed (or inline-defined) struct type: its body is not a declarator
  const tm = d.match(/^(struct|class|union)\b[^{=;]*\{/);
  if (tm) { let k = tm[0].length, depth = 1; for (; k < d.length && depth; k++) { if (d[k] === '{') depth++; else if (d[k] === '}') depth--; } d = 'T ' + d.slice(k); }
  // split at top-level commas (tracking <> only outside initialisers)
  const parts = []; let depth = 0, angle = 0, cur = '', init = false;
  for (const ch of d) {
    if ('({['.includes(ch)) depth++; else if (')}]'.includes(ch)) depth--;
    if (!init && depth === 0) { if (ch === '<') angle++; else if (ch === '>') angle--; }
    if (depth === 0 && angle === 0 && (ch === '=' || ch === '{')) init = true;
    if (ch === ',' && depth === 0 && angle === 0) { parts.push(cur); cur = ''; init = false; continue; }
    cur += ch;
  }
  parts.push(cur);
  const out = [];
  parts.forEach((p, k) => {
    const lhs = p.split(/=|\{/)[0].replace(/\[[^\]]*\]/g, '').trim();
    const fp = lhs.match(/\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(/); if (fp) { out.push(fp[1]); return; } // a function pointer
    if (/\)\s*$/.test(lhs) && k === 0 && /\(/.test(lhs)) return; // a function
    const m = lhs.match(/([A-Za-z_][\w]*(?:::[A-Za-z_]\w*)*)\s*$/);
    if (m && !/^(static|const|int|float|bool|double|unsigned|auto)$/.test(m[1])) out.push(m[1]);
  });
  return out;
}

export function generate() {
  fs.mkdirSync(OUT, { recursive: true });
  const all = [];
  for (const unit of UNITS) {
    const src = path.join(ROOT, 'web', unit + '.cpp');
    const decls = scan(strip(flatten(src)));
    const rows = [];
    for (const dcl of decls) for (const n of names(dcl.decl)) {
      const q = n.includes('::') ? n : [...dcl.ns.filter((x) => x), n].join('::');
      const cur = SNAPSHOT_CURRENT[q] ?? SNAPSHOT_CURRENT[n], own = SNAPSHOT_OWN.includes(q) || SNAPSHOT_OWN.includes(n) || !!cur; // (own / current: not restored, not checked)
      const rederived = SNAPSHOT_REDERIVED.includes(q) || SNAPSHOT_REDERIVED.includes(n);
      rows.push({ name: q, file: dcl.file, line: dcl.line, keep: cur ? 'not in the PS2 snapshot: ' + cur : own ? 'the snapshot hook\'s own storage' : SNAPSHOT_KEEP[q] ?? SNAPSHOT_KEEP[n] ?? null, own, rederived });
    }
    const body = rows.map((r) => `  ssx_snapshot::add(&::${r.name}, ${JSON.stringify(r.name)}, ${JSON.stringify(r.file)}, ${r.own ? 2 : r.keep ? 1 : r.rederived ? 3 : 0});`).join('\n');
    const code = `// Generated by web/generate-snapshot-registry.mjs from ${unit}.cpp and its includes: do not edit.\n#include "../../world_snapshot.hpp"\nnamespace {\nstruct SnapshotRegistry_${unit} { SnapshotRegistry_${unit}() {\n${body}\n} };\n[[maybe_unused]] SnapshotRegistry_${unit} snapshotRegistry_${unit};\n}\n`;
    const file = path.join(OUT, unit + '.inc');
    if (!fs.existsSync(file) || fs.readFileSync(file, 'utf8') !== code) fs.writeFileSync(file, code);
    all.push(...rows.map((r) => ({ ...r, unit })));
  }
  const stale = [...Object.keys(SNAPSHOT_KEEP), ...SNAPSHOT_OWN, ...Object.keys(SNAPSHOT_CURRENT), ...SNAPSHOT_REDERIVED].filter((k) => !all.some((r) => r.name === k || r.name.split('::').pop() === k));
  if (stale.length) throw new Error(`web/snapshot-policy.mjs keeps names that are not registered: ${stale.join(', ')}`);
  fs.writeFileSync(path.join(OUT, 'registry.json'), JSON.stringify(all, null, 1) + '\n');
  return all;
}

if (import.meta.url === `file://${process.argv[1]}`) { const all = generate(); console.log(`snapshot registry: ${all.length} variables in ${UNITS.length} units (${all.filter((r) => r.keep).length} kept)`); }
