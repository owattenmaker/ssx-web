#!/usr/bin/env python3
"""Generate engine/generated/set_piece_particle_bounds.inc (git-ignored, like web/generated: lifted from the ELF,
so it is built from your own disc and never published; web/build-core.sh runs this when it is missing): the particle emitter bounds routine 0x36D500
(kernel, bounds[8]) lifted instruction by instruction from the ELF (development generator; the
output is plain C++ over the kernel image, no guest CPU). 0x36D500 is straight-line VU0/FPU code
(2,759 instructions: corner/velocity extremes, apex times, min/max compare-stores). Each EE op keeps
its original semantics (add.s/sub.s originalScalarAdd/Subtract, mul.s chop, div.s eeDivide,
min.s, c.lt.s + bc1f/bc1tl) and each VU0 macro op its lane-wise chop arithmetic.

usage: test_set_piece_particles_lift.py [--check]   (--check: fail if the checked-in file differs)
"""
import re, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
ELF = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
GP = 0x4A30F0
START, END = 0x36D500, 0x370018
OUT = ROOT / 'engine/generated/set_piece_particle_bounds.inc'


def word(a): return struct.unpack_from('<I', ELF, a - 0xFF000)[0]


def disassemble():
    import rabbitizer
    out = []
    for pc in range(START, END, 4):
        i = rabbitizer.Instruction(word(pc), vram=pc, category=rabbitizer.InstrCategory.R5900)
        out.append((pc, i.disassemble()))
    return out


REG = {'zero': 0, 'at': 1, 'v0': 2, 'v1': 3, 'a0': 4, 'a1': 5, 'sp': 29, 'gp': 28, 'ra': 31}


def reg(name):
    name = name.strip().lstrip('$')
    if name in REG: return REG[name]
    raise ValueError(name)


def fpr(name): return int(name.strip().lstrip('$').lstrip('f'))
def vfr(name): return int(re.match(r'\$?vf(\d+)', name.strip()).group(1))


def mem(operand):
    m = re.match(r'(-?0x[0-9A-Fa-f]+|-?\d+)\((\$\w+)\)', operand.strip())
    return int(m.group(1), 0), reg(m.group(2))


def branch_target(pc, text):
    m = re.search(r'\. \+ 4 \+ \((-?0x[0-9A-Fa-f]+|-?\d+) << 2\)', text)
    return pc + 4 + (int(m.group(1), 0) << 2)


def load32(base, off):
    if base == 4: return f'k.u({off:#x})'
    if base == 5: return f'bitsf(b[{off // 4}])'
    if base == 29: return f'S[{off // 4}]'
    if base == 28: return f'{word(GP + off):#010x}u'
    raise ValueError((base, off))


def store32(base, off, value):
    if base == 5: return f'b[{off // 4}]=fbits({value});'
    if base == 29: return f'S[{off // 4}]={value};'
    raise ValueError((base, off))


