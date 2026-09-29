// Trick HUD draw lists (web/trick-hud.js) against original draws captured by tools/probe_trick_hud.py
// (0x1E9A30 slot cases run on the user's snapshot memory; font submissions 0x391CB0, sprites 0x1F1190).
import fs from 'node:fs';
import { TrickHud } from './trick-hud.js';
const ui = new URL('public/assets/UI/', import.meta.url);
const data = JSON.parse(fs.readFileSync(new URL('trick-hud.json', ui)));
const glyphs = { FEFONT: JSON.parse(fs.readFileSync(new URL('FEFONT-glyphs.json', ui))), HUDFONT: JSON.parse(fs.readFileSync(new URL('HUDFONT-glyphs.json', ui))) };
const hud = new TrickHud(data, glyphs);
const casesPath = new URL('../local/browser-ui/trick-hud/trick-hud-cases.json', import.meta.url);
if (!fs.existsSync(casesPath)) { console.log('skip trick HUD: original draw capture not present (python3 tools/probe_trick_hud.py)'); process.exit(0); }
data.cases = JSON.parse(fs.readFileSync(casesPath)).cases;
// Trick-scoring slot types and the checkpoint / peak-run split messages 0x29..0x2B (docs/peak3.md); the career popups
// (0x2C..) are not ported here.
const TRICK_TYPES = new Set([0, 1, 2, 3, 4, 7, 0xB, 0xE, 0x19, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x2F, 0x31]); // 0x2E/0x2F/0x31: Conquer the Mountain cash / 'Collect +$ n' (cases with the free-ride flags)
const FONT = { [0x5a0100]: 'FEFONT', [0x5a0200]: 'HUDFONT' };
const only = process.argv[2] ? process.argv[2].split(',').map((x) => parseInt(x, 16)) : null;
const decode = (s) => Buffer.from(s, 'latin1').toString('latin1');
let checked = 0, failed = 0, worst = 0; const failures = new Map();
const near = (a, b, tol) => Math.abs(a - b) <= tol;
for (const c of data.cases) {
  if (only ? !only.includes(c.type) : !TRICK_TYPES.has(c.type)) continue;
  const slots = new Array(44).fill(null); const index = c.type < 0x22 ? c.type : 0x23; const maximum = c.maximum ?? -1;
  slots[index] = { type: c.type, maximum, value: maximum === -1 ? -c.ratio : Math.fround(c.ratio * maximum), arg: c.arg ?? 0, field10: c.field10 ?? 0, points: c.points ?? 0, text: c.text ?? String(c.points ?? 0), ratio: Math.fround(c.ratio) }; // the probe sets f22 = ratio directly
  const want = c.draws.filter((d) => d.kind === 'text' || d.kind === 'sprite');
  if (c.type === 7) want.splice(1); // the score case continues into race standings (flags 0x10), not part of the trick HUD
  // Labels (0x1C..0x20) and the recover bar are drawn after the slot loop; the probe stops at the loop continuation.
  const got = hud.frame(slots, { flash: c.flash, pulse25: c.pulse, ...(c.flags !== undefined ? { flags: c.flags } : {}), collectMode: c.mode88 ?? 0 }).filter((d) => (d.kind === 'text' || d.kind === 'sprite') && !d.epilogue && !d.recover);
  const problems = [];
  if (got.length !== want.length) problems.push(`draw count web ${got.length} original ${want.length}`);
  for (let k = 0; k < Math.min(got.length, want.length); k++) {
    const g = got[k], w = want[k];
    if (g.kind !== w.kind) { problems.push(`#${k} kind ${g.kind}/${w.kind}`); continue; }
    const err = Math.max(Math.abs(g.x - w.position[0]), Math.abs(g.y - w.position[1])); worst = Math.max(worst, err);
    if (!near(g.x, w.position[0], 0.02) || !near(g.y, w.position[1], 0.02)) problems.push(`#${k} pos web ${g.x.toFixed(3)},${g.y.toFixed(3)} original ${w.position.map((v) => v.toFixed(3))}`);
    if (!g.scale.every((v, i) => near(v, w.scale[i], 1e-4))) problems.push(`#${k} scale web ${g.scale} original ${w.scale}`);
    if (!g.argb.every((v, i) => near(v, w.argb[i], 1e-4))) problems.push(`#${k} argb web ${g.argb} original ${w.argb}`);
    if (g.kind === 'text') {
      if (g.text !== decode(w.text)) problems.push(`#${k} text web ${g.text} original ${w.text}`);
      if (FONT[w.font] !== g.font) problems.push(`#${k} font web ${g.font} original ${w.font.toString(16)}`);
      if (!g.shadow.every((v, i) => near(v, w.shadow[i], 1e-6))) problems.push(`#${k} shadow web ${g.shadow} original ${w.shadow}`);
    } else {
      const size = g.size || [g.w, g.h];
      if (!size.every((v, i) => near(v, w.size[i], 0.02))) problems.push(`#${k} size web ${size} original ${w.size}`);
      if (!g.sprite.uv.every((v, i) => near(v, w.uv[i], 1e-6))) problems.push(`#${k} uv web ${g.sprite.uv} original ${w.uv}`);
    }
  }
  checked++;
  if (problems.length) { failed++; const key = c.type.toString(16); if (!failures.has(key)) failures.set(key, []); failures.get(key).push(`ratio ${c.ratio} arg ${c.arg} ${problems.join('; ')}`); }
}
for (const [type, list] of failures) { console.log(`type 0x${type}: ${list.length} case(s)`); for (const l of list.slice(0, 3)) console.log('   ', l); }
console.log(`trick HUD: ${checked - failed}/${checked} captured slot states match the original draw lists (max position error ${worst.toFixed(4)} px)`);
if (failed && !process.env.TRICK_HUD_REPORT_ONLY) process.exit(1);
