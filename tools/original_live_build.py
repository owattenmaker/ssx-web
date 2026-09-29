#!/usr/bin/env python3
"""Full-original live harness builder (development oracle only).

Links every recompiled function reachable through `jal` from the given roots,
plus the entries of the given vtables (virtual targets reached by `jalr`), as
PCSX2 scalar-FP oracle copies. Every switch label of an included file is
registered, so a C++ trampoline (`tests/original_live_runtime.hpp`) can resume
intra-file continuations exactly like the runtime's dispatcher. Function
entries that the recompiler folded into a neighbouring file get an explicit
entry label in the oracle copy; the original generated files never change.
Entry calls are counted (`callCounts`) for coverage reports.
"""
import bisect, glob, os, re, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_fp_oracle import write_scalar_fp_oracle  # noqa: E402
from original_oracle_build import cached_oracle_build  # noqa: E402

OUTPUT = ROOT / 'local/output'
_files = sorted((int(re.search(r'sub_([0-9A-F]{8})_0x', os.path.basename(f)).group(1), 16), f)
                for f in glob.glob(str(OUTPUT / 'sub_*.cpp')))
_starts = [s for s, _ in _files]
_info = {}


def file_info(path):
    if path in _info:
        return _info[path]
    text = Path(path).read_text(errors='replace')
    m = re.search(r'// Address: 0x([0-9a-f]+) - 0x([0-9a-f]+)', text)
    if not m:  # library stub file: a single entry
        start = int(re.search(r'sub_([0-9A-F]{8})_0x', path).group(1), 16)
        _info[path] = (start, start + 4, {start}, set())
    else:
        cases = {int(x, 16) for x in re.findall(r'case 0x([0-9a-f]+)u:', text)}
        jals = {int(x, 16) for x in re.findall(r'jal\s+func_([0-9A-F]+)', text)}
        _info[path] = (int(m.group(1), 16), int(m.group(2), 16), cases, jals)
    return _info[path]


def containing_file(pc):
    i = bisect.bisect_right(_starts, pc) - 1
    path = _files[i][1]
    lo, hi, _, _ = file_info(path)
    if not lo <= pc < hi:
        raise ValueError(f'No recompiled file contains 0x{pc:x}')
    return path


def closure(roots):
    need, todo, seen = {}, list(roots), set()
    while todo:
        pc = todo.pop()
        if pc in need:
            continue
        path = containing_file(pc)
        need[pc] = path
        if path not in seen:
            seen.add(path)
            todo.extend(file_info(path)[3])
    return need


def vtable_targets(vtables):
    """Function words of original vtables ((address, byte length) pairs) in the ELF."""
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    out = set()
    for base, length in vtables:
        for offset in range(4, length, 8):
            target = struct.unpack_from('<I', elf, base + offset - 0xFF000)[0]
            if 0x100000 <= target < 0x440000:
                out.add(target)
    return sorted(out)


def build_live(roots, main_source, binary, folder, extra_sources=(), defines=(), cache=None):
    need = closure(roots)
    by_file = {}
    for pc, path in need.items():
        by_file.setdefault(path, set()).add(pc)
    folder = Path(folder).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    sources, regs, decls = [], [], []
    for path, pcs in sorted(by_file.items()):
        text = Path(path).read_text(errors='replace')
        name = re.search(r'void (sub_[0-9A-F]{8}_0x[0-9a-f]+)\(', text).group(1)
        start = int(name[4:12], 16)
        _, _, cases, _ = file_info(path)
        extra = sorted(pc for pc in pcs if pc != start and pc not in cases)
        if extra:
            if 'switch (ctx->pc) {' not in text:
                m = re.search(r'void ' + name + r'\([^)]*\) \{\n(?:#ifdef PS2_FUNCTION_LOG_TRACKER\n.*\n#endif\n)?', text)
                text = text[:m.end()] + '    switch (ctx->pc) {\n        default: break;\n    }\n' + text[m.end():]
            text = text.replace('switch (ctx->pc) {', 'switch (ctx->pc) {' + ''.join(
                f'\n        case 0x{pc:x}u: goto original_entry_{pc:x};' for pc in extra), 1)
            for pc in extra:
                marker = f'    // 0x{pc:x}:'
                if marker not in text:
                    raise ValueError(f'{path}: no instruction marker for 0x{pc:x}')
                text = text.replace(marker, f'original_entry_{pc:x}:\n' + marker, 1)
        out = folder / ('oracle-' + Path(path).name)
        if '// Address' not in text:
            out.write_text(text)
        else:
            write_scalar_fp_oracle(path, out, source=text)
        sources.append(str(out))
        decls.append(f'void {name}(uint8_t*,R5900Context*,PS2Runtime*);')
        target = name
        if name == 'sub_0032CDB0_0x32cdb0':  # 32CDB0 re-enters its own continuation (see world_collision_live)
            decls.append('static void liveTreeLoop(uint8_t* m,R5900Context* c,PS2Runtime* r){do{sub_0032CDB0_0x32cdb0(m,c,r);}while(c->pc==0x32cf34);}')
            target = 'liveTreeLoop'
        for pc in sorted(set(pcs) | set(cases) | {start}):
            if pc in pcs:
                decls.append(f'static void liveCount_{pc:x}(uint8_t* m,R5900Context* c,PS2Runtime* r){{callCounts[0x{pc:x}]++;{target}(m,c,r);}}')
                regs.append(f'reg(0x{pc:x},liveCount_{pc:x});')
            else:
                regs.append(f'reg(0x{pc:x},{target});')
    (folder / 'live_registrations.inc').write_text(
        '#include "ps2_runtime_macros.h"\n#include "ps2_runtime.h"\n#include <map>\n#include <stdexcept>\nstatic std::map<uint32_t,uint64_t> callCounts;\n' + '\n'.join(decls) +
        '\nstatic void registerLiveOriginals(PS2Runtime& runtime){auto reg=[&](uint32_t pc,PS2Runtime::RecompiledFunction fn){'
        'if(!runtime.registerFunction(pc,fn))throw std::runtime_error("live registration");};\n' + '\n'.join(regs) + '\n}\n')
    vendor, recomp = ROOT / 'local/vendor/PS2Recomp', ROOT / 'build/ps2recomp'
    includes = [vendor / 'ps2xRuntime/include', vendor / 'ps2xRuntime/src/lib/Kernel', vendor / 'ps2xIOP/include',
                recomp / '_deps/sse2neon-src', OUTPUT, folder, ROOT / 'tests']
    command = ['xcrun', 'clang++', '-std=c++20', '-O1', '-arch', 'arm64', '-DUSE_SSE2NEON', '-frounding-math', '-ffp-contract=off']
    command += ['-D' + d for d in defines] + ['-I' + str(p) for p in includes]
    command += [str(main_source)] + [str(s) for s in extra_sources] + sources
    command += [str(recomp / 'ps2xRuntime/libps2_runtime.a'), str(recomp / '_deps/raylib-build/raylib/libraylib.a'), str(recomp / 'ps2xIOP/libps2_iop.a')]
    for framework in ['Foundation', 'OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']:
        command += ['-framework', framework]
    command += ['-o', str(binary)]
    cached_oracle_build(command, cache or (ROOT / 'build/original-live-objects'))
    return need
