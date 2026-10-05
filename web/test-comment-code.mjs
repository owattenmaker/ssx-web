// No code hides inside a comment. A `//` comment put in the middle of a line of code turns the rest of that line into comment text:
// 2026-09-30 one did that to career-ui.js back(), and three dispatches stopped running without any error.
// Every `//` comment of web/*.js, web/*.mjs, web/net/*.js, web/server/*.mjs and tools/*.mjs (and one level below) is parsed as
// JavaScript: its whole text and each suffix that starts at a code-like token. The first suffix that parses and holds a call,
// return, if or assignment gets a score. The score rises with `;`, `return`, `if (`, `{ }`, `this.` / `ui.`, several statements,
// length and code glued to the word before it (`pv onlineRecordsif(...)`). Prose and usage notes score up to about 6 (2026-10-04);
// the career-ui case scored 13. A comment at THRESHOLD or above fails.
//   node test-comment-code.mjs
import fs from 'node:fs';
import path from 'node:path';
import { parseSync } from 'rolldown/experimental';

const THRESHOLD = 8;
const web = path.dirname(new URL(import.meta.url).pathname);
const root = path.dirname(web);
// written by tools, not by hand
const GENERATED = new Set(['web/three-patches.js']);
const list = (dir, ext) =>
  fs.existsSync(path.join(root, dir))
    ? fs
        .readdirSync(path.join(root, dir))
        .filter((f) => ext.some((e) => f.endsWith(e)))
        .map((f) => dir + '/' + f)
    : [];
const toolDirs = fs
  .readdirSync(path.join(root, 'tools'), { withFileTypes: true })
  .filter((d) => d.isDirectory() && d.name !== 'node_modules' && d.name !== '__pycache__')
  .map((d) => 'tools/' + d.name);
const files = [
  ...list('web', ['.js', '.mjs']),
  ...list('web/net', ['.js']),
  ...list('web/server', ['.mjs']),
  ...list('tools', ['.mjs']),
  ...toolDirs.flatMap((d) => list(d, ['.mjs']))
].filter((f) => !GENERATED.has(f));

const CODE = new Set(['CallExpression', 'ReturnStatement', 'IfStatement', 'AssignmentExpression', 'VariableDeclaration']);

// the statements of t, or null when t is not JavaScript (wrapped in a function so that return and await parse)
function statements(t) {
  const r = parseSync('comment.js', `async function __comment() {\n${t}\n}`, { sourceType: 'module' });
  if (r.errors.length) return null;
  return r.program.body[0]?.body?.body ?? null;
}

function hasCode(nodes) {
  let found = false;
  (function walk(n) {
    if (found || !n || typeof n !== 'object') return;
    if (Array.isArray(n)) {
      n.forEach(walk);
      return;
    }
    if (CODE.has(n.type)) {
      found = true;
      return;
    }
    for (const k in n) if (k !== 'start' && k !== 'end') walk(n[k]);
  })(nodes);
  return found;
}

// the score of a comment's text: 0 when no suffix of it is code
export function codeScore(text) {
  const starts = new Set([0]);
  for (let i = 1; i < text.length; i++) {
    if (/[\s;:,.)\]}>-]/.test(text[i - 1]) && /[A-Za-z_$({[!]/.test(text[i])) starts.add(i);
  }
  for (const m of text.matchAll(/(?<=[a-z0-9_)\]])(if|return|const|let|var|for|while|this|ui|await)\b/g)) starts.add(m.index);
  for (const i of [...starts].sort((a, b) => a - b)) {
    const t = text.slice(i).trim();
    if (t.length < 8) continue;
    const body = statements(t);
    if (!body || !hasCode(body)) continue;
    let s = 0;
    if (/;/.test(t)) s += 2;
    if (/\breturn\b/.test(t)) s += 2;
    if (/\bif\s*\(/.test(t)) s += 2;
    if (/[{}]/.test(t)) s += 1;
    if (/\bthis\.|\bui\./.test(t)) s += 1;
    if (body.length > 1) s += 2;
    if (t.length > 40) s += 1;
    // glued to the word before it: a block comment's end or a line break lost
    if (i > 0 && /[a-z0-9]$/i.test(text.slice(0, i))) s += 3;
    return s;
  }
  return 0;
}

// the detector itself: the 2026-09-30 case fails, a prose note passes
const swallowed = ' pv onlineRecordsif(this.lodge.owns(s))return this.lodge.back();if(this.messages.owns(s))return this.messages.goBack();';
if (codeScore(swallowed) < THRESHOLD) {
  console.error(`the detector misses the career-ui back() case (score ${codeScore(swallowed)})`);
  process.exit(1);
}
const prose = " The map region: s = 'ctm-peaks' | 'ctm-goals' | 'ctm-events'; o = {index, peak, herePeak, tab, info, red, indicator, locked}";
if (codeScore(prose) >= THRESHOLD) {
  console.error(`the detector flags a prose note (score ${codeScore(prose)})`);
  process.exit(1);
}

const bad = [];
let comments = 0;
for (const file of files) {
  const src = fs.readFileSync(path.join(root, file), 'utf8');
  const r = parseSync(file, src, { sourceType: 'module' });
  if (r.errors.length) {
    bad.push(`${file}: does not parse (${r.errors[0].message})`);
    continue;
  }
  for (const c of r.comments) {
    if (c.type !== 'Line') continue;
    comments++;
    const score = codeScore(c.value);
    if (score < THRESHOLD) continue;
    const line = src.slice(0, c.start).split('\n').length;
    bad.push(`${file}:${line} (score ${score}): //${c.value.slice(0, 140)}`);
  }
}
if (bad.length) {
  console.error(`${bad.length} comment(s) hold code that does not run:\n  ${bad.join('\n  ')}`);
  console.error('Move the comment to its own line (above the code it describes), so the code after it runs again.');
  process.exit(1);
}
console.log(`comment code OK: ${files.length} files, ${comments} line comments, none holds code (threshold ${THRESHOLD})`);
