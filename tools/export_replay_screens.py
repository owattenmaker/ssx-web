#!/usr/bin/env python3
"""Export the replay overlay for the browser (read-only; docs/replay.md, pv replay).

Source (user's own disc, never modified): DATA/UI/OV.LUI + OV_1.SSH (pages already exported as web/public/assets/UI/OV_1-N.png
by web/prepare-ui.py) and DATA/LOCALE/*.LOC. Screens (overlay manager 0x20AB50):

  64replay        cOVState_REPLAY (dialog 0x11, onWidgetCreate 0x20DE30, onUpdate 0x20E530): the 'Replay' title bar with the
                  timeline (percentcomplete = frame / length x 315 + 170) and 'Timeline', the help panel (Camera - <name> from
                  setupCameraName 0x20E6B0, Skip backward / forward, Change camera, Slow motion, Play/Pause, Timeline, Manual cam,
                  Exit, Menu up / down, Hide help) that D-pad up / down slides between y 245 and 480
  (the Replay Menu popup, Start: Save replay / Exit replay / Continue, is a group of 64replay; '119Save Replay', the memory-card
  save, is not exported: the browser greys Save replay)

Element decoding is tools/export_audio_menus.py decode(); the widget names the executable spells out are added as `label`.
Output: --out FILE (default: the session scratchpad; the page reads /assets/UI/replay-screens.json).
"""
import argparse, hashlib, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_objects, lui_animations, shps_pages  # noqa: E402
import export_audio_menus as eam  # noqa: E402
import loc_file  # noqa: E402

H = loc_file.name_hash
SCREENS = ['64replay']


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--out', default=str(ROOT / 'web/public/assets/UI/replay-screens.json')); args = ap.parse_args()
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
        for e in elements:
            n = names.get(int(e['name'], 16))
            if n and not e.get('label'): e['label'] = n
            if e.get('sprite'): used.add(e['sprite']['page'])
        anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
        screens[key] = dict(hash=f'{H(key):08x}', pack='OV', elements=elements, events=events, labels=labels,
                            animations={k: animations[k] for k in sorted(anims) if k in animations})
    for page in sorted(used):
        if not (ROOT / 'web/public/assets/UI' / f'{page}.png').exists(): raise ValueError(f'{page}.png missing: run web/prepare-ui.py')
    result = dict(provenance=dict(lui_sha256=hashlib.sha256(lui).hexdigest(), ssh_sha256=hashlib.sha256(ssh).hexdigest(), tool='tools/export_replay_screens.py'),
                  coordinates='LUI 640x480 frame, as web/public/assets/UI/ctm-screens.json', pages=sorted(used), screens=screens)
    Path(args.out).write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(f"replay screens: {', '.join(f'{k} {len(v['elements'])}' for k, v in screens.items())}, pages {sorted(used)} -> {args.out}")


if __name__ == '__main__':
    main()
