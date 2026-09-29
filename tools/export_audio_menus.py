#!/usr/bin/env python3
"""Export the original audio menus for the browser (read-only; docs/audio-menus.md).

Source (user's own disc, never modified): DATA/UI/FE.LUI + FE_1.SSH, DATA/UI/OV.LUI + OV_1.SSH (pages already
exported as web/public/assets/UI/FE_1-N.png / OV_1-N.png by web/prepare-ui.py) and DATA/LOCALE/*.LOC. Screens:

  FE.LUI 140audio        0x0768BC7F  Music (cFEStateAudioOptions 0x195FF0, front end / lodge)
  FE.LUI 16radio         0x04D87A9F  Edit Playlist / Request Line (cFEStateRequestLine 0x196B90, front end / lodge)
  FE.LUI 141advsettings  0x0255B2C3  Sound Options (cFEStateOptionsSound 0x18A258: sound mode, 3 sliders, DJ, arcade, talk)
  OV.LUI 142audio_pda    0x0C8A77C1  in-game Audio (the same cFEStateAudioOptions when global+0x84 != 0, i.e. in game)
  OV.LUI 37beoptions                 in-game Options (the pause / MCOMM Options PDA page)
  OV.LUI 38session                   MCOMM Session (the map and its points: web/career-ui.js drawSession)
  OV.LUI finishov                    the freestyle finish panel (ctor 0x21CF60, setup 0x1E8200: run label, place, points, medal;
                                     web/career-ui.js finishPanel)
  OV.LUI 143radio_pda    0x0C8A3641  in-game Edit Playlist (the same cFEStateRequestLine in game)

Element decoding is tools/export_character_select.py decode() plus the two widget kinds these screens use:
0x15 menu row (tail: label hash, value hash; the children draw relative to the row) and 0x16 slider (after the u16 words 89, 94: u32 0,
track sprite hash, knob sprite hash; 0x39E130 places the knob at track.x + value * (trackW - knobW) /
(count - 1), count = +0x78 = 12 for the sound sliders, 0x18B4B8). Rows and sliders are exported as 'group' with
widget='row'/'slider' so web/lui-player.js offsets their children and does not draw them.
Output (git-ignored): web/public/assets/UI/audio-menus.json.
"""
import hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_objects, lui_animations, shps_pages  # noqa: E402
from lui_screen import Screen  # noqa: E402
import export_character_select as ecs  # noqa: E402
import loc_file  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = Path(sys.argv[sys.argv.index('--out') + 1]) if '--out' in sys.argv else ROOT / 'web/public/assets/UI'   # --out DIR: a scratch export
H = loc_file.name_hash
SCREENS = {'FE': ['140audio', '16radio', '141advsettings'], 'OV': ['142audio_pda', '143radio_pda', '37beoptions', '38session', 'finishov']}   # 37beoptions: the in-game Options PDA page (web/audio-menu.js pda-options)
# Widget names the code looks up (0x196540, 0x196F98, 0x18A3D8, 0x18B380).
ecs.NAMES += ['0box', '1box', '2box', '3box', '4box', '5box', '6box', '7box', 'radio big', 'big mountain', 'request line', 'edit req',
              'playlist', 'helptext', 'CurrentSong', 'CurrentArtist', 'arr_up', 'arr_down', 'num_songs', 'song list', 'singer', 'song by',
              'cost', 'cost_answer', 'you have', 'you have_answer', 'free songs', 'Menu0000', 'btext_select', 'btext_preview',
              'ps2x', 'ps2sq', 'ps2tri', 'ps2o'] + ['songs%d' % i for i in range(8)] + ['%dfree' % i for i in range(6)] + \
             ['1lb', '2sl', '3sl', '4sl', '5lb', '6lb', '7lb', 'ongroup', 'fegroup'] + \
             ['0lb', '2lb', '5sl', '6sl', '8lb', 'save', 'EATalk', 'EATalksl', 'EATalkPair', 'camera select', 'camera 2 select', 'Menu', 'pdabtext1', 'pdabtext2', 'pdaps2x', 'pdaps2tri', 'pdabuttons',
              'HelpText', 'SessionViewPoints', 'title', 'pdatitle', 'MapPic', 'ConfirmMenu', 'ConfirmMenu_Yes', 'ConfirmMenu_No', 'Popup1',
              'ConfirmPopup', 'btext1', 'ConfirmMenu_Yes_x', 'ConfirmMenu_No_x', '3D Ov', 'Big outline', 'Big shape', 'Small outline', 'Small shape', 'Shadow'] + \
             ['firstrun', 'secondrun', 'thirdrun', 'finalrun', 'pointstotal', 'new_record', 'screendata', 'gold', 'silver', 'bronze', 'platinum'] + ['place%d' % i for i in range(1, 7)]   # finishov (0x1E8200)
