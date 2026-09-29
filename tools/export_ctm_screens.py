#!/usr/bin/env python3
"""Export the Conquer the Mountain in-game PDA screens (the MCOMM) for the browser (read-only; docs/ctm-parity.md).

Source (user's own disc, never modified): DATA/UI/OV.LUI + OV_1.SSH (pages already exported as web/public/assets/UI/OV_1-N.png
by web/prepare-ui.py) and DATA/LOCALE/*.LOC. Screens (overlay manager 0x20AB50, 'cOVStateOverlay'):

  PDATemplate      0x0CEE25E5  the MCOMM frame every PDA overlay sits in (bars, the MCOMM badge with the temperature, mountain)
  bganim1          0x08D850C1  the drifting snowflakes behind the PDA screens
  31paus_freeride  0x05A57E05  the MCOMM menu (overlay 3 in free ride / the career pause): eight text rows + vector icons
  87yndialog       0x04A83D57  the Yes / No popup ('Quit Game', 'Are you sure?', 'Transport to this area now?', ...)
  98enterlodge     0x0B0ED195  'Would you like to enter the lodge?' (overlay 0x1F)
  63bc_start       Big Challenge offer / info (overlay 0x1D: title, name, description, 'Accept challenge?', Yes / No)
  90bc_fail        'Challenge failed' / 'Retry?' / Yes / No / Challenge Info (overlay 0x1E)

Element decoding is tools/export_audio_menus.py decode() (tools/export_character_select.py + menu rows and sliders); the widget
names the executable spells out are added as `label` where the ELF has them. Output (git-ignored): web/public/assets/UI/ctm-screens.json.
"""
import hashlib, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_objects, lui_animations, shps_pages  # noqa: E402
import export_audio_menus as eam  # noqa: E402
import loc_file  # noqa: E402

ISO = eam.ISO
OUT = ROOT / 'web/public/assets/UI'
H = loc_file.name_hash
SCREENS = ['PDATemplate', 'bganim1', '31paus_freeride', '87yndialog', '98enterlodge', '63bc_start', '90bc_fail']
KEYS = ['kT_OVRHELPGetBoarding', 'kT_MAPHELPTransport', 'kT_OVRHELPMAP', 'kT_OVRHELPMessages', 'kT_OVRHELPChangeMusic', 'kT_OVRHELPOptions',
        'kT_MAPHELPQuitGame', 'kT_OVRHELPRestartComp', 'kT_OVRHELPQuitComp', 'kT_CMNYes', 'kT_CMNNo', 'kT_BTNSelect', 'kT_BTNPrevious']


def main():
    disc = Disc(ISO)
    try:
        lui, ssh = disc.file('DATA/UI/OV.LUI'), disc.file('DATA/UI/OV_1.SSH')
        strings = {}
        for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
            for k, v in loc_file.entries(disc.file(f'DATA/LOCALE/{name}.LOC')).items(): strings.setdefault(k, v)
    finally:
        disc.close()
    pages = shps_pages(ssh); sprites = {}
    for o in lui_objects(lui):
        size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
        sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'OV_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes(); names = {}
    for m in re.finditer(rb'[ -~]{2,40}', elf): names.setdefault(H(m.group().decode()), m.group().decode())
    raw, animations, screens, used = lui_screens(lui), lui_animations(lui), {}, set()
    for key in SCREENS:
        elements, events, labels = eam.decode(raw[H(key)], sprites, strings)
        for e in elements:
            n = names.get(int(e['name'], 16))
            if n and not e.get('label'): e['label'] = n
            if e.get('sprite'): used.add(e['sprite']['page'])
        anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
        screens[key] = dict(hash=f'{H(key):08x}', pack='OV', elements=elements, events=events, labels=labels,
                            animations={k: animations[k] for k in sorted(anims) if k in animations})
    for page in sorted(used):
        if not (OUT / f'{page}.png').exists(): raise ValueError(f'{page}.png missing: run web/prepare-ui.py')
    texts = {k: strings.get(H(k)) for k in KEYS}
    missing = [k for k, v in texts.items() if v is None]
    if missing: raise ValueError(f'Missing strings: {missing}')
    result = dict(provenance=dict(lui_sha256=hashlib.sha256(lui).hexdigest(), ssh_sha256=hashlib.sha256(ssh).hexdigest(), tool='tools/export_ctm_screens.py'),
                  coordinates='LUI 640x480 frame, as web/public/assets/UI/audio-menus.json', strings=texts, pages=sorted(used), screens=screens)
    (OUT / 'ctm-screens.json').write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(f"CTM screens: {', '.join(f'{k} {len(v['elements'])}' for k, v in screens.items())}, pages {sorted(used)} -> {OUT / 'ctm-screens.json'}")


if __name__ == '__main__':
    main()
