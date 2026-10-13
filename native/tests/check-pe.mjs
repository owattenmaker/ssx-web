// The Windows cross build's link check (ctest windows_link in a build with native/cmake/llvm-mingw-x86_64.cmake): each executable is
// an x86-64 PE image that imports only Windows system DLLs (the UCRT, KERNEL32 ...), so it runs standalone on Windows. It cannot
// run here; docs/native.md "Windows" says how to run the parity tests on a Windows machine.
//   node check-pe.mjs OBJDUMP EXE...
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';

const [objdump, ...exes] = process.argv.slice(2);
const system = /^(kernel32|user32|advapi32|ws2_32|bcrypt|ntdll|shell32|ole32|api-ms-win-[a-z0-9-]+|ucrtbase|msvcrt)\.dll$/i;
let bad = 0;
for (const exe of exes) {
  const b = fs.readFileSync(exe);
  const pe = b.readUInt32LE(0x3c);
  const ok = b.toString('latin1', 0, 2) === 'MZ' && b.toString('latin1', pe, pe + 4) === 'PE\0\0' && b.readUInt16LE(pe + 4) === 0x8664;
  const dlls = [...execFileSync(objdump, ['-p', exe], { encoding: 'utf8', maxBuffer: 1 << 26 }).matchAll(/DLL Name: (\S+)/g)].map((m) => m[1]);
  const foreign = dlls.filter((d) => !system.test(d));
  if (!ok || foreign.length) bad++;
  console.log(`${exe}: ${ok ? 'PE x86-64' : 'NOT a PE x86-64 image'}, ${(b.length / 1048576).toFixed(1)} MB, imports ${dlls.join(' ')}${foreign.length ? ` (not system: ${foreign.join(' ')})` : ''}`);
}
process.exit(bad ? 1 : 0);
