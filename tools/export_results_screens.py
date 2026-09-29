#!/usr/bin/env python3
"""Export the in-game results panels for the browser (read-only; docs/visual-parity.md, pv luiResults).

Source (user's own disc, never modified): DATA/UI/OV.LUI + OV_1.SSH (pages already exported as web/public/assets/UI/OV_1-N.png
by web/prepare-ui.py) and DATA/LOCALE/*.LOC. Screens (overlay manager 0x20AB50):

  OV_darkblue        0x083818F5  the dark blue panel every in-race card / result sits in: the frame shapes (cut corners, the
                                 header bar and its frame lines), the header picture (OV_1-6) and the blinking lights; open
                                 animation frames 13..60, idle lights 66..315, close 500..560
  43final_standings  0x00286583  the results (0x1E6668 / 0x1E78F4): track_event / title, Rank / Riders / Time columns (six rows
                                 at 20 px), the gold / silver / bronze medals, helptext, the overlayMenu (Continue / Restart /
                                 Replay / Records / Quit; focus frames 40..80 move ps2x and whiten the row), the multiplayer chat
  61toptimes         0x067DF2A3  Top 5 Record Times / Scores: track_xxx, top5record | top5scores, Rank / Name / With / Time|Points
                                 over five rows at 26 px, helptext01 | 02 (a new top time / score), the Continue / Save Records menu
                                 (focus frames 40 / 45; ps2x group authored at alpha 0)
  70peakchal_results 0x056B3173  a peak run's result: title1 / title2, 'Event Results', Time|Score to beat and Your time|score
                                 (labels right-aligned at 328), helptext, the Transport / Restart / Quit menu (focus frames 50 / 60 / 70)
  40race_pre         0x079D8885  the race round card: track_event, the round (title / tab_*), objTextLine1 with its bullet,
                                 'Riders' (right-aligned) over six names at 23 px, Record time: / topTime, Continue
  62reward_list      0x0BD7AB44  the rewards (overlay 0x10): title_rewardlisting, 'Rewards', the intro line, nine rows at 22 px
                                 (focus frames 40 + 5 k), the up / down arrows, Continue
  68rival_pre        0x0C359585  the rival / peak run card: title1 / title2, the headline, obj1 / obj2 with bullets, the target, Continue

Element decoding is tools/export_audio_menus.py decode(); the widget names the executable spells out are added as `label`.
Output: --out FILE (default web/public/assets/UI/results-screens.json).
"""
import argparse, hashlib, json, re, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_objects, lui_animations, shps_pages  # noqa: E402
import export_audio_menus as eam  # noqa: E402
import loc_file  # noqa: E402

from lui_screen import Screen  # noqa: E402
from export_loading_screen import props as record_props  # noqa: E402

H = loc_file.name_hash
SCREENS = ['OV_darkblue', '43final_standings', '61toptimes', '70peakchal_results', '40race_pre', '62reward_list', '68rival_pre']


def state0_by_name(raw, elements):
    """Frame-0 properties by element name, as the runtime applies them: 0x39CD30 (record 0x21) and 0x39CCA8 (0x20) look the
    element up by the record's hash (0x39D860: screen+0x3C, element+0x38), in record order. decode() pairs the records with
    the definitions by index, which holds until a screen repeats a record: OV_darkblue's frame 0 sets 00e91994 twice
    (records 11 and 12, 50 records for 49 definitions), so every later element took its predecessor's record (the header
    lights' groups 06c3ae11 / 06c3ae12 got the lights' vertices, the lights the groups' layout, 06c3ae12's own (449, 63) was
    dropped). No other FE / OV / GL screen repeats a frame-0 record inside its definitions. A 0x21 record stops the
    element's animation (0x39FD38) and sets the listed properties; 0x20 binds one."""
    by = {e['name']: e for e in elements}; seen = {}
    for r in Screen.decode(raw).states[0].records:
        kind = struct.unpack_from('<H', r, 0)[0]
        if kind == 0x21:
            name = f'{struct.unpack_from("<I", r, 4)[0]:08x}'; s = seen.setdefault(name, {})
            s['props'] = {**s.get('props', {}), **record_props(r)}; s.pop('anim', None)
        elif kind == 0x20:
            anim, element, mode = struct.unpack_from('<III', r, 4)
            seen.setdefault(f'{element:08x}', {})['anim'] = dict(hash=f'{anim:08x}', mode=mode)
    for name, s in seen.items():
        e = by.get(name)
        if not e: continue
        for k in ('props', 'anim'):                      # in place: an element whose record did not move stays byte-identical
            if k in s: e[k] = s[k]
            else: e.pop(k, None)


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--out', default=str(ROOT / 'web/public/assets/UI/results-screens.json')); args = ap.parse_args()
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
    raw, animations, screens, used = lui_screens(lui), lui_animations(lui), {}, set()
    for key in SCREENS:
        elements, events, labels = eam.decode(raw[H(key)], sprites, strings)
        state0_by_name(raw[H(key)], elements)
        for e in elements:
            n = names.get(int(e['name'], 16))
            if n and not e.get('label'): e['label'] = n
            if e.get('sprite'): used.add(e['sprite']['page'])
        anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
        screens[key] = dict(hash=f'{H(key):08x}', pack='OV', state0='byName', elements=elements, events=events, labels=labels,
                            animations={k: animations[k] for k in sorted(anims) if k in animations})
    for page in sorted(used):
        if not (ROOT / 'web/public/assets/UI' / f'{page}.png').exists(): raise ValueError(f'{page}.png missing: run web/prepare-ui.py')
    result = dict(provenance=dict(lui_sha256=hashlib.sha256(lui).hexdigest(), ssh_sha256=hashlib.sha256(ssh).hexdigest(), tool='tools/export_results_screens.py'),
                  coordinates='LUI 640x480 frame, as web/public/assets/UI/ctm-screens.json', pages=sorted(used), screens=screens)
    Path(args.out).write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(f"results screens: {', '.join(f'{k} {len(v['elements'])}' for k, v in screens.items())}, pages {sorted(used)} -> {args.out}")


if __name__ == '__main__':
    main()
