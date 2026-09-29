// WGSL memory budget (docs/mobile.md): WebKit (Safari, every iPhone browser) enforces the WGSL implementation limits
// "combined byte size of all variables in the private address space" and "... in the function address space" per
// shader, 8192 bytes each, and rejects the pipeline otherwise ("The combined byte size of all variables in the private
// address space exceeds 8192 bytes"): the object it draws is simply missing. Chrome (Tint) and Firefox (naga) accept the
// same shader, so only a size check catches it off-device. three.js r186 declares every TSL temporary as a module-scope
// `var<private>` (WGSLNodeBuilder.getVars), so a node graph that repeats itself (a select() emits its input in both
// branches) grows past the limit quickly; fog-renderer.js and glare-pass.js keep their stage results in vars for that.
//
// wgslMemory(code) -> {privateBytes, functionBytes (the largest function), vars, largest: [{name, type, bytes}]}.
// Sizes follow the WGSL layout rules (SizeOf); each variable is rounded up to its alignment (a vec3<f32> counts 16), so
// the estimate is on the safe side of WebKit's count.
export const WGSL_PRIVATE_LIMIT = 8192, WGSL_FUNCTION_LIMIT = 8192;
const SCALARS = { f32: 4, i32: 4, u32: 4, f16: 2, bool: 4 };
const SHORT = { f: 'f32', i: 'i32', u: 'u32', h: 'f16' };
const roundUp = (n, a) => Math.ceil(n / a) * a;

export function wgslTypeLayout(type, structs = {}) {
  let t = type.trim(), m;
  if (SCALARS[t]) return { size: SCALARS[t], align: SCALARS[t] };
  if ((m = /^vec([234])([fiuh])$/.exec(t))) t = `vec${m[1]}<${SHORT[m[2]]}>`;
  if ((m = /^mat([234])x([234])([fh])$/.exec(t))) t = `mat${m[1]}x${m[2]}<${SHORT[m[3]]}>`;
  if ((m = /^vec([234])<\s*(\w+)\s*>$/.exec(t)) && SCALARS[m[2]]) { const n = +m[1], s = SCALARS[m[2]]; return { size: n * s, align: (n === 3 ? 4 : n) * s }; }
  if ((m = /^mat([234])x([234])<\s*(\w+)\s*>$/.exec(t))) { const col = wgslTypeLayout(`vec${m[2]}<${m[3]}>`); return { size: +m[1] * roundUp(col.size, col.align), align: col.align }; }
  if ((m = /^array<(.+),\s*(\d+)[ui]?\s*>$/.exec(t))) { const e = wgslTypeLayout(m[1], structs); return { size: +m[2] * roundUp(e.size, e.align), align: e.align }; }
  if ((m = /^atomic<\s*(\w+)\s*>$/.exec(t))) return wgslTypeLayout(m[1], structs);
  if (structs[t]) {
    let offset = 0, align = 1;
    for (const member of structs[t]) { const l = wgslTypeLayout(member, structs); offset = roundUp(offset, l.align) + l.size; align = Math.max(align, l.align); }
    return { size: roundUp(offset, align), align };
  }
  return { size: 16, align: 16, unknown: true }; // an unparsed type: counted as one vec4
}

export function wgslMemory(code) {
  const src = String(code).replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
  const structs = {};
  for (const m of src.matchAll(/\bstruct\s+(\w+)\s*\{([^}]*)\}/g))
    structs[m[1]] = m[2].split(',').map((s) => s.replace(/@\w+(\s*\([^)]*\))?/g, '').trim()).filter(Boolean).map((s) => s.slice(s.indexOf(':') + 1).trim());
  const bytes = (type) => { const l = wgslTypeLayout(type, structs); return roundUp(l.size, l.align); };
  // Module scope (outside every brace pair): var<private> declarations.
  let depth = 0, top = '';
  for (const ch of src) { if (ch === '{') depth++; if (depth === 0) top += ch; if (ch === '}') depth--; }
  const vars = [];
  for (const m of top.matchAll(/\bvar\s*<\s*private\s*>\s*(\w+)\s*:\s*([^;=]+)[;=]/g)) vars.push({ name: m[1], type: m[2].trim(), bytes: bytes(m[2]) });
  // Function scope: explicitly typed `var name : T` in each function body (three.js always writes the type).
  let functionBytes = 0;
  for (const m of src.matchAll(/\bfn\s+\w+\s*\(/g)) {
    const start = src.indexOf('{', m.index); if (start < 0) break;
    let d = 0, i = start; for (; i < src.length; i++) { if (src[i] === '{') d++; else if (src[i] === '}' && --d === 0) break; }
    let total = 0; for (const v of src.slice(start, i).matchAll(/\bvar(?:\s*<\s*function\s*>)?\s+(\w+)\s*:\s*([^;=]+)[;=]/g)) total += bytes(v[2]);
    functionBytes = Math.max(functionBytes, total);
  }
  const privateBytes = vars.reduce((a, v) => a + v.bytes, 0);
  return { privateBytes, functionBytes, vars: vars.length, largest: vars.sort((a, b) => b.bytes - a.bytes).slice(0, 3) };
}
