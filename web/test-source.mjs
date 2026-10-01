// Test helper: a source file as compact code, for the tests that check how a module is wired by matching its text.
// Comments go, whitespace goes (one space stays between two word characters, `const x`, `return a`, and between a word and a
// quote, `case 'x'`), and a lone arrow parameter loses its parentheses (`(o) => o` -> `o=>o`), so a check does not depend on
// the file's formatting.
// String, template and regular-expression literals are kept as written.
import fs from 'node:fs';

const WORD = /[\w$]/;
const REGEX_AFTER = /(^|[(,=:[!&|?{};+\-*%<>~^]|\b(?:return|typeof|case|do|else|in|of|new|delete|void|throw|yield|await))$/;

export function compact(src) {
  let out = '', i = 0, pendingSpace = false;
  const emit = (s) => {
    if (pendingSpace && WORD.test(out.at(-1) ?? '') && (WORD.test(s[0]) || s[0] === "'" || s[0] === '"' || s[0] === '`')) out += ' ';
    pendingSpace = false;
    out += s;
  };
  const quoted = (q) => {
    let j = i + 1;
    while (j < src.length && src[j] !== q) {
      if (src[j] === '\\') j++;
      else if (q === '`' && src[j] === '$' && src[j + 1] === '{') {
        // a template expression: compact it too
        emit(src.slice(i, j + 2));
        let depth = 1, k = j + 2;
        const start = k;
        while (k < src.length && depth) {
          if (src[k] === '{') depth++;
          else if (src[k] === '}') depth--;
          else if (src[k] === '"' || src[k] === "'" || src[k] === '`') {
            const q2 = src[k];
            k++;
            while (k < src.length && src[k] !== q2) k += src[k] === '\\' ? 2 : 1;
          }
          k++;
        }
        out += compact(src.slice(start, k - 1));
        i = k - 1;
        j = i;
      }
      j++;
    }
    emit(src.slice(i, j + 1));
    i = j + 1;
  };
  while (i < src.length) {
    const c = src[i];
    if (c === '/' && src[i + 1] === '/') { while (i < src.length && src[i] !== '\n') i++; pendingSpace = true; continue; }
    if (c === '/' && src[i + 1] === '*') { const e = src.indexOf('*/', i + 2); i = e < 0 ? src.length : e + 2; pendingSpace = true; continue; }
    if (/\s/.test(c)) { i++; pendingSpace = true; continue; }
    if (c === '"' || c === "'" || c === '`') { quoted(c); continue; }
    if (c === '/' && REGEX_AFTER.test(out)) {
      let j = i + 1, cls = false;
      while (j < src.length && (src[j] !== '/' || cls)) { if (src[j] === '\\') j++; else if (src[j] === '[') cls = true; else if (src[j] === ']') cls = false; j++; }
      j++;
      while (WORD.test(src[j] ?? '')) j++;
      emit(src.slice(i, j));
      i = j;
      continue;
    }
    let j = i + 1;
    if (WORD.test(c)) while (j < src.length && WORD.test(src[j])) j++;
    emit(src.slice(i, j));
    i = j;
  }
  // `(o)=>` -> `o=>`
  return out.replace(/(^|[^\w$.)\]])\(([A-Za-z_$][\w$]*)\)=>/g, '$1$2=>');
}

/** The compact text of a file next to this one (or any path / URL). */
export function sourceOf(file) {
  return compact(fs.readFileSync(typeof file === 'string' && !file.startsWith('/') ? new URL(file, import.meta.url) : file, 'utf8'));
}
