// Hand-written code stays readable: no line of web/*.js, web/net/*.js or web/server/*.mjs is longer than MAX characters.
// Text inside a string or template literal (data tables, worker sources, CSS) does not count: a line passes when its character MAX
// is inside one. Generated files are listed in GENERATED.
//   node test-line-length.mjs
import fs from 'node:fs';
import path from 'node:path';
import { parseAst } from 'rolldown/parseAst';

const MAX = 200;
const web = path.dirname(new URL(import.meta.url).pathname);
// written by tools, not by hand
const GENERATED = new Set(['three-patches.js']);   // tools/gen_three_patches.py
const files = [
  ...fs.readdirSync(web).filter((f) => f.endsWith('.js')),
  ...fs.readdirSync(path.join(web, 'net')).filter((f) => f.endsWith('.js')).map((f) => 'net/' + f),
  ...fs.readdirSync(path.join(web, 'server')).filter((f) => f.endsWith('.mjs')).map((f) => 'server/' + f)
].filter((f) => !GENERATED.has(f));

const bad = [];
for (const file of files) {
  const src = fs.readFileSync(path.join(web, file), 'utf8');
  if (!src.split('\n').some((l) => l.length > MAX)) continue;
  // the string and template literals' spans: their text is data
  const spans = [];
  (function walk(n) {
    if (!n || typeof n !== 'object') return;
    if (Array.isArray(n)) { n.forEach(walk); return; }
    if (n.type === 'TemplateLiteral' || (n.type === 'Literal' && typeof n.value === 'string')) spans.push([n.start, n.end]);
    for (const k in n) if (k !== 'start' && k !== 'end') walk(n[k]);
  })(parseAst(src, { sourceType: 'module' }));
  let at = 0;
  src.split('\n').forEach((line, i) => {
    const start = at;
    at += line.length + 1;
    if (line.length <= MAX) return;
    const inLiteral = spans.some(([a, b]) => start + MAX > a && start + MAX < b);
    if (!inLiteral) bad.push(`${file}:${i + 1} (${line.length} characters)`);
  });
}
if (bad.length) {
  console.error(`${bad.length} line(s) over ${MAX} characters:\n  ${bad.slice(0, 40).join('\n  ')}${bad.length > 40 ? '\n  ...' : ''}`);
  console.error('Put the comment on its own line above the code it describes, keep it short, and break the statement (one statement per line).');
  process.exit(1);
}
console.log(`line length OK: ${files.length} files, no line over ${MAX} characters`);
