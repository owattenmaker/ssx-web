"""Disassemble VU0 micro code from a savestate's vu0MicroMem.bin (for reading the micro programs the oracle runs).

usage: vudis.py vu0MicroMem.bin START_BYTE [COUNT]
"""
import struct
import sys

LANES = 'xyzw'
BC = 'xyzw'
UPPER1 = {}
for k, name in enumerate(['add', 'sub', 'madd', 'msub', 'max', 'mini', 'mul']):
    for b in range(4):
        UPPER1[k * 4 + b] = name + BC[b]
UPPER1.update({0x1C: 'mulq', 0x1D: 'maxi', 0x1E: 'muli', 0x1F: 'minii', 0x20: 'addq', 0x21: 'maddq', 0x22: 'addi', 0x23: 'maddi',
               0x24: 'subq', 0x25: 'msubq', 0x26: 'subi', 0x27: 'msubi', 0x28: 'add', 0x29: 'madd', 0x2A: 'mul', 0x2B: 'max', 0x2C: 'sub',
               0x2D: 'msub', 0x2E: 'opmsub', 0x2F: 'mini'})
SPECIAL2 = {}
for k, name in enumerate(['adda', 'suba', 'madda', 'msuba']):
    for b in range(4):
        SPECIAL2[k * 4 + b] = name + BC[b]
SPECIAL2.update({0x10: 'itof0', 0x11: 'itof4', 0x12: 'itof12', 0x13: 'itof15', 0x14: 'ftoi0', 0x15: 'ftoi4', 0x16: 'ftoi12', 0x17: 'ftoi15',
                 0x18: 'mulax', 0x19: 'mulay', 0x1A: 'mulaz', 0x1B: 'mulaw', 0x1C: 'mulaq', 0x1D: 'abs', 0x1E: 'mulai', 0x1F: 'clipw',
                 0x20: 'addaq', 0x21: 'maddaq', 0x22: 'addai', 0x23: 'maddai', 0x24: 'subaq', 0x25: 'msubaq', 0x26: 'subai', 0x27: 'msubai',
                 0x28: 'adda', 0x29: 'madda', 0x2A: 'mula', 0x2C: 'suba', 0x2D: 'msuba', 0x2E: 'opmula', 0x2F: 'nop',
                 0x30: 'move', 0x31: 'mr32', 0x34: 'lqi', 0x35: 'sqi', 0x36: 'lqd', 0x37: 'sqd', 0x38: 'div', 0x39: 'sqrt', 0x3A: 'rsqrt',
                 0x3B: 'waitq', 0x3C: 'mtir', 0x3D: 'mfir', 0x3E: 'ilwr', 0x3F: 'iswr', 0x40: 'rnext', 0x41: 'rget', 0x42: 'rinit', 0x43: 'rxor'})
LOWER = {0x00: 'lq', 0x01: 'sq', 0x04: 'ilw', 0x05: 'isw', 0x08: 'iaddiu', 0x09: 'isubiu', 0x10: 'fceq', 0x11: 'fcset', 0x12: 'fcand',
         0x13: 'fcor', 0x14: 'fseq', 0x15: 'fsset', 0x16: 'fsand', 0x17: 'fsor', 0x18: 'fmeq', 0x1A: 'fmand', 0x1B: 'fmor', 0x1C: 'fcget',
         0x20: 'b', 0x21: 'bal', 0x24: 'jr', 0x25: 'jalr', 0x28: 'ibeq', 0x29: 'ibne', 0x2C: 'ibltz', 0x2D: 'ibgtz', 0x2E: 'iblez', 0x2F: 'ibgez'}
INTEGER = {0x30: 'iadd', 0x31: 'isub', 0x32: 'iaddi', 0x34: 'iand', 0x35: 'ior'}


def dest(word):
    d = (word >> 21) & 15
    return ''.join(LANES[k] for k in range(4) if d & (8 >> k))


def upper(word):
    code = word & 0x07FFFFFF
    ft, fs, fd, funct = (code >> 16) & 31, (code >> 11) & 31, (code >> 6) & 31, code & 63
    flags = ''.join(f for bit, f in ((31, 'I'), (30, 'E'), (29, 'M'), (28, 'D'), (27, 'T')) if word >> bit & 1)
    if funct < 0x30:
        return f'{UPPER1.get(funct, "?")}.{dest(code)} vf{fd}, vf{fs}, vf{ft}', flags
    index = (code & 3) | ((code >> 4) & 0x7C)
    name = SPECIAL2.get(index, f'?{index:#x}')
    if name == 'nop':
        return 'nop', flags
    return f'{name}.{dest(code)} vf{ft}/acc, vf{fs}, vf{ft}', flags


def lower(word):
    op = word >> 25
    it, is_, dst = (word >> 16) & 31, (word >> 11) & 31, dest(word)
    imm11 = ((word & 0x7FF) ^ 0x400) - 0x400
    if op == 0x40:
        funct = word & 63
        if funct in INTEGER:
            return f'{INTEGER[funct]} vi{(word >> 6) & 15}, vi{is_ & 15}, vi{it & 15}'
        index = (word & 3) | ((word >> 4) & 0x7C)
        fsf, ftf = (word >> 21) & 3, (word >> 23) & 3
        name = SPECIAL2.get(index, f'?{index:#x}')
        if name in ('div', 'rsqrt'):
            return f'{name} Q, vf{is_}{LANES[fsf]}, vf{it}{LANES[ftf]}'
        if name == 'sqrt':
            return f'sqrt Q, vf{it}{LANES[ftf]}'
        return f'{name}.{dst} vf{it}, vf{is_}'
    name = LOWER.get(op, f'?{op:#x}')
    if name in ('ibeq', 'ibne', 'ibltz', 'ibgtz', 'iblez', 'ibgez', 'b', 'bal'):
        return f'{name} vi{it & 15}, vi{is_ & 15}, {imm11:+d}'
    if name in ('fsand', 'fseq', 'fsor'):
        return f'{name} vi{it & 15}, {((word >> 10) & 0x800) | (word & 0x7FF):#x}'
    if name in ('fcand', 'fceq', 'fcor', 'fcset'):
        return f'{name} vi1, {word & 0xFFFFFF:#x}'
    return f'{name}.{dst} vi/vf{it}, {imm11}(vi{is_ & 15})'


def main():
    memory = open(sys.argv[1], 'rb').read()
    start = int(sys.argv[2], 0)
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 64
    for k in range(count):
        address = start + 8 * k
        lo, hi = struct.unpack_from('<II', memory, address)
        text, flags = upper(hi)
        low = f'loi {struct.unpack("<f", struct.pack("<I", lo))[0]:g}' if hi >> 31 else lower(lo)
        print(f'{address:#06x} {flags:2} {text:40} {low}')
        if hi >> 30 & 1:
            lo, hi = struct.unpack_from('<II', memory, address + 8)
            print(f'{address + 8:#06x}    {upper(hi)[0]:40} {lower(lo)}')
            break


if __name__ == '__main__':
    main()
