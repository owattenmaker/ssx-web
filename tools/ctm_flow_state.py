#!/usr/bin/env python3
"""Conquer the Mountain flow facts from an EE memory image (the eeMemory.bin of a .p2s), for tools/ctm_flow_capture.py.

SLUS_207.72, gp = 0x4A30F0 (docs/ctm-parity.md):
  G = [0x4A28A8]; world S = [G+0x84]; world state S+0x214; race C = S+0xC (tick C+8, riders C+0x28[], count C+0x78).
  settings 0x535BC8: +0x40 current course (0x535C08), +0x48 event type, +0x49 game type (0 = CTM), +0x4A game mode (12 free ride).
  NIS manager [0x4A28A4]: list players [se+0x54C] + k*0xCC (5-step FIFO {list, script, flags, req, D, E, F}), requests se+0x2C0.
  Career block of player 0 (profile 0): 0x4A6CA8 + char*0xF88 with char = byte 0x534FE0+0x11 (145C38):
    +0 new-career flag, +0x278 lock word, +0x27C last lodge, +0xAC4 cash, +0xAC8 earned, +0xACC visited mask,
    +0x280..0x286 race/freestyle levels, +0xE38 inbox (25 x {item, variant}, +0xC8 read bits, +0xCC count).

  python3 tools/ctm_flow_state.py STATE.p2s [...]
"""
import json, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHAR = ['Moby', 'Kaori', 'Allegra', 'Mac', 'Zoe', 'Griff', 'Elise', 'Nate', 'Psymon', 'Viggo']
PROFILE, CHAR_BLOCK = 0x4A6CA8, 0xF88


def script_names():
    try:
        index = json.loads((ROOT / 'web/public/assets/CUTSCENES/index.json').read_text())
        return {s['number']: s['name'] for s in index['scripts']}
    except (OSError, ValueError, KeyError):
        return {}


NAMES = script_names()


def flow_state(m):
    u = lambda a: struct.unpack_from('<I', m, a & 0x1FFFFFF)[0]
    i = lambda a: struct.unpack_from('<i', m, a & 0x1FFFFFF)[0]
    f = lambda a: struct.unpack_from('<f', m, a & 0x1FFFFFF)[0]
    ok = lambda p: 0x100000 <= p < 0x2000000
    out = {}
    G = u(0x4A28A8); S = u(G + 0x84) if ok(G) else 0; C = u(S + 0xC) if ok(S) else 0
    out['tick'] = u(C + 8) if ok(C) else None
    if ok(S): out['ws'] = i(S + 0x214)
    out['course'] = i(0x535C08); out['etype'] = m[0x535C10]; out['gtype'] = m[0x535C11]; out['mode'] = m[0x535C12]
    ev = u(G + 0xC0) if ok(G) else 0
    if ok(ev): out['round'] = i(ev); out['next_round'] = i(ev + 0x70); out['ev98'] = i(ev + 0x98); out['ev9c'] = i(ev + 0x9C)
    ci = m[0x534FE0 + 0x11]
    if ci < 10:
        b = PROFILE + ci * CHAR_BLOCK
        box = b + 0xE38; count = max(0, min(25, i(box + 0xCC)))
        out['career'] = dict(char=CHAR[ci], new=i(b), lodge=i(b + 0x27C), locks=hex(struct.unpack_from('<Q', m, b + 0x278)[0]),
                             visited=hex(u(b + 0xACC)), cash=i(b + 0xAC4), earned=i(b + 0xAC8),
                             levels=[struct.unpack_from('<h', m, b + o)[0] for o in (0x280, 0x282, 0x284, 0x286)],
                             inbox=[list(struct.unpack_from('<ii', m, box + 8 * k)) for k in range(count)], unread=hex(u(box + 0xC8)))
    se = u(0x4A28A4)
    if ok(se):
        lists = []; base = u(se + 0x54C)
        if ok(base):
            for k in range(2):
                p = base + k * 0xCC
                head, tail, full = i(p + 0x98), i(p + 0x9C), i(p + 0xA0)
                n = 5 if full else (tail - head) % 5
                lists.append([dict(zip(('list', 'script', 'flags'), (i(p + 0xC + ((head + j) % 5) * 0x1C + o) for o in (0, 4, 8)))) for j in range(n)])
        out['lists'] = lists
        playing = []
        for k in range(16):
            r = se + 0x2C0 + k * 0x24; slot, script = u(r), i(r + 8)
            if ok(slot) and 0 <= script < 167 and u(slot + 0x20) == 3: playing.append([script, round(f(slot + 0x28), 2), NAMES.get(script, '')])
        out['playing'] = playing
    return out


def brief(s):
    return f"tick={s.get('tick')} ws={s.get('ws')} course={s.get('course')} playing={[p[0] for p in s.get('playing', [])]}"


if __name__ == '__main__':
    for fn in sys.argv[1:]:
        print(fn); print(json.dumps(flow_state(zipfile.ZipFile(fn).read('eeMemory.bin')), indent=1))
