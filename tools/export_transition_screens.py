#!/usr/bin/env python3
"""Export the lodge transition load screens for the browser (read-only; docs/cutscenes.md "Inventory" row 14).

  FL.LUI 117loadinlodge  (0x0995F895) cFELoadStateInLodge: 'Would you like to enter the lodge?' Yes -> the lodge
  GL.LUI 118loadoutlodge (0x02D97D35) cGameLoadStateOutLodge: the lodge's Return to Game -> back on the slope
Both are the handheld (PDA) with the load percentage and 'Loading...' over the ice background. Decoded with the same
LUI reader as the front-end screens (tools/export_character_select.py decode()); played by web/lui-player.js.

Writes (git-ignored) web/public/assets/CUTSCENES/TRANSITIONS/: screens.json + the atlas pages FL_1-N.png / GL_1-N.png.
  python3 tools/export_transition_screens.py
"""
import json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_animations, lui_objects, shps_pages, png  # noqa: E402
from export_character_select import decode  # noqa: E402
import loc_file  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = ROOT / 'web/public/assets/CUTSCENES/TRANSITIONS'
SCREENS = {'117loadinlodge': ('FL', 0x0995F895), '118loadoutlodge': ('GL', 0x02D97D35)}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    disc = Disc(ISO)
    try:
        strings = {}
        for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
            for k, v in loc_file.entries(disc.file(f'DATA/LOCALE/{name}.LOC')).items(): strings.setdefault(k, v)
        files = {f: (disc.file(f'DATA/UI/{f}.LUI'), disc.file(f'DATA/UI/{f}_1.SSH')) for f in ('FL', 'GL')}
    finally:
        disc.close()
    screens, used = {}, set()
    for key, (f, h) in SCREENS.items():
        lui, ssh = files[f]
        pages = shps_pages(ssh); sprites = {}
        for o in lui_objects(lui):
            size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
            sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'{f}_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
        elements, events, labels = decode(lui_screens(lui)[h], sprites, strings)
        animations = lui_animations(lui)
        anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
        for e in elements:
            if e.get('sprite'): used.add((f, e['sprite']['page']))
        screens[key] = dict(hash=f'{h:08x}', file=f'{f}.LUI', elements=elements, events=events, labels=labels,
                            animations={k: animations[k] for k in sorted(anims) if k in animations})
    for f, page in sorted(used):
        index = int(page.split('-')[1]); p = shps_pages(files[f][1])[index]
        (OUT / f'{page}.png').write_bytes(png(p['width'], p['height'], p['rgba']))
    (OUT / 'screens.json').write_text(json.dumps(dict(version=1, source='DATA/UI/FL.LUI, GL.LUI', screens=screens,
                                                      pages=sorted(p for _, p in used)), separators=(',', ':')))
    print(f'transition screens: {list(screens)}, pages {sorted(p for _, p in used)} -> {OUT}')


if __name__ == '__main__':
    main()
