#!/usr/bin/env python3
"""Build/run the development-only instruction oracle for the LUN stage-script VM (docs/stage-scripts.md).

Runs the recompiled interpreter 0x2227D0 (host entry 0x2224B8, nested frames 0x222648, hash tables and
value helpers) on every stage program of the three courses and their resident extra locations
(tools/export_stage_scripts.py), for every function record, with randomized globals and builtin results
(builtin table 0x441F38 redirected to a recording stub), plus randomized synthetic programs covering all
43 opcodes, and compares engine/stage_script_vm.hpp: builtin call trace, return value, top-level register
file, and every table's refcount/buckets/chains.

The recompiled copies get the PCSX2 EE scalar FP correction (tools/original_fp_oracle.py); the interpreter's
DIV.S additionally applies the EE zero-exponent divisor rule (sign(a^b)|0x7F7FFFFF) that the port models.
"""
from pathlib import Path
import hashlib, json, re, struct, subprocess, sys
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
import export_stage_scripts

ROOT = Path(__file__).resolve().parents[1]
FUNCTIONS = ['002227D0', '002224A0', '002224A8', '002224B8', '00222648', '00224DA0', '00224E50', '00224DF0',
             '00224F30', '00224D00', '00225068', '00225248', '00225338', '00225B90', '00226600', '00226610',
             '00226618', '00226620', '00226628']
EE_DIVIDE = '''
static float stageScriptOracleDivide(float a,float b){
    const uint32_t x=std::bit_cast<uint32_t>(a),y=std::bit_cast<uint32_t>(b);
    if((y&0x7F800000u)==0)return std::bit_cast<float>(((x^y)&0x80000000u)|0x7F7FFFFFu);
    return originalOracleScalarDivide(a,b);
}
'''


def stage_programs():
    """[(course index, track, program index, words)] for every stage of every course event world."""
    stages, locs = export_stage_scripts.stage_records()
    out = []
    for ci, course in enumerate(export_stage_scripts.COURSES):
        exported = export_stage_scripts.export(course, stages, locs)
        stage_rows, programs, words = exported[0], exported[1], exported[2]
        for track, first, count, _, _ in stage_rows:
            for i in range(count):
                start, length = programs[first + i]
                out.append((ci, track, i, words[start:start + length]))
    return out


def check_course_json(programs):
    """The extracted course programs equal the earlier scripts.json extraction (code + trailer words)."""
    sources = {8: ROOT / 'local/browser-pickups/ara1-scripts.json', 16: ROOT / 'local/browser-pickups/BRA2/scripts.json',
               15: ROOT / 'local/browser-pickups/BHP1/scripts.json'}
    for track, path in sources.items():
        if not path.exists():
            continue
        listed = json.loads(path.read_text())['programs']
        mine = [w for _, t, _, w in programs if t == track]
        assert len(mine) == len(listed), (track, len(mine), len(listed))
        for words, p in zip(mine, listed):
            code_end = words[1] // 4
            assert words[4:code_end] == p['code_words'] and words[code_end:] == p['trailer_words'], (track, p['index'])


def main():
    synthetic = int(sys.argv[1]) if len(sys.argv) > 1 else 100000
    vendor = ROOT / 'local/vendor/PS2Recomp'; build = ROOT / 'build/ps2recomp'
    elf = ROOT / 'local/disc/SLUS_207.72'
    if hashlib.sha1(elf.read_bytes()).hexdigest() != EXPECTED_SHA1:
        raise ValueError('Unexpected original SSX3 executable')
    programs = stage_programs()
    check_course_json(programs)
    work = ROOT / 'local/reference/stage-script-vm'; work.mkdir(parents=True, exist_ok=True)
    blob = bytearray(struct.pack('<I', len(programs)))
    for ci, track, index, words in programs:
        blob += struct.pack(f'<4I{len(words)}I', ci, track, index, len(words), *words)
    (work / 'programs.bin').write_bytes(bytes(blob))

    includes = [vendor/'ps2xRuntime/include', vendor/'ps2xRuntime/src/lib/Kernel', vendor/'ps2xIOP/include', build/'_deps/sse2neon-src', ROOT/'local/output']
    command = ['xcrun', 'clang++', '-std=c++20', '-O2', '-arch', 'arm64', '-DUSE_SSE2NEON', '-frounding-math', '-ffp-contract=off'] + ['-I' + str(p) for p in includes]
    command += [str(ROOT / 'tests/stage_script_vm_reference.cpp')]
    for prefix in FUNCTIONS:
        path = next((ROOT / 'local/output').glob('sub_' + prefix + '*'))
        out = write_scalar_fp_oracle(path, work / ('vm-' + path.name), path.read_text())
        if prefix == '002227D0':
            text = out.read_text()
            text, n = re.subn(r'= originalOracleScalarDivide\(', '= stageScriptOracleDivide(', text)
            if n != 1:
                raise ValueError(f'Expected one DIV.S in 0x2227D0, found {n}')
            marker = '#pragma STDC FENV_ACCESS ON\n'
            text = text.replace(marker, marker + EE_DIVIDE.replace('static float stageScriptOracleDivide', 'static float originalOracleScalarDivide(float,float);\nstatic float stageScriptOracleDivide', 1), 1)
            out.write_text(text)
        command.append(str(out))
    command += [str(build/'ps2xRuntime/libps2_runtime.a'), str(build/'_deps/raylib-build/raylib/libraylib.a'), str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ('OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation'):
        command += ['-framework', framework]
    binary = ROOT / 'build/ssx3_stage_script_vm_reference'; command += ['-o', str(binary)]
    cached_oracle_build(command, ROOT / 'build/original-stage-script-vm-objects')
    run = subprocess.run([str(binary), str(elf), str(work / 'programs.bin'), str(synthetic)], check=False, timeout=3600, capture_output=True, text=True)
    print(run.stdout, end=''); print(run.stderr, end='')
    if 'missing-target' in run.stdout or 'missing-target' in run.stderr or 'Missing' in run.stderr:
        raise RuntimeError('Incomplete original stage-script VM call graph')
    run.check_returncode()


if __name__ == '__main__':
    main()