def lift():
    ins = disassemble()
    targets = set()
    for pc, t in ins:
        if t.startswith('bc1f') or (t.startswith('bc1tl') and branch_target(pc, t) != pc + 8):
            targets.add(branch_target(pc, t))
    lines = []
    skip = set()

    def one(pc, t):
        op, _, rest = t.partition(' ')
        args = [x.strip() for x in rest.split(',')] if rest.strip() else []
        if op in ('nop',): return ''
        if op == 'addiu' and args[0] == '$sp': return ''
        if op == 'jr': return 'JR'
        if op == 'lwc1': off, base = mem(args[1]); return f'f[{fpr(args[0])}]=fbits({load32(base, off)});'
        if op == 'swc1':
            off, base = mem(args[1])
            return f'b[{off // 4}]=f[{fpr(args[0])}];' if base == 5 else store32(base, off, f'bitsf(f[{fpr(args[0])}])')
        if op in ('lq', 'lqc2'):
            off, base = mem(args[1]); src = [load32(base, off + 4 * k) for k in range(4)]
            if op == 'lq': return f'g[{reg(args[0])}]={{{",".join(src)}}};'
            return f'vf[{vfr(args[0])}]={{fbits({src[0]}),fbits({src[1]}),fbits({src[2]}),fbits({src[3]})}};'
        if op in ('sq', 'sqc2'):
            off, base = mem(args[1])
            if op == 'sq': vals = [f'g[{reg(args[0])}][{k}]' for k in range(4)]
            else: vals = [f'bitsf(vf[{vfr(args[0])}][{k}])' for k in range(4)]
            if base == 5: return ''.join(f'b[{off // 4 + k}]=fbits({vals[k]});' for k in range(4))
            return ''.join(store32(base, off + 4 * k, vals[k]) for k in range(4))
        if op == 'vadd.xyzw': return f'vf[{vfr(args[0])}]=vAdd(vf[{vfr(args[1])}],vf[{vfr(args[2])}]);'
        if op == 'vsub.xyzw': return f'vf[{vfr(args[0])}]=vSub(vf[{vfr(args[1])}],vf[{vfr(args[2])}]);'
        if op == 'vmulx.xyzw': return f'vf[{vfr(args[0])}]=vMulS(vf[{vfr(args[1])}],vf[{vfr(args[2])}][0]);'
        if op == 'mfc1': return f'g[{reg(args[0])}]={{bitsf(f[{fpr(args[1])}]),0u,0u,0u}};'
        if op == 'qmtc2.ni': return f'vf[{vfr(args[1])}]={{fbits(g[{reg(args[0])}][0]),fbits(g[{reg(args[0])}][1]),fbits(g[{reg(args[0])}][2]),fbits(g[{reg(args[0])}][3])}};'
        if op == 'mtc1': return f'f[{fpr(args[1])}]=fbits(g[{reg(args[0])}][0]);'
        if op == 'lui': return f'g[{reg(args[0])}]={{{(int(args[1], 0) & 0xFFFF) << 16:#010x}u,0u,0u,0u}};'
        if op == 'mov.s': return f'f[{fpr(args[0])}]=f[{fpr(args[1])}];'
        if op == 'add.s': return f'f[{fpr(args[0])}]=eeAdd(f[{fpr(args[1])}],f[{fpr(args[2])}]);'
        if op == 'sub.s': return f'f[{fpr(args[0])}]=eeSub(f[{fpr(args[1])}],f[{fpr(args[2])}]);'
        if op == 'mul.s': return f'f[{fpr(args[0])}]=eeMul(f[{fpr(args[1])}],f[{fpr(args[2])}]);'
        if op == 'div.s': return f'f[{fpr(args[0])}]=eeDivide(f[{fpr(args[1])}],f[{fpr(args[2])}]);'
        if op == 'min.s': return f'{{const float s_=f[{fpr(args[1])}],t_=f[{fpr(args[2])}];f[{fpr(args[0])}]=t_<s_?t_:s_;}}'
        if op == 'cvt.s.w': return f'f[{fpr(args[0])}]=float(int32_t(bitsf(f[{fpr(args[1])}])));'
        if op == 'c.lt.s': return f'cc=f[{fpr(args[0])}]<f[{fpr(args[1])}];'
        raise ValueError(f'{pc:#x}: {t}')

    i = 0
    while i < len(ins):
        pc, t = ins[i]
        if pc in targets: lines.append(f'L{pc:x}:;')
        op = t.split(' ')[0]
        if op in ('bc1f', 'bc1tl'):
            target = branch_target(pc, t); delay = one(*ins[i + 1])
            if op == 'bc1f':
                lines.append(f'{{const bool c_=cc;{delay}if(!c_)goto L{target:x};}}')
                if ins[i + 1][0] in targets: raise ValueError('delay slot is a target')
            else:
                if target == pc + 8: lines.append(f'if(cc){{{delay}}}')
                else: lines.append(f'if(cc){{{delay}goto L{target:x};}}')
            i += 2; continue
        s = one(pc, t)
        if s == 'JR': break
        if s: lines.append(s)
        i += 1
    for tgt in sorted(targets):
        if not any(l.startswith(f'L{tgt:x}:') for l in lines): raise ValueError(f'missing label {tgt:#x}')
    body = '\n'.join('    ' + l for l in lines)
    return f'''// Generated by tools/test_set_piece_particles_lift.py from SLUS_207.72 0x36D500..0x370014 (do not edit).
// 0x36D500(kernel, bounds): emitter culling box (min xyzw at b[0..3], max at b[4..7]) from the
// position corners, velocity extremes, force, maximum lifetime and the apex times of the source
// polynomial (-0.73 t + 0.113 t^2, t <= 2.7). Registers/stack are locals; memory = kernel image.
#pragma once
namespace ssx {{
template<class K> void originalParticleKernelBounds(K k,std::array<float,8>& b){{
    using namespace set_piece_particles;OriginalRounding rounding;
    float f[32]={{}};Vec4 vf[32]={{}};std::array<uint32_t,4> g[32]={{}};uint32_t S[0xB0/4]={{}};bool cc=false;
    (void)cc;
{body}
}}
}} // namespace ssx
'''


def main():
    text = lift()
    if '--check' in sys.argv:
        if OUT.read_text() != text: raise SystemExit(f'{OUT} differs from the generator')
        print('bounds routine up to date'); return
    OUT.parent.mkdir(exist_ok=True); OUT.write_text(text)
    print(f'wrote {OUT} ({text.count(chr(10))} lines)')


if __name__ == '__main__':
    main()
