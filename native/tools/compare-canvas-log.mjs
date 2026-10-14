// Compares two UI canvas call logs (native/tools/browser-shot.mjs --canvas-log from the page, ssx3_render_shot --canvas-log
// natively): entry by entry, the op, the image (a PNG's name or a canvas's getImageData hash), every argument, the transform and
// the global alpha, numbers exactly. Prints a summary and the first differences; exit code 1 when they differ.
//   node native/tools/compare-canvas-log.mjs PAGE.json NATIVE.json [--max 20]
import fs from 'node:fs';

const args = process.argv.slice(2);
const [pagePath, nativePath] = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1] === '--max'));
if (!pagePath || !nativePath) throw new Error('usage: compare-canvas-log.mjs PAGE.json NATIVE.json [--max N]');
const max = args.includes('--max') ? +args[args.indexOf('--max') + 1] : 20;
const page = JSON.parse(fs.readFileSync(pagePath, 'utf8'));
const native = JSON.parse(fs.readFileSync(nativePath, 'utf8'));

const diffs = [];
let worst = 0;
const same = (a, b, where) => {
  if (typeof a === 'number' && typeof b === 'number') {
    if (a !== b) {
      worst = Math.max(worst, Math.abs(a - b));
      diffs.push(`${where}: page ${a} native ${b}`);
    }
    return;
  }
  if (a !== b) diffs.push(`${where}: page ${JSON.stringify(a)} native ${JSON.stringify(b)}`);
};
const count = Math.max(page.length, native.length);
const images = new Map();
for (let i = 0; i < count; i++) {
  const p = page[i], n = native[i];
  if (!p || !n) {
    diffs.push(`#${i}: ${p ? JSON.stringify(p).slice(0, 160) : '(none)'} | ${n ? JSON.stringify(n).slice(0, 160) : '(none)'}`);
    continue;
  }
  same(p.op, n.op, `#${i} op`);
  if (p.op !== n.op) continue;
  if (p.img !== undefined || n.img !== undefined) {
    if (p.img !== n.img) images.set(`${p.img} -> ${n.img}`, (images.get(`${p.img} -> ${n.img}`) ?? 0) + 1);
    same(p.img, n.img, `#${i} ${p.op} img`);
  }
  const pa = p.args ?? [], na = n.args ?? [];
  if (pa.length !== na.length) diffs.push(`#${i} ${p.op} args: page ${JSON.stringify(pa)} native ${JSON.stringify(na)}`);
  else pa.forEach((v, k) => same(v, na[k], `#${i} ${p.op} arg ${k}`));
  (p.m ?? []).forEach((v, k) => same(v, n.m?.[k], `#${i} ${p.op} m[${k}]`));
  same(p.alpha, n.alpha, `#${i} ${p.op} alpha`);
}
const ops = (log) => log.reduce((o, e) => ((o[e.op] = (o[e.op] ?? 0) + 1), o), {});
console.log(JSON.stringify({ page: page.length, native: native.length, pageOps: ops(page), nativeOps: ops(native), differences: diffs.length, worstNumber: worst }));
for (const [k, v] of images) console.log(`image ${k}: ${v}`);
for (const d of diffs.slice(0, max)) console.log(d);
process.exit(diffs.length ? 1 : 0);
