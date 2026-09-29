#!/usr/bin/env python3
"""RollerModifier world-query instruction oracle (engine/roller_world_query.hpp).

Builds tests/roller_world_query_reference.cpp against PCSX2 scalar-FP oracle
copies of every original reachable from 0x330788/0x330828/0x32C0F8/0x335960/
0x336850/0x334888 (plus the 0x48E5F0 query vtable) and runs the randomized
comparison on synthetic memory laid over the carve-bag tick-607 savestate
(gp constants and the VU0 0x6E0 microprogram). Log: local/reference/roller-world-query/.
"""
import json, re, struct, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import original_live_build  # noqa: E402
from original_fp_oracle import write_scalar_fp_oracle  # noqa: E402
from original_live_build import vtable_targets  # noqa: E402

# PCSX2 zero handling for the Q unit and DIV.S (VUops.cpp _vuDIV/_vuSQRT/_vuRSQRT after
# vuDouble; FPU.cpp checkDivideByZero): an operand with a zero exponent gives +-FLT_MAX,
# VSQRT takes |x|. The recompiled lowering returns 0 (VDIV/VRSQRT/VSQRT) or inf (DIV.S).
# Applied to the generated oracle copies only; engine/roller_world_query.hpp follows it.
ZERO_POLICY = """
static inline bool pcsx2ZeroExp(float x){return (std::bit_cast<uint32_t>(x)&0x7f800000u)==0;}
static inline float pcsx2Max(uint32_t sign){return std::bit_cast<float>((sign&0x80000000u)|0x7f7fffffu);}
"""
ZERO_RULES = [
    (r'ctx->vu0_q = sqrtf\(std::max\(0\.0f, ft\)\);', 'ctx->vu0_q = pcsx2ZeroExp(ft) ? 0.0f : sqrtf(fabsf(ft));'),
    (r'ctx->vu0_q = \(ft != 0\.0f\) \? \(fs / ft\) : 0\.0f;',
     'ctx->vu0_q = pcsx2ZeroExp(ft) ? pcsx2Max(std::bit_cast<uint32_t>(ft)^std::bit_cast<uint32_t>(fs)) : (fs / ft);'),
    (r'ctx->vu0_q = \(ft > 0\.0f\) \? \(1\.0f / sqrtf\(ft\)\) : 0\.0f;',
     'ctx->vu0_q = pcsx2ZeroExp(ft) ? pcsx2Max(std::bit_cast<uint32_t>(ft)) : (1.0f / sqrtf(fabsf(ft)));'),
    (r'if \(ctx->f\[(\d+)\] == 0\.0f\) \{ ctx->fcr31 \|= 0x100000; /\* DZ flag \*/ ctx->f\[(\d+)\] = copysignf\(INFINITY, ctx->f\[(\d+)\] \* 0\.0f\); \}',
     r'if (pcsx2ZeroExp(ctx->f[\1])) { ctx->fcr31 |= 0x100000; ctx->f[\2] = pcsx2Max(std::bit_cast<uint32_t>(ctx->f[\1])^std::bit_cast<uint32_t>(ctx->f[\3])); }'),
]


def write_pcsx2_oracle(original, output, source=None):
    path = write_scalar_fp_oracle(original, output, source=source)
    text = path.read_text()
    counts = []
    for pattern, replacement in ZERO_RULES:
        text, n = re.subn(pattern, replacement, text)
        counts.append(n)
    if any(counts):
        marker = '#pragma STDC FENV_ACCESS ON\n'
        text = text.replace(marker, marker + ZERO_POLICY, 1)
    path.write_text(text)
    return path


def build_live(*args, **kwargs):
    original_live_build.write_scalar_fp_oracle = write_pcsx2_oracle
    return original_live_build.build_live(*args, **kwargs)

FOLDER = ROOT / 'local/reference/roller-world-query'
STATE = ROOT / 'local/ps2-capture/runs/bag/carve-bag.tick607.p2s'


def savestate(folder, state=STATE, name='607'):
    folder.mkdir(parents=True, exist_ok=True)
    paths = [folder / f'{name}.ee', folder / f'{name}.vuc', folder / f'{name}.vud']
    with zipfile.ZipFile(state) as z:
        for path, member in zip(paths, ['eeMemory.bin', 'vu0MicroMem.bin', 'vu0Memory.bin']):
            path.write_bytes(z.read(member))
    return paths


def bag_tree(folder):
    world = json.loads((ROOT / 'local/assets/native/ARA1/world_collision.json').read_text())
    model = world['collision_meshes']['8:170']['models'][0]  # mdl_ARA1_crashbag collision resource 0xAA08
    data = struct.pack('<I3f', len(model['levels']), *model['center_cm'])
    for level in model['levels']:
        data += struct.pack('<ffI', level['radius_cm'], level['child_offset_cm'], level['stride'])
    data += struct.pack('<I', len(model['masks'])) + bytes(model['masks'])
    path = folder / 'bag-tree.bin'
    path.write_bytes(data)
    return path


def main():
    ee, vuc, vud = savestate(FOLDER)
    tree = bag_tree(FOLDER)
    roots = [0x330788, 0x330828, 0x32C0F8, 0x335960, 0x336850, 0x334888, 0x330540, 0x330710, 0x330778, 0x3306D8]
    roots += vtable_targets([(0x48E5F0, 0x80)])
    binary = ROOT / 'build/ssx3_roller_world_query_reference'
    need = build_live(roots, ROOT / 'tests/roller_world_query_reference.cpp', binary, FOLDER / 'oracle',
                      cache=ROOT / 'build/roller-world-query-objects')
    run = subprocess.run([str(binary), str(ee), str(vuc), str(vud), str(tree)], capture_output=True, text=True,
                         timeout=3600, env={'COUNTS': '1'})
    log = run.stdout + run.stderr
    (FOLDER / 'reference.log').write_text(f'{len(need)} original entries linked\n' + log)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr, end='')
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
