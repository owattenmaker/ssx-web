#!/usr/bin/env python3
"""Camera log capture for the booth teleports: the injection state of inject_booth.py plus entry logs at 0x166C60 (base
set-target) and 0x1624E8 (the algorithm's frame step: pending reset -> vt+0x24, update, finish), on ticks in two windows.
Entry (64 bytes): tick | site << 24, a0, $ra, a0+0x100/0x104 (velocity filter), a0+0x1B0/0x1B4 (last velocity), a0+0x2F0
(reset pending), a0+0x190/0x194, human +0x1E0..0x1E8, human +0x438.
    python3 tools/ps2_booth_camlog.py OUT.p2s PREFIX NEUTRAL LO1:HI1 LO2:HI2 --inject T:RES ...
"""
import json, struct, sys, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_booth_inject as ib, ps2_capture as cap
from reference_replay import patch_state

SITES = [(0x166C60, struct.pack('<2I', 0x27BDFF70, 0x7FB00070)), (0x1624E8, struct.pack('<2I', 0x27BDFFD0, 0x7FB00020))]
CODE, CTL, MAX = 0xE8000, 0xE9000, 80


def assemble():
    T0, T1, T3, T4, T5, T6, T7, T8, T9 = cap.T0, cap.T1, cap.T3, cap.T4, cap.T5, cap.T6, cap.T7, cap.T8, cap.T9
    a = cap.Asm(CODE)
    a.label('common')
    a.li(T0, CTL); cap.tick_into(a, T3)
    for w in (0, 8):   # two windows [lo, hi]
        a.lw(T1, w, T0); a.sltu(T4, T3, T1); a.bne(T4, cap.ZERO, f'no{w}'); a.nop()
        a.lw(T1, w + 4, T0); a.sltu(T4, T1, T3); a.beq(T4, cap.ZERO, 'log'); a.nop()
        a.label(f'no{w}')
    a.beq(cap.ZERO, cap.ZERO, 'done'); a.nop()
    a.label('log')
    a.lw(T5, 16, T0); a.li(T6, MAX); a.sltu(T4, T5, T6); a.beq(T4, cap.ZERO, 'done'); a.nop()
    a.sll(T7, T5, 6); a.addu(T7, T7, T0); a.addiu(T7, T7, 32)
    a.sll(T4, T8, 24); a.addu(T4, T4, T3); a.sw(T4, 0, T7); a.sw(cap.A0, 4, T7); a.sw(31, 8, T7)
    for k, off in enumerate((0x100, 0x104, 0x1B0, 0x1B4, 0x2F0, 0x190, 0x194)): a.lw(T4, off, cap.A0); a.sw(T4, 12 + 4 * k, T7)
    a.li(T6, cap.DATA); a.lw(T6, cap.F_RIDER, T6)
    for k, off in enumerate((0x1E0, 0x1E4, 0x1E8, 0x438)): a.lw(T4, off, T6); a.sw(T4, 40 + 4 * k, T7)
    a.addiu(T5, T5, 1); a.sw(T5, 16, T0)
    a.label('done')
    a.emit((T9 << 21) | 0x08); a.nop()   # jr t9
    entries = []
    for k, (fn, words) in enumerate(SITES):
        entries.append(a.here())
        a.li(T8, k + 1); a.li(T9, a.here() + 16); a.j(CODE); a.nop()   # li is 2 words: back = here after li(T9) + j + nop
        w0, w1 = struct.unpack('<2I', words); a.emit(w0); a.emit(w1); a.j(fn + 8); a.nop()
    return a.link(), entries


def main():
    out, prefix, neutral, w1, w2 = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5]
    injects = [x for k, x in enumerate(sys.argv) if k and sys.argv[k - 1] == '--inject']
    staged = Path(out).with_name(Path(out).stem + '.pre.p2s')
    sys.argv = ['ps2_booth_inject.py', 'build', str(staged), prefix, neutral] + sum((['--inject', x] for x in injects), [])
    ib.cap.build = (lambda orig: (lambda b, s, o, **k: orig(b, s, o, **dict(k, watches=[(CTL, 32 + 64 * MAX)]))))(ib.cap.build)
    ib.main()
    mem = zipfile.ZipFile(staged).read('eeMemory.bin')
    code, entries = assemble()
    lo1, hi1 = (int(v) for v in w1.split(':')); lo2, hi2 = (int(v) for v in w2.split(':'))
    ctl = struct.pack('<8I', lo1, hi1, lo2, hi2, 0, 0, 0, 0)
    patches = [dict(address=hex(CODE), expected=mem[CODE:CODE + len(code)].hex(), replacement=code.hex()),
               dict(address=hex(CTL), expected=mem[CTL:CTL + 32].hex(), replacement=ctl.hex())]
    if any(mem[CODE:CODE + len(code)]) or any(mem[CTL:CTL + 32 + 64 * MAX]): raise ValueError('log arena not free')
    for (fn, words), e in zip(SITES, entries):
        if mem[fn:fn + 8] != words: raise ValueError(hex(fn))
        patches.append(dict(address=hex(fn), expected=words.hex(), replacement=struct.pack('<2I', (2 << 26) | (e >> 2), 0).hex()))
    patch_state(staged, Path(out), patches)
    m = json.loads(staged.with_suffix('.capture.json').read_text()); m['camera_log'] = dict(ctl=hex(CTL), max=MAX)
    Path(out).with_suffix('.capture.json').write_text(json.dumps(m, indent=2) + '\n')
    print('ok', out)


if __name__ == '__main__':
    main()
