#!/usr/bin/env python3
"""Freestyle rosters and posted scores of PS2 savestates (docs/slopestyle-bigair.md "Opponent and posted riders").

The freestyle handler init 0x238E20 draws its roster 0x239938 (0x23C770 shuffle) and the posted scores 0x239AA0 ->
0x1453D0 from the roster generator 0x4C9548. For every freestyle savestate this writes what web/career.js must
reproduce: the roster seed and the number of draws since (0x4C9548 stepped back to its seeding), the event (0x535C08),
Single Event flag 0x535C11, game mode 0x535C12, the player's character and freestyle level (profile 0x4A6CA8 +
char*0xF88 + 0x284, 0x147CB8), GMM+0x18.. (player, +0x1C, the shuffled characters), the computer riders spawned
(0x535C04), 0x536640 and the handler's three posted rounds (handler 0x57A600 + 4 + round*0x28 + slot*4).

  python3 tools/export_freestyle_rosters.py      -> web/public/test-data/CAREER/freestyle-rosters.json (web/test-slopestyle-bigair.mjs)
"""
import json, struct, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from export_lineups import ROOT, GP, memory_of, seed_of  # noqa: E402

HANDLER = 0x57A600   # the freestyle handler object (vtable 0x47CFA0) in every state
STATES = [
    # Quick Play anchors (boot seed 0x182200, roster seed = presentation draw 130)
    'local/reference/pcsx2/r-and-b-countdown-anchor.p2s', 'local/reference/pcsx2/crows-nest-countdown-anchor.p2s',
    'local/reference/pcsx2/the-junction-countdown-anchor.p2s',
    # Conquer the Mountain heat 1 (Zoe, fresh career: the world load seeded the generator, this is its first event)
    'local/ps2-capture/menus/rnbctm/zoe-a.f03400.p2s', 'local/ps2-capture/menus/pipegu/nav2.final.p2s',
]
GLOBS = ['local/reference/pcsx2/characters/lineups-ASS1/*/countdown.p2s', 'local/ps2-capture/menus/rnbctm/level*.f*.p2s']


def facts(path):
    m = memory_of(path)
    u = lambda a: struct.unpack_from('<I', m, a)[0]; i32 = lambda a: struct.unpack_from('<i', m, a)[0]
    h = lambda a: struct.unpack_from('<h', m, a)[0]
    gmm = u(GP - 0x480)
    if u(HANDLER) != 0x47CFA0: raise ValueError(f'{path}: no freestyle handler at 0x57A600')
    seed, draws = seed_of([u(0x4C9548 + 4 * k) for k in range(6)], 100000)
    player = i32(gmm + 0x18)
    return dict(state=str(Path(path).relative_to(ROOT)), course=i32(0x535C08), single=m[0x535C11], mode=m[0x535C12],
                roster_seed=seed, roster_draws=draws, player=player, level=h(0x4A6CA8 + player * 0xF88 + 0x284),
                gmm_characters=[i32(gmm + 0x18 + 4 * k) for k in range(10)], posted_count=i32(gmm + 0x14), slots=i32(gmm + 0x10),
                computer_riders=u(0x535C04), shown=[i32(0x536640 + 4 * k) for k in range(6)],
                rounds=[[i32(HANDLER + 4 + r * 0x28 + 4 * k) for k in range(6)] for r in range(3)])


def main():
    paths = [ROOT / p for p in STATES if (ROOT / p).exists()]
    for g in GLOBS: paths += sorted(ROOT.glob(g))
    out = [facts(p) for p in paths]
    target = ROOT / 'web/public/test-data/CAREER/freestyle-rosters.json'; target.parent.mkdir(parents=True, exist_ok=True)   # test data
    target.write_text(json.dumps(dict(provenance='tools/export_freestyle_rosters.py', states=out), indent=0) + '\n')
    for f in out: print(f['state'], f['course'], 'single' if f['single'] else 'career', hex(f['roster_seed']), f['roster_draws'], f['gmm_characters'][:6], f['rounds'][0])


if __name__ == '__main__':
    main()
