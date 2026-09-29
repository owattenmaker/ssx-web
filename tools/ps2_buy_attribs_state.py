#!/usr/bin/env python3
"""Read the lodge's Buy Attributes state from ARMSX2 savestates (docs/career-events.md "Buy Attributes (cFEStateBuyAttrib)").

  python3 tools/ps2_buy_attribs_state.py STATE.p2s [STATE.p2s ...] [--char 4]

Per state: the rider's profile record (profile 0 at 0x4A6CA8 + char*0xF88: cash +0xAC4, the seven bytes +0xBDF in the original
order speed, accel, tricks, edging, spin, toughness, stability), the runtime bank 0x535538 + char*7, and when the screen is open
the cFEStateBuyAttrib object (vtable 0x473908 at +8, state id 0x29 at +0xC): raw[7] +0x4C, level[7] +0x68, pending[7] +0x84
(menu row order, 0x4780B0), total +0xA0, bank +0xA4, and the buy popup (cUIStateBuyPopup, vtable 0x46CB08, id 7): cost +0x48,
you have +0x4C, Yes +0x6C. Development reference only; states are read, never written.
"""
import argparse, json, struct, zipfile


def memory(path):
    with zipfile.ZipFile(path) as z: return z.read('eeMemory.bin')


def objects(m, vtable, ident):
    out, pat, i = [], struct.pack('<I', vtable), 0
    while (i := m.find(pat, i)) >= 0:
        a = i - 8
        if i % 4 == 0 and 0x100000 < a < 0x2000000 and struct.unpack_from('<I', m, a + 0xC)[0] == ident: out.append(a)
        i += 4
    return out


def read(path, char=4):
    m = memory(path); rider = 0x4A6CA8 + char * 0xF88
    r = dict(cash=struct.unpack_from('<i', m, rider + 0xAC4)[0], profile=list(struct.unpack_from('<7b', m, rider + 0xBDF)),
             runtime_bank=list(struct.unpack_from('<7b', m, 0x535538 + char * 7)))
    for a in objects(m, 0x473908, 0x29):
        g = lambda o, n=1: list(struct.unpack_from('<%di' % n, m, a + o))
        r['screen'] = dict(at=hex(a), raw=g(0x4C, 7), level=g(0x68, 7), pending=g(0x84, 7), total=g(0xA0)[0], bank=g(0xA4)[0])
    for a in objects(m, 0x46CB08, 7):
        g = lambda o: struct.unpack_from('<i', m, a + o)[0]
        r.setdefault('popup', []).append(dict(at=hex(a), cost=g(0x48), have=g(0x4C), yes=g(0x6C)))
    return r


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('states', nargs='+'); p.add_argument('--char', type=int, default=4, help='CHARDB index (4 = Zoe)')
    a = p.parse_args()
    for s in a.states: print(s, json.dumps(read(s, a.char)))
