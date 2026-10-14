// The Windows cross build's link check (ctest windows_link in a build with native/cmake/llvm-mingw-x86_64.cmake): each executable is
// an x86-64 PE image that imports only Windows system DLLs (the UCRT, KERNEL32 ...), so it runs standalone on Windows. It cannot
// run here; docs/native.md "Windows" says how to run the parity tests on a Windows machine.
// The front end (ssx3_render_shot, ssx3) also imports the Windows graphics DLLs wgpu-native's D3D12 / Vulkan / GL backends load (Direct3D,
// DXGI, OpenGL and the shell / COM ones it uses), and ssx3.exe SDL3.dll, which must sit next to it (--allow SDL3.dll).
//   node check-pe.mjs OBJDUMP [--allow DLL]... EXE_OR_DLL...
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const args = process.argv.slice(2);
const objdump = args.shift();
const allowed = [];
const exes = [];
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--allow') allowed.push(args[++i].toLowerCase());
  else exes.push(args[i]);
}
// Windows system DLLs (installed with every Windows 10 / 11)
const system = new RegExp(
  '^(' +
    [
      'kernel32', 'kernelbase', 'user32', 'gdi32', 'advapi32', 'ws2_32', 'bcrypt', 'ntdll', 'shell32', 'ole32', 'oleaut32', 'combase',
      'ucrtbase', 'msvcrt', 'userenv', 'propsys', 'setupapi', 'imm32', 'version', 'winmm', 'cfgmgr32', 'hid', 'shlwapi', 'dwmapi',
      'uxtheme', 'crypt32', 'secur32', 'dbghelp', 'opengl32', 'd3d11', 'd3d12', 'dxgi', 'd3dcompiler_47', 'dcomp', 'runtimeobject',
      'bcryptprimitives', 'rpcrt4', 'msvcp_win',
      'api-ms-win-[a-z0-9-]+'
    ].join('|') +
    ')\\.dll$',
  'i'
);
let bad = 0;
for (const exe of exes) {
  const b = fs.readFileSync(exe);
  const pe = b.readUInt32LE(0x3c);
  const ok = b.toString('latin1', 0, 2) === 'MZ' && b.toString('latin1', pe, pe + 4) === 'PE\0\0' && b.readUInt16LE(pe + 4) === 0x8664;
  const dlls = [...execFileSync(objdump, ['-p', exe], { encoding: 'utf8', maxBuffer: 1 << 26 }).matchAll(/DLL Name: (\S+)/g)].map((m) => m[1]);
  // a shipped DLL counts only when it is next to the executable
  const shipped = (d) => allowed.includes(d.toLowerCase()) && fs.existsSync(path.join(path.dirname(exe), d));
  const foreign = dlls.filter((d) => !system.test(d) && !shipped(d));
  if (!ok || foreign.length) bad++;
  const ships = dlls.filter(shipped);
  console.log(
    `${exe}: ${ok ? 'PE x86-64' : 'NOT a PE x86-64 image'}, ${(b.length / 1048576).toFixed(1)} MB, imports ${dlls.join(' ')}` +
      `${ships.length ? ` (shipped: ${ships.join(' ')})` : ''}${foreign.length ? ` (missing: ${foreign.join(' ')})` : ''}`
  );
}
process.exit(bad ? 1 : 0);
