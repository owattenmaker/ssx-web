#!/usr/bin/env python3
"""Export the original Career Highlights / career stats screen for the browser (read-only; docs/tricks-scoring.md).

Source (user's own disc, never modified): DATA/UI/FE.LUI screen 35car_stat (hash 0x08807DA4) + FE_1.SSH pages (exported
as web/public/assets/UI/FE_1-N.png by web/prepare-ui.py) and DATA/LOCALE/*.LOC.

The highlights page is filled by 0x1F5DA0 per row r = 1..3 (item p = top + r - 1, stat p / 3, tier p % 3):
  hl<r>        text  = locale "%s%d" % (0x441BA0[stat] = kT_STATStayOnRail .. kT_CMNDoXCombo, tier + 1)
  checkmark<r> shown / checkbox<r> hidden when tier < medal[stat] (0x1552B0), else the reverse
  hlsec<r>     text  = "%s Monster Trick" (0x46F2A8) with the monster name (116950 of {0, id << 27})
  hlsec<r>a    text  = the trick (116E08) when unlocked, else kT_FEUnlockMonsterTrick
  HL_arrowup shown when top > 0, HL_arrowdown when top + 3 < 24 (0x1F5DE4..0x1F5E40).
Output (git-ignored like the other UI exports): web/public/assets/UI/career-highlights.json.
"""
import hashlib, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_objects, lui_animations, shps_pages  # noqa: E402
import export_character_select as ecs  # noqa: E402
import loc_file  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = ROOT / 'web/public/assets/UI'
H = loc_file.name_hash
SCREEN = '35car_stat'
STATS = ['kT_STATStayOnRail', 'kT_STATHoldHandplt', 'kT_STATStayInAir', 'kT_STATKOPeopleRace', 'kT_STATDoUberGrind', 'kT_STATDoSupUber',
         'kT_STATGetPoints', 'kT_CMNDoXCombo']
ecs.NAMES += [f'{n}{k}' for n in ('hl', 'hlsec', 'checkmark', 'checkbox') for k in range(1, 4)] + [f'hlsec{k}a' for k in range(1, 4)] + \
             ['HL_arrowup', 'HL_arrowdown']
KEYS = [f'{s}{t}' for s in STATS for t in (1, 2, 3)] + ['kT_FEUnlockMonsterTrick']


def main():
    disc = Disc(ISO)
    try:
        lui, ssh = disc.file('DATA/UI/FE.LUI'), disc.file('DATA/UI/FE_1.SSH')
        strings = {}
        for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
            for k, v in loc_file.entries(disc.file(f'DATA/LOCALE/{name}.LOC')).items(): strings.setdefault(k, v)
    finally:
        disc.close()
    pages = shps_pages(ssh); sprites = {}
    for o in lui_objects(lui):
        size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
        sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'FE_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
    elements, events, labels = ecs.decode(lui_screens(lui)[H(SCREEN)], sprites, strings)
    anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
    animations = lui_animations(lui)
    texts = {k: strings.get(H(k)) for k in KEYS}
    missing = [k for k, v in texts.items() if v is None]
    if missing: raise ValueError(f'Missing strings: {missing}')
    names = {n: f'{H(n):08x}' for n in ecs.NAMES if any(e['name'] == f'{H(n):08x}' for e in elements)}
    pages_used = sorted({e['sprite']['page'] for e in elements if e.get('sprite')})
    result = dict(provenance=dict(lui_sha256=hashlib.sha256(lui).hexdigest(), ssh_sha256=hashlib.sha256(ssh).hexdigest(), tool='tools/export_career_highlights.py'),
                  screen=dict(hash=f'{H(SCREEN):08x}', pack='FE', elements=elements, events=events, labels=labels,
                              animations={k: animations[k] for k in sorted(anims) if k in animations}),
                  names=names, strings=texts, pages=pages_used)
    (OUT / 'career-highlights.json').write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(f'Career highlights: {len(elements)} elements, names {sorted(names)}, pages {pages_used}')


if __name__ == '__main__':
    main()
