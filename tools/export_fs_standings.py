#!/usr/bin/env python3
"""Export the CTM freestyle heat results panel for the browser (read-only; docs/ctm-parity.md "Freestyle standings", pv fsStandings).

Source (user's own disc, never modified): DATA/UI/OV.LUI + OV_1.SSH (pages already exported as web/public/assets/UI/OV_1-N.png by
web/prepare-ui.py) and DATA/LOCALE/*.LOC.

  42freestyle_standings  0x0A32C723  the qualifying heats' results (0x1E5B80 build, 0x1E64C8 show): track_event / title
                                     ('Qualifier Heat 1 Standings' / 'Qualifier Heat 2 Standings'), Rank / Riders / Heat 1 /
                                     Heat 2 (column_secondrun) / Total: (column_total) over six rows 24 px apart (rider_N,
                                     run_1.N, run_2.N, total_N), the help lines (help_current01..06 'You are currently in Nth
                                     place.', help_advance, help_sorry), the overlayMenu (focus frames 40 + 10 i as 43final_standings)
  41freestyle_pre        0x073C2F05  the heat card (0x1FBD20 build, 0x1FC768 show): track_event, the round's tab (heat1_tab /
                                     heat2_tab / finalheat_tab), objText1 / objText2 with their bullets, 'Current standings' (three
                                     rows: rider_N, run_1.N, run_2.N in column_secondrun, total_N in column_total), 'Up next'
                                     p0rider / p0run1 / p0dashes, topScoreLabel / topScore, Continue
  @TextPulseWhiteOrange  (animation) the human's row: 0x1E64C8 plays it (mode 3) on rider_N and run_<round>.N (20A6F0); the
                                     card's 0x1FC768 on p0rider and p0dashes (heat 2) or p0run1

The panel behind it is OV_darkblue from tools/export_results_screens.py (web/results-lui.js). Element decoding is
tools/export_audio_menus.py decode() with the state-0 records paired by name (export_results_screens.state0_by_name); the widget
names the executable spells out are added as `label` (and the row widgets rider_N / run_1.N / run_2.N / total_N / help_current0N,
which are formatted names in the ELF).
Output: --out FILE (default web/public/assets/UI/fs-standings.json).
"""
import argparse, hashlib, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_objects, lui_animations, shps_pages  # noqa: E402
import export_audio_menus as eam  # noqa: E402
from export_results_screens import state0_by_name  # noqa: E402
import loc_file  # noqa: E402

H = loc_file.name_hash
SCREEN = '42freestyle_standings'
SCREENS = [SCREEN, '41freestyle_pre']
PULSE = '@TextPulseWhiteOrange'
# the names 0x1E5B80 / 0x1E64C8 build with sprintf ('rider_%d', 'run_1.%d', 'run_2.%d', 'total_%d', 'help_current0%d', 'run_%d.%d')
FORMATTED = [f'rider_{k}' for k in range(1, 7)] + [f'run_{r}.{k}' for r in (1, 2) for k in range(1, 7)] + [f'total_{k}' for k in range(1, 7)] + \
    [f'help_current0{k}' for k in range(1, 7)] + [f'overlayOpt{k}' for k in range(5)]
KEYS = ['kT_OVRCMNHeat1', 'kT_OVRCMNQFHeat1Stand', 'kT_OVRCMNQFHeat2Stand', 'kT_CMNHELPEarnedEnoughPoints', 'kT_OVRCMNFinalRound', 'kT_OVRCMNScoreColon']


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--out', default=str(ROOT / 'web/public/assets/UI/fs-standings.json')); args = ap.parse_args()
    disc = Disc(eam.ISO)
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
    for n in FORMATTED: names[H(n)] = n
    raw, animations = lui_screens(lui), lui_animations(lui)
    pulse = f'{H(PULSE):08x}'
    if pulse not in animations: raise ValueError(f'{PULSE} ({pulse}) not in OV.LUI animations')
    used, screens = set(), {}
    for key in SCREENS:
        elements, events, labels = eam.decode(raw[H(key)], sprites, strings)
        state0_by_name(raw[H(key)], elements)
        for e in elements:
            n = names.get(int(e['name'], 16))
            if n and (not e.get('label') or n in FORMATTED): e['label'] = n
            if e.get('sprite'): used.add(e['sprite']['page'])
        anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v} | {pulse}
        screens[key] = dict(hash=f'{H(key):08x}', pack='OV', state0='byName', elements=elements, events=events, labels=labels,
                            animations={k: animations[k] for k in sorted(anims) if k in animations})
    for page in sorted(used):
        if not (ROOT / 'web/public/assets/UI' / f'{page}.png').exists(): raise ValueError(f'{page}.png missing: run web/prepare-ui.py')
    texts = {k: strings.get(H(k)) for k in KEYS}
    missing = [k for k, v in texts.items() if v is None]
    if missing: raise ValueError(f'Missing strings: {missing}')
    result = dict(provenance=dict(lui_sha256=hashlib.sha256(lui).hexdigest(), ssh_sha256=hashlib.sha256(ssh).hexdigest(), tool='tools/export_fs_standings.py'),
                  coordinates='LUI 640x480 frame, as web/public/assets/UI/results-screens.json', pages=sorted(used), pulse=pulse, strings=texts,
                  screens=screens)
    Path(args.out).write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(f"{', '.join(f'{k} {len(v['elements'])} elements' for k, v in screens.items())}, pulse {pulse}, pages {sorted(used)} -> {args.out}")


if __name__ == '__main__':
    main()