# Sprites the code sets by name (0x196088 / 0x196C68): checkbox, empty box, the yellow dollar of an unowned song.
SPRITES = ['checkbox', 'empty box', 'yell_dollar sign']
# OV sprites the Session map code sets by name (0x2096A8 'location' Player Indicator Icon, 0x209970 'dot_visited' / 'indicator').
OV_SPRITES = ['location', 'dot_visited', 'indicator']
# Locale keys the code sets on these screens (0x196960 help table, 0x1981F8, 0x198340, 0x1986D0, 0x197E70, 0x18B380).
KEYS = ['kT_HELPAUDIORadioBig', 'kT_HELPAUDIOBMA', 'kT_HELPAUDIOCreateInLodge', 'kT_HELPAUDIOCreate', 'kT_HELPAUDIOCreateInCTM',
        'kT_FEHELPCustPlayDJ', 'kT_FEHELPCustPlayNoDJ', 'kT_HELPAUDIOEditReq', 'kT_OVRCMNSongsInList', 'kT_16NowPlaying', 'kT_16Artist',
        'kT_HELPAudio1SongMin', 'kT_HELPAUDIOAddRem', 'kT_HELPAUDIOAvailBuy', 'kT_16SaveMoreCarsh', 'kT_FAQRADIOBA', 'kT_CMNHELPBuySongCTM',
        'kT_BTNRemSong', 'kT_BTNAddSong', 'kT_16BuyMusicTracks', 'kT_OVRCMNSongBy', 'kT_OVRCMNFree', 'kT_OVRCMNBuySongCredit',
        'kT_16BuyMusicTrack', 'kT_20SpeakerStereo', 'kT_20SpeakerSurround', 'kT_20SpeakerMono', 'kT_20SpeakerDTS', 'kT_CMNOff', 'kT_CMNOn',
        'kT_CMNYes', 'kT_CMNNo', 'kT_OVRHELPChangeMusic', 'kT_OVRCMNPointstotal']


def decode(raw, sprites, strings):
    elements, events, labels = ecs.decode(raw, sprites, strings)
    sc = Screen.decode(raw); by = {e['name']: e for e in elements}
    for e, d in zip(elements, sc.definitions):
        kind = struct.unpack_from('<H', d, 0)[0]; tail = d[32:]
        if kind == 0x15:
            e.update(kind='group', widget='row', children=[f'{c:08x}' for c in struct.unpack_from('<2I', tail, 0)])
        elif kind == 0x16:
            track, knob = struct.unpack_from('<2I', tail, 4)
            e.update(kind='group', widget='slider', children=[f'{track:08x}', f'{knob:08x}'], track=f'{track:08x}', knob=f'{knob:08x}')
    for e in elements:
        if e.get('widget'):
            for c in e['children']:
                if c in by: by[c]['parent'] = e['name']
    return elements, events, labels


def main():
    disc = Disc(ISO)
    try:
        files = {k: (disc.file(f'DATA/UI/{k}.LUI'), disc.file(f'DATA/UI/{k}_1.SSH')) for k in SCREENS}
        strings = {}
        for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
            for k, v in loc_file.entries(disc.file(f'DATA/LOCALE/{name}.LOC')).items(): strings.setdefault(k, v)
    finally:
        disc.close()
    screens, named, used, provenance = {}, {}, set(), {}
    for pack, (lui, ssh) in files.items():
        pages = shps_pages(ssh); sprites = {}
        for o in lui_objects(lui):
            size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
            sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'{pack}_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
        named[pack] = {n: sprites[H(n)] for n in SPRITES + (OV_SPRITES if pack == 'OV' else [])}
        raw = lui_screens(lui)
        provenance[pack] = dict(lui_sha256=hashlib.sha256(lui).hexdigest(), ssh_sha256=hashlib.sha256(ssh).hexdigest())
        animations = lui_animations(lui)
        for key in SCREENS[pack]:
            elements, events, labels = decode(raw[H(key)], sprites, strings)
            anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
            for e in elements:
                if e.get('sprite'): used.add(e['sprite']['page'])
            screens[key] = dict(hash=f'{H(key):08x}', pack=pack, elements=elements, events=events, labels=labels,
                                animations={k: animations[k] for k in sorted(anims) if k in animations})
    for s in named.values(): used |= {x['page'] for x in s.values()}
    for page in sorted(used):
        if not (ROOT / 'web/public/assets/UI' / f'{page}.png').exists(): raise ValueError(f'{page}.png missing: run web/prepare-ui.py')
    texts = {k: strings.get(H(k)) for k in KEYS}
    missing = [k for k, v in texts.items() if v is None]
    if missing: raise ValueError(f'Missing strings: {missing}')
    result = dict(provenance=dict(**provenance, tool='tools/export_audio_menus.py'),
                  coordinates='LUI 640x480 frame, as web/public/assets/UI/character-select.json; widget rows/sliders are groups',
                  sprites=named, strings=texts, pages=sorted(used), screens=screens)
    (OUT / 'audio-menus.json').write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(f"Audio menus: {', '.join(f'{k} {len(v['elements'])}' for k, v in screens.items())}, pages {sorted(used)} -> {OUT / 'audio-menus.json'}")


if __name__ == '__main__':
    main()
