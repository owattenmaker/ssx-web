#!/usr/bin/env python3
"""Export the original front-end menu screens around Single Event for the browser (read-only; docs/characters.md
"Single Event track selector", "Main menu").

Source (user's own disc, never modified): DATA/UI/FE.LUI (+ the FE_1 pages already exported by web/prepare-ui.py),
DATA/LOCALE/{FEAMER,OVAMER,CMNAMER}.LOC and the executable SLUS_207.72 (its strings name the LUI widgets). Screens
(the table keys are the ELF hash of these names, tools/sam_ps2/loc_file.py name_hash):

  Map               0x00005380  Select Peak / Select Mode / Select Event (and the MCOMM Transport): the peak list,
                                goal/mode/event menus Peak%dRaceLocations ..., the map picture MapPic, the course
                                route shapes <code>path, the start indicators, INFO tables, locks, help text
  femap_template    0x0437FEC5  the orange '3' front-end frame the map screens draw over
  07main_men        0x08065FDE  Main Menu (Single Event / Conquer The Mountain / Multi Play / Previews / Online)
  146Bonusmat       0x063496A4  Previews: the EA trailers (NFS Underground, NFL STREET, NBA STREET Vol. 2)
  25saveload        0x08CE8C54  Save/Load (Save game / Load game / Save options / Load options / Load replay / New game)
  122Autosave       0x0CDE35C5  the autosave message box
  06title           0x03DB0B45  the title screen: FE blue, white ramp, two FE_1-18 mountains, the FE_1-20 logo,
                                'Press START button' and the copyright (web/title-screen.js, docs/first-load.md)
  transition        0x05AFE15E  the snow flake burst from the title to the Main Menu (40 frames, ends white)

Every widget is decoded like tools/export_character_select.py decode(); names the executable spells out (e.g.
'ara1path', 'Peak1RaceLocations', 'MapPic') are added as 'label'. Output (git-ignored):
web/public/assets/UI/fe-menus.json.
"""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
import export_character_select as X  # noqa: E402
from inspect_disc import Disc  # noqa: E402

OUT = ROOT / 'web/public/assets/UI/fe-menus.json'
SCREENS = {'Map': 0x00005380, 'femap_template': 0x0437FEC5, '07main_men': 0x08065FDE, '146Bonusmat': 0x063496A4,
           '25saveload': 0x08CE8C54, '122Autosave': 0x0CDE35C5, '06title': 0x03DB0B45,
           'transition': 0x05AFE15E}


def main():
    disc = Disc(X.ISO)
    try:
        lui = disc.file('DATA/UI/FE.LUI'); ssh = disc.file('DATA/UI/FE_1.SSH'); elf = disc.file('SLUS_207.72'); strings = {}
        for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
            for k, v in X.loc_file.entries(disc.file(f'DATA/LOCALE/{name}.LOC')).items(): strings.setdefault(k, v)
    finally:
        disc.close()
    for name in SCREENS: assert X.H(name) == SCREENS[name], name
    names = {}
    for m in re.finditer(rb'[\x20-\x7e]{1,40}', elf):
        s = m.group().decode(); names.setdefault(X.H(s), s)
    for peak in (1, 2, 3):                      # formatted at run time (0x2010E0 ...)
        for f in ('Peak%dRaceLocations', 'Peak%dFreestyleLocations', 'Peak%dFreerideLocations', 'P%dyou are here', 'SPG_Peak%dGoals', 'Peak%dRace', 'Peak%dFreestyle', 'Peak%dFreeride'):
            names[X.H(f % peak)] = f % peak
    pages = X.shps_pages(ssh); sprites = {}; used = set()
    for o in X.lui_objects(lui):
        size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
        sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'FE_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
    raw, animations = X.lui_screens(lui), X.lui_animations(lui); screens = {}
    for key, h in SCREENS.items():
        elements, events, labels = X.decode(raw[h], sprites, strings)
        for e in elements:
            n = names.get(int(e['name'], 16))
            if n and 'label' not in e: e['label'] = n.strip()
            if e.get('sprite'): used.add(e['sprite']['page'])
        for lab in labels:
            n = names.get(int(lab['name'], 16))
            if n and 'label' not in lab: lab['label'] = n.strip()
        anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
        screens[key] = dict(hash=f'{h:08x}', elements=elements, events=events, labels=labels, animations={k: animations[k] for k in sorted(anims) if k in animations})
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(dict(provenance=dict(source='DATA/UI/FE.LUI', tool='tools/export_fe_menus.py', screens={k: f'0x{v:08X}' for k, v in SCREENS.items()}),
                                  pages=sorted(used), screens=screens), separators=(',', ':')))
    print(f'{OUT.relative_to(ROOT)}: {len(screens)} screens, pages {sorted(used)}')


if __name__ == '__main__':
    main()
