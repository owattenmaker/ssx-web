#!/usr/bin/env python3
"""VU micro-mode disassembler (development only): prints a VU1 program image (tools/extract_fx_microcode.py) as
'address: upper | lower', decoded with the opcode tables of PCSX2's VUops.cpp (local/vendor/pcsx2). Addresses are byte addresses in
micro memory (an instruction pair is 8 bytes; branch targets are shown the same way).
usage: tools/vudis.py program.bin [start_hex end_hex]"""
import struct, sys

DEST = lambda c: ''.join(ch for ch, bit in zip('xyzw', (24, 23, 22, 21)) if c >> bit & 1)
BC = 'xyzw'
FLAGS = [(31, 'I'), (30, 'E'), (29, 'M'), (28, 'D'), (27, 'T')]

UP = ['ADDx', 'ADDy', 'ADDz', 'ADDw', 'SUBx', 'SUBy', 'SUBz', 'SUBw', 'MADDx', 'MADDy', 'MADDz', 'MADDw', 'MSUBx', 'MSUBy', 'MSUBz', 'MSUBw',
      'MAXx', 'MAXy', 'MAXz', 'MAXw', 'MINIx', 'MINIy', 'MINIz', 'MINIw', 'MULx', 'MULy', 'MULz', 'MULw', 'MULq', 'MAXi', 'MULi', 'MINIi',
      'ADDq', 'MADDq', 'ADDi', 'MADDi', 'SUBq', 'MSUBq', 'SUBi', 'MSUBi', 'ADD', 'MADD', 'MUL', 'MAX', 'SUB', 'MSUB', 'OPMSUB', 'MINI']
FD = [
    ['ADDAx', 'SUBAx', 'MADDAx', 'MSUBAx', 'ITOF0', 'FTOI0', 'MULAx', 'MULAq', 'ADDAq', 'SUBAq', 'ADDA', 'SUBA'],
    ['ADDAy', 'SUBAy', 'MADDAy', 'MSUBAy', 'ITOF4', 'FTOI4', 'MULAy', 'ABS', 'MADDAq', 'MSUBAq', 'MADDA', 'MSUBA'],
    ['ADDAz', 'SUBAz', 'MADDAz', 'MSUBAz', 'ITOF12', 'FTOI12', 'MULAz', 'MULAi', 'ADDAi', 'SUBAi', 'MULA', 'OPMULA'],
    ['ADDAw', 'SUBAw', 'MADDAw', 'MSUBAw', 'ITOF15', 'FTOI15', 'MULAw', 'CLIP', 'MADDAi', 'MSUBAi', None, 'NOP'],
]


def upper(c):
    op = c & 0x3f; fd, fs, ft = c >> 6 & 31, c >> 11 & 31, c >> 16 & 31; d = DEST(c)
    fl = ''.join(n for b, n in FLAGS if c >> b & 1)
    fl = f' [{fl}]' if fl else ''
    if op >= 0x3c:
        sub = c >> 6 & 31; names = FD[op & 3]; name = names[sub] if sub < len(names) else None
        if name is None: return f'?upper {c:08x}{fl}'
        if name == 'NOP': return 'nop' + fl
        if name == 'CLIP': return f'clipw.xyz vf{fs}, vf{ft}w' + fl
        if name.startswith(('ITOF', 'FTOI')) or name == 'ABS': return f'{name.lower()}.{d} vf{ft}, vf{fs}' + fl
        acc = f'ACC.{d}'
        if name.endswith(('x', 'y', 'z', 'w')) and name[-1] in BC and name not in ('MULA', 'ADDA', 'SUBA', 'MADDA', 'MSUBA', 'OPMULA'):
            return f'{name.lower()}.{d} {acc}, vf{fs}, vf{ft}{name[-1]}' + fl
        if name.endswith('q'): return f'{name.lower()}.{d} {acc}, vf{fs}, Q' + fl
        if name.endswith('i') and name not in ('MULA',): return f'{name.lower()}.{d} {acc}, vf{fs}, I' + fl
        return f'{name.lower()}.{d} {acc}, vf{fs}, vf{ft}' + fl
    if op >= len(UP): return f'?upper {c:08x}{fl}'
    name = UP[op]
    if op < 0x1c: return f'{name.lower()}.{d} vf{fd}, vf{fs}, vf{ft}{name[-1]}' + fl
    if name.endswith('q'): return f'{name.lower()}.{d} vf{fd}, vf{fs}, Q' + fl
    if name.endswith('i'): return f'{name.lower()}.{d} vf{fd}, vf{fs}, I' + fl
    return f'{name.lower()}.{d} vf{fd}, vf{fs}, vf{ft}' + fl


def s11(c): v = c & 0x7ff; return v - 0x800 if v & 0x400 else v


def lower(c, pc):
    op = c >> 25; it, is_, id_ = c >> 16 & 31, c >> 11 & 31, c >> 6 & 31; d = DEST(c)
    fsf, ftf = BC[c >> 21 & 3], BC[c >> 23 & 3]
    br = lambda: pc + 8 + s11(c) * 8
    table = {0: 'lq', 1: 'sq', 4: 'ilw', 5: 'isw', 8: 'iaddiu', 9: 'isubiu', 0x10: 'fceq', 0x11: 'fcset', 0x12: 'fcand', 0x13: 'fcor', 0x14: 'fseq',
             0x15: 'fsset', 0x16: 'fsand', 0x17: 'fsor', 0x18: 'fmeq', 0x1a: 'fmand', 0x1b: 'fmor', 0x1c: 'fcget', 0x20: 'b', 0x21: 'bal', 0x24: 'jr',
             0x25: 'jalr', 0x28: 'ibeq', 0x29: 'ibne', 0x2c: 'ibltz', 0x2d: 'ibgtz', 0x2e: 'iblez', 0x2f: 'ibgez'}
    if op == 0x40:
        f = c & 0x3f
        if f < 0x3c:
            return {0x30: f'iadd vi{id_}, vi{is_}, vi{it}', 0x31: f'isub vi{id_}, vi{is_}, vi{it}', 0x32: f'iaddi vi{it}, vi{is_}, {((c >> 6 & 31) ^ 16) - 16}',
                    0x34: f'iand vi{id_}, vi{is_}, vi{it}', 0x35: f'ior vi{id_}, vi{is_}, vi{it}'}.get(f, f'?lowerop {c:08x}')
        t = [None] * 4
        t[0] = {12: 'move', 13: 'lqi', 14: 'div', 15: 'mtir', 16: 'rnext', 25: 'mfp', 26: 'xtop', 27: 'xgkick', 28: 'esadd', 29: 'eatanxy', 30: 'esqrt', 31: 'esin'}
        t[1] = {12: 'mr32', 13: 'sqi', 14: 'sqrt', 15: 'mfir', 16: 'rget', 26: 'xitop', 28: 'ersadd', 29: 'eatanxz', 30: 'ersqrt', 31: 'eatan'}
        t[2] = {13: 'lqd', 14: 'rsqrt', 15: 'ilwr', 16: 'rinit', 28: 'eleng', 29: 'esum', 30: 'ercpr', 31: 'eexp'}
        t[3] = {13: 'sqd', 14: 'waitq', 15: 'iswr', 16: 'rxor', 28: 'erleng', 30: 'waitp'}
        n = t[f & 3].get(c >> 6 & 31)
        if n is None: return f'?t3 {c:08x}'
        if n == 'nop' or (n == 'move' and it == 0 and is_ == 0): return 'nop'
        if n in ('move', 'mr32'): return f'{n}.{d} vf{it}, vf{is_}'
        if n == 'lqi': return f'lqi.{d} vf{it}, (vi{is_}++)'
        if n == 'sqi': return f'sqi.{d} vf{is_}, (vi{it}++)'
        if n == 'lqd': return f'lqd.{d} vf{it}, (--vi{is_})'
        if n == 'sqd': return f'sqd.{d} vf{is_}, (--vi{it})'
        if n in ('div', 'rsqrt'): return f'{n} Q, vf{is_}{fsf}, vf{it}{ftf}'
        if n in ('sqrt',): return f'sqrt Q, vf{it}{ftf}'
        if n == 'mtir': return f'mtir vi{it}, vf{is_}{fsf}'
        if n == 'mfir': return f'mfir.{d} vf{it}, vi{is_}'
        if n in ('ilwr', 'iswr'): return f'{n}.{d} vi{it}, (vi{is_})'
        if n in ('xgkick', 'xtop', 'xitop'): return f'{n} vi{is_ if n == "xgkick" else it}'
        if n == 'mfp': return f'mfp.{d} vf{it}, P'
        if n in ('waitq', 'waitp'): return n
        if n in ('esadd', 'ersadd', 'eleng', 'erleng', 'esum'): return f'{n} P, vf{is_}'
        return f'{n} P, vf{is_}{fsf}'
    n = table.get(op)
    if n is None: return f'?lower {c:08x}'
    if n in ('lq',): return f'lq.{d} vf{it}, {s11(c)}(vi{is_})'
    if n in ('sq',): return f'sq.{d} vf{is_}, {s11(c)}(vi{it})'
    if n in ('ilw', 'isw'): return f'{n}.{d} vi{it}, {s11(c)}(vi{is_})'
    if n in ('iaddiu', 'isubiu'): return f'{n} vi{it}, vi{is_}, {((c >> 10) & 0x7800) | (c & 0x7ff)}'
    if n in ('b', 'bal'): return f'{n} {"vi%d, " % it if n == "bal" else ""}0x{br():x}'
    if n in ('jr', 'jalr'): return f'{n} {"vi%d, " % it if n == "jalr" else ""}vi{is_}'
    if n in ('ibeq', 'ibne'): return f'{n} vi{it}, vi{is_}, 0x{br():x}'
    if n.startswith('ib'): return f'{n} vi{is_}, 0x{br():x}'
    if n in ('fcset',): return f'fcset 0x{c & 0xffffff:x}'
    if n.startswith('fc'): return f'{n} vi1, 0x{c & 0xffffff:x}' if n != 'fcget' else f'fcget vi{it}'
    if n.startswith('fs'): return f'{n} vi{it}, 0x{((c >> 10) & 0x800) | (c & 0x7ff):x}' if n != 'fsset' else f'fsset 0x{((c >> 10) & 0x800) | (c & 0x7ff):x}'
    if n.startswith('fm'): return f'{n} vi{it}, vi{is_}'
    return n


def disassemble(code, start=0, end=None):
    end = len(code) if end is None else end
    out = []
    for pc in range(start, end, 8):
        lo, hi = struct.unpack_from('<2I', code, pc)
        if lo == 0 and hi == 0: out.append(f'{pc:04x}: -'); continue
        u = upper(hi)
        l = f'loi {struct.unpack("<f", struct.pack("<I", lo))[0]!r} ({lo:08x})' if hi >> 31 & 1 else lower(lo, pc)
        out.append(f'{pc:04x}: {u:44s} | {l}')
    return out


if __name__ == '__main__':
    code = open(sys.argv[1], 'rb').read()
    a = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0; b = int(sys.argv[3], 16) if len(sys.argv) > 3 else len(code)
    print('\n'.join(disassemble(code, a, b)))
