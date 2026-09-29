#!/usr/bin/env python3
"""Export the original Select Character front end for the browser (read-only; docs/characters.md).

Source (user's own disc, never modified): DATA/UI/FE.LUI + FE_1.SSH (the pages are already exported as
web/public/assets/UI/FE_1-N.png by web/prepare-ui.py) and DATA/LOCALE/*.LOC. Screens:

  08sel_char        0x0C23E1A2  Select Character (cFEStateCharSelect, vtable 0x46D000, onEnter 0x181148)
  09set_char        0x0CA31FA2  Setup Character
  131cheat_char     0x006AA322  Select Cheat Character (cFEStateCheatCharSelect, ctor 0x182220)
  transition_flash  0x00AB3C45  the white flash Cross plays before Setup Character
  bg_snow_loop      0x056D8BBD  the always-on front-end snowflakes
and the screens around them (web/fe-screens.js, docs/characters.md "Setup Character, Rider Details, Options, Load game"):
  154rider_details          0x0B32CE73  Rider Details (state 0x1832BC, items Option_1..5 = 5..9)
  155rider_details_conquer  0x0FFAD5A2  Rider Details in the career lodge (0x1F4064)
  14rid_prof                0x0FA0EE56  Rider Profile: DNA / Faves / Q&A / BIO pages (0x1908D4)
  140audio                  0x0768BC7F  Music (Radio BIG / Ambience / Custom Playlist [DJ] / [No DJ] / Edit Playlist)
  18options                 0x067AAFB3  Options (FEOptions 0x1887A0)
  93profile_load            0x06488254  Load game (cFEStateProfileLoad, 0x18EDDC)
  66ut_btnmap               0x05E8EAC0  Ubertrick Setup (cFEStateUberTrick, 0x1849C4)
  Fullkeyboard              0x05413EA4  the on-screen keyboard (cKeyboardPopup 0x1CB030: Player Name, Enter Cheat)
  19game_opt                0x083DF9D4  Game Options (Speed units, Widescreen, Screen position, Video calibration)
  141advsettings            0x0255B2C3  Sound Options (cFEStateOptionsSound)
  22control                 0x0A65D25C  Controller Settings (cFEStateOptionsController)
  21hud_opt                 0x0FBA4C94  HUD Options (cFEStateOptionsHUD)
  26credits                 0x0A8BDB33  Credits (cFEOptionsCredits 0x185AAC: TextScroll of kT_CREDITS*, CRAMER.LOC)
  128rewardsroom            0x034B045D  Rewards (cFEStateRewardsRoom)
  129 rewardgallery         0x0C495039  a reward category's page of thumbnails
  130rewardposter           0x046FCA42  one reward shown full screen
  popup                     0x007767C0  the Yes/No question box (e.g. 'Would you like to save your Options?' leaving Options)
  24screen_position         0x0746461E  Game Options > Screen position (cFEPopupScreenPos, vtable 0x46B230)
Element kinds 0x12 / 0x20 (a label / a text box whose text the code sets) are exported as text; 0x15 (an option
row: label element + value element) as 'option' with those two children. Also exported:
the rider profile texts (kT_DNA1..8 / kT_FAVES1..12 / kT_QNA1..4 / kT_FULLBIO1 + first name; Sam's from
tools/sam_ps2/rider_bio.py), the locale strings the code sets on these screens, and the trick score table
0x530600 (begin/hold points per score id, runtime RAM, read from Zoe's derived countdown savestate) that turns an
Ubertrick Setup choice into a grab-profile uber row.

Every definition is flattened like tools/export_loading_screen.py screen_layout() (frame-0 properties or
animation binding, timeline events), plus the kinds that screen does not use: menus (0x13, child list), bars
(0x19, background/fill shapes) and shapes with their vertex/triangle bytes. Sprites resolve through the FE.LUI
object table (page = flags bits 8..15). The cheat character faces (<name>face, CHAR pages FE_1-0..4) come from
the same object table. Sam's roster silhouettes (the Sam PS2 build's 64x128 page, tools/sam_ps2
extend_roster_texture.py) are copied from local/sam-ps2/roster/sam-roster-atlas.png when present.
Output (git-ignored): web/public/assets/UI/character-select.json (+ sam-roster.png).
"""
import hashlib, json, os, shutil, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_animations, lui_objects, shps_pages, props  # noqa: E402
from lui_screen import Screen  # noqa: E402
import loc_file  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
ASSETS_UI = ROOT / 'web/public/assets/UI'
# UI_OUT=DIR writes the export elsewhere (a scratch copy for the coordinator); the atlas pages are still checked in the assets
OUT = Path(os.environ['UI_OUT']) if os.environ.get('UI_OUT') else ASSETS_UI
SCREENS = {'08sel_char': 0x0C23E1A2, '09set_char': 0x0CA31FA2, '131cheat_char': 0x006AA322, 'transition_flash': 0x00AB3C45, 'bg_snow_loop': 0x056D8BBD,
           '154rider_details': 0x0B32CE73, '155rider_details_conquer': 0x0FFAD5A2, '14rid_prof': 0x0FA0EE56, '140audio': 0x0768BC7F,
           '18options': 0x067AAFB3, '93profile_load': 0x06488254, '66ut_btnmap': 0x05E8EAC0, 'Fullkeyboard': 0x05413EA4,
           '19game_opt': 0x083DF9D4, '141advsettings': 0x0255B2C3, '22control': 0x0A65D25C, '21hud_opt': 0x0FBA4C94, '26credits': 0x0A8BDB33,
           '128rewardsroom': 0x034B045D, '129rewardgallery': 0x0C495039, '130rewardposter': 0x046FCA42,
           'popup': 0x007767C0, '24screen_position': 0x0746461E,
           '28lodge': 0x05F35AB5, '33buyattribs': 0x0E68B673,
           '139buy_popup': 0x09EA9440,
           '125mountainroom': 0x0F88507D, '126peakroom': 0x0B2A309D, '127trophyroom': 0x0E7DE85D}   # the lodge's Trophies (0x1D2990 / 0x1D3890 / 0x1D4368, web/trophy-room.js)   # cUIStateBuyPopup's screen (0x1CAC30): the buy popups (web/buy-popup.js, pv buyAttribs)   # the career lodge (web/fe-screens.js drawLodgeMenu) and Buy Attributes
# Credits order (0x185C54..0x185F04): kT_CREDITS000..006, 006a, 006b, 007..046, 049..074, 074a, 075..080, 087, 088, 090..145,
# 147, 148, 150..153, 154..167 (the gaps are skipped by the code).
CREDITS = ([f'{i:03d}' for i in range(0, 7)] + ['006a', '006b'] + [f'{i:03d}' for i in range(7, 47)] + [f'{i:03d}' for i in range(49, 75)] + ['074a']
           + [f'{i:03d}' for i in range(75, 81)] + ['087', '088'] + [f'{i:03d}' for i in range(90, 146)] + ['147', '148']
           + [f'{i:03d}' for i in range(150, 154)] + [f'{i:03d}' for i in range(154, 168)])
# Option value strings by locale key (hashed like 0x317670).
OPTION_KEYS = ['kT_CMNMPH', 'kT_CMNKPH', 'kT_CMNOff', 'kT_CMNOn', 'kT_19Widescreen169', 'kT_19WidescreenAnimorphic', 'kT_20SpeakerStereo',
               'kT_20SpeakerSurround', 'kT_20SpeakerMono', 'kT_20PresetDefault', 'kT_OVRCMNPro', 'kT_CMNHUDFull', 'kT_CMNHUDMinimal', 'kT_CMNHUDNone']
KIND = {0x10: 'group', 0x11: 'sprite', 0x12: 'text', 0x13: 'menu', 0x15: 'option', 0x17: 'text', 0x18: 'shape', 0x19: 'bar', 0x20: 'text'}
H = loc_file.name_hash
# Widget / state names the code looks up by hash (0x181620 onWidgetCreate, 0x181BD0 stats, menu labels).
NAMES = ['NumAttr%d' % i for i in range(7)] + ['%dpb' % i for i in range(7)] + ['NumAttrOver', 'overall', 'Menu', 'Stop', 'hll', 'hlr']
# Trophies (0x1D2A90 / 0x1D38F8 / 0x1D43F0 look these up by name)
NAMES += ['Peak pass text', 'peak1', 'peak2', 'peak3', 'Group peak pass', 'Front', 'peak pass', 'level numb back', 'level numb front', 'nm1w', 'nm2w', 'nm3w',
          'screentitle', 'helptext', 'race', 'freestyle', 'explore', 'earnings', 'trph_race', 'trph_freestyle', 'trph_explore', 'trph_earn', 'bitmap', 'stats', 'trophy',
          'TransitionOut'] + [f'mrk_{k}_{i}' for k in ('race', 'slopestyle', 'superpipe', 'bigair', 'racebackcountry', 'freestylebackcountry') for i in range(5)] \
       + [f'txtmdl_{i}' for i in range(6)] + [f'mdl_{i}' for i in range(5)] + [f'mdl_back{i}' for i in range(5)] + [f'{i}check' for i in range(1, 6)] \
       + [f'{i}nocheck' for i in range(1, 6)] + [f'{i}text blocks' for i in range(5)] + [f'event_{i}' for i in range(5)] + [f'Numb {i}' for i in range(1, 4)]
NAMES += [chr(48 + i) for i in range(11)] + ['Zoe', 'Mac', 'Moby', 'Psymon', 'Viggo', 'Elise', 'Kaori', 'Griff', 'Nate', 'Allegra']
ORDER = [4, 0, 8, 5, 9, 6, 7, 3, 2, 1]            # 0x440F68: screen index -> CHARDB id
# Profile texts per rider (first name as in the locale keys, 0x45E998 pointer table order).
PROFILE_NAMES = {'zoe': 'Zoe', 'moby': 'Moby', 'psymon': 'Psymon', 'griff': 'Griff', 'viggo': 'Viggo', 'elise': 'Elise', 'nate': 'Nate',
                 'mac': 'Mac', 'allegra': 'Allegra', 'kaori': 'Kaori'}
# Locale strings the code puts on these screens (FEAMER / OVAMER / CMNAMER hashes).
STRINGS = {
    'rider_dna': 0x0A3E94A1, 'rider_faves': 0x0A3E94A2, 'rider_qna': 0x0A3E94A3, 'rider_bio': 0x0A3E94A4,
    'help_radio_big': 0x026F4CB7, 'help_ambience': 0x0E1BDD51, 'help_custom_dj': 0x0FD5706A, 'help_custom_nodj': 0x0573AC2A,
    'help_edit_playlist': 0x01ECAF61, 'help_no_playlist': 0x0D4187ED,
    'buy_trick_ctm': 0x05C18D6D, 'buy_item_ctm': 0x0B6490ED, 'cost': 0x05F1A304, 'you_have': 0x0918C5A5,
    'choose_uber_category': 0x0303ACE4, 'customize_ubers': 0x0E018922, 'no_gear': 0x03AAC20D,
    'insert_card': 0x0BEB0B42, 'loading_card': 0x0A953DD3, 'load_complete': 0x06180D25, 'empty': 0x0A9653D9, 'loading': 0x0EFAE167,
    'save_options_q': 0x087EE023, 'yes': 0x0382DE83, 'no': 0x0A382D4F,
    'enter_player_name': 0x0DB80995,
    'trophy_room': 0x08C2C71D,
    'help_view_pass': 0x085981A3,
    'help_view_trophies': 0x00765D63,
    'locked_peak2': 0x096ABC62,
    'locked_peak3': 0x096ABC63,
    'peak1_name': 0x09F4CD55,
    'peak2_name': 0x09FDCD55,
    'peak3_name': 0x09FECD55,
    'help_select_goal': 0x0E419A1C,
    'help_locked_goal': 0x0F487C9C,
    'race_trophy': 0x04854A59,
    'fs_trophy': 0x09775CB9,
    'explore_trophy': 0x016ECCB9,
    'earnings_trophy': 0x0A0C9B99,
    'earnings': 0x0F8650A3,
    'collectibles': 0x0EC5D203,
    'big_challenges': 0x00E6B603,
    'race_caps': 0x032D0353,
    'pk1_race': 0x017E4E35,
    'pk2_race': 0x017F4E35,
    'all_peak_race': 0x0307D4D5,
    'pk1_jam': 0x0D17ECFD,
    'pk2_jam': 0x0D17FCFD,
    'best_time': 0x0B8A0895,
    'best_score': 0x08A32DE5,
    'you_earned': 0x056E421D,
    'collect_num': 0x072CDDDD,
    'chal_num': 0x0FF513BD,
    'all_peak_jam': 0x0A2FB55D,
 'player_name': 0x0ED59295, 'enter_cheat': 0x02534344, 'enter_cheat_help': 0x047D2625,
}
FACES = ['moby', 'kaori', 'allegra', 'mac', 'zoe', 'griff', 'elise', 'nate', 'psymon', 'viggo']  # 0x440F78 <name>face


def decode(data, sprites, strings):
    sc = Screen.decode(data); elements, by = [], {}
    for index, (d, s) in enumerate(zip(sc.definitions, sc.states[0].records)):
        kind, _ = struct.unpack_from('<HH', d, 0); name, flags = struct.unpack_from('<II', d, 4); tail = d[32:]
        e = dict(index=index, name=f'{name:08x}', kind=KIND.get(kind, hex(kind)), layer=flags & 0x3F, flags=flags,
                 words=[struct.unpack_from('<h', d, 12 + 2 * k)[0] for k in range(10)])
        label = next((n for n in NAMES if H(n) == name), None)
        if label: e['label'] = label
        if kind in (0x10, 0x13):
            n = struct.unpack_from('<I', tail, 0)[0]; e['children'] = [f'{c:08x}' for c in struct.unpack_from('<%dI' % n, tail, 4)]
        elif kind == 0x11: e['sprite'] = sprites.get(struct.unpack_from('<I', tail, 4)[0])
        elif kind in (0x17, 0x20) and len(tail) >= 8:
            key = struct.unpack_from('<I', tail, 4)[0]; e['text_hash'] = f'{key:08x}'; e['text'] = strings.get(key)
        elif kind == 0x12: e['text'] = None
        elif kind == 0x15:                        # option row: u32 label element, u32 value element (positioned in the row)
            a, b = struct.unpack_from('<II', tail, 0); e['children'] = [f'{a:08x}', f'{b:08x}']
        elif kind == 0x18: e['shape'] = list(tail)
        elif kind == 0x19:
            a, b = struct.unpack_from('<II', tail, 0); e['bar'] = dict(background=f'{a:08x}', fill=f'{b:08x}')
        record = struct.unpack_from('<H', s, 0)[0]
        if record == 0x21: e['props'] = props(s)
        elif record == 0x20:
            anim, _, mode = struct.unpack_from('<III', s, 4); e['anim'] = dict(hash=f'{anim:08x}', mode=mode)
        elements.append(e); by[e['name']] = e
    for e in elements:
        for c in e.get('children', []):
            if c in by and e['kind'] in ('group', 'option'): by[c]['parent'] = e['name']
            elif c in by: by[c].setdefault('menu', e['name'])
    events, labels = [], []
    for state in sc.states[1:]:
        lab = dict(frame=state.flags, name=f'{state.name:08x}', control=[])
        label = next((n for n in NAMES if H(n) == state.name), None)
        if label: lab['label'] = label
        for r in state.records:
            k = struct.unpack_from('<H', r, 0)[0]
            if k == 0x20:
                anim, el, mode = struct.unpack_from('<III', r, 4); events.append(dict(frame=state.flags, anim=f'{anim:08x}', element=f'{el:08x}', mode=mode))
            elif k == 0x21: events.append(dict(frame=state.flags, element=f'{struct.unpack_from("<I", r, 4)[0]:08x}', props=props(r)))
            else: lab['control'].append(r.hex())
        labels.append(lab)
    return elements, events, labels


def main():
    disc = Disc(ISO)
    try:
        lui = disc.file('DATA/UI/FE.LUI'); ssh = disc.file('DATA/UI/FE_1.SSH'); strings = {}
        for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
            for k, v in loc_file.entries(disc.file(f'DATA/LOCALE/{name}.LOC')).items(): strings.setdefault(k, v)
        credit_strings = loc_file.entries(disc.file('DATA/LOCALE/CRAMER.LOC'))
    finally:
        disc.close()
    pages = shps_pages(ssh); sprites = {}; used = set()
    for o in lui_objects(lui):
        size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
        sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'FE_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
    screens_raw, animations = lui_screens(lui), lui_animations(lui); screens = {}
    for key, h in SCREENS.items():
        elements, events, labels = decode(screens_raw[h], sprites, strings)
        for e in elements:
            if e.get('sprite'): used.add(e['sprite']['page'])
        anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
        screens[key] = dict(hash=f'{h:08x}', elements=elements, events=events, labels=labels, animations={k: animations[k] for k in sorted(anims) if k in animations})
    # Faces for Select Cheat Character: entry 0 = the base rider (0x440F78 '<name>face'), then RWRDPS2 cheat images.
    faces = {}
    for n in FACES + ['brodi', 'eddie', 'jp', 'luther', 'marisol', 'marty', 'seeiah', 'hiro', 'jurgen', 'nakedluther', 'stretch', 'bessy', 'bunnysan',
                      'churchill', 'skeleton', 'snowman', 'yeti', 'unknown', 'beaver', 'abominable']:
        s = sprites.get(H(n + 'face'))
        if not s: raise ValueError(f'No face sprite {n}face')
        faces[n + 'face'] = s; used.add(s['page'])
    # Trophies (125mountainroom / 126peakroom): the FE textures the code picks by name (0x398380): the trophy thumbnails and medal
    # icons are named by the RWRDPS2 entries' third word (0x156A90 / 0x156AE0 +8: trc1 .. ter3, racp .. ernb), the markers
    # 'dot_visit arrow' (medal earned) / 'dot_loc arrow' (0x1D2880).
    trophy_sprites = {}
    for n in [k + str(p) for k in ('trc', 'tfs', 'tex', 'ter') for p in (1, 2, 3)] + [k + m for k in ('rac', 'fst', 'exp', 'ern') for m in 'pgsb'] + ['dot_visit arrow', 'dot_loc arrow']:
        s = sprites.get(H(n))
        if not s: raise ValueError(f'No trophy sprite {n}')
        trophy_sprites[n] = s; used.add(s['page'])
    # Rider Profile pages: DNA 1..8, FAVES 1..12, Q&A 1..4, full bio (CMNAMER/FEAMER keys hashed like 0x317670).
    profiles = {}
    for rid, first in PROFILE_NAMES.items():
        get = lambda key: strings.get(H(key))
        profiles[rid] = dict(dna=[get(f'kT_DNA{i}{first}') for i in range(1, 9)], faves=[get(f'kT_FAVES{i}{first}') for i in range(1, 13)],
                             qna=[get(f'kT_QNA{i}{first}') for i in range(1, 5)], bio=get(f'kT_FULLBIO1{first}'))
        if not all(profiles[rid]['faves']) or not all(profiles[rid]['qna']) or not all(profiles[rid]['dna']): raise ValueError(f'Profile text missing for {first}')
    try:
        import rider_bio   # tools/sam_ps2: the Sam build's own profile (its FEAMER entries)
        profiles['sam'] = dict(dna=list(rider_bio.DNA), faves=list(rider_bio.FAVES), qna=list(rider_bio.QNA), bio=rider_bio.BIO)
    except ImportError: pass
    texts = {name: strings.get(key) for name, key in STRINGS.items()}
    texts.update({key: strings.get(H(key)) for key in OPTION_KEYS})
    credits = [credit_strings.get(H('kT_CREDITS' + k)) for k in CREDITS]
    if any(c is None for c in credits): raise ValueError('Missing credit lines in CRAMER.LOC')
    if not all(texts.values()): raise ValueError(f'Missing strings: {[k for k, v in texts.items() if not v]}')
    # Trick score table 0x530600 (s32 begin, s32 hold per score id; filled at boot, read-only here) from Zoe's derived countdown.
    uber_points = None
    state = ROOT / 'local/reference/pcsx2/characters/zoe/countdown.p2s'
    if state.exists():
        import zipfile
        memory = zipfile.ZipFile(state).read('eeMemory.bin')
        uber_points = [list(struct.unpack_from('<2i', memory, 0x530600 + 8 * i)) for i in range(85)]
    # Sam's silhouettes, made from his own model like the originals (tools/export_sam_roster.py); the Sam PS2 build's
    # atlas (half GS alpha, a generated mask) is only the fallback when his pose renders are missing.
    import export_sam_roster
    try: sam_layout = export_sam_roster.export(out=OUT / 'sam-roster.png')
    except FileNotFoundError:
        sam_layout = None
        atlas = ROOT / 'local/sam-ps2/roster/sam-roster-atlas.png'
        if atlas.exists(): shutil.copy2(atlas, OUT / 'sam-roster.png')
    sam = OUT / 'sam-roster.png'
    for page in sorted(used):
        if not (ASSETS_UI / f'{page}.png').exists(): raise ValueError(f'{page}.png missing: run web/prepare-ui.py')
    result = dict(
        provenance=dict(fe_lui_sha256=hashlib.sha256(lui).hexdigest(), fe_1_ssh_sha256=hashlib.sha256(ssh).hexdigest(),
                        screens={k: f'0x{v:08X}' for k, v in SCREENS.items()}, tool='tools/export_character_select.py'),
        coordinates='LUI 640x480 frame; props 0 x, 1 y, 3/4 pivot, 5 rot, 6 w, 7 h, 9/10 scale %, 12 anchor, 13..16 ARGB; shapes: byte 0 vertices, '
                    'byte 1 triangles, triangle indices from byte 4, vertex k at props 21+9k x, 22+9k y, 26..29+9k ARGB; child x,y relative to the parent group',
        order=ORDER, faces=faces, trophy_sprites=trophy_sprites, pages=sorted(used), sam_roster='sam-roster.png' if sam.exists() else None,
        sam=sam_layout or dict(screen_index=10, silhouette=[514, 270, 26, 74], white_uv=[1, 1.5, 28, 74.5], orange_uv=[33, 1.5, 60, 74.5], right_arrow_x=556,
                               source='tools/sam_ps2/build_roster_ui_candidate.py (--roster-icons) + extend_roster_texture.py'),
        profiles=profiles, strings=texts, credits=credits,
        uber_points=dict(address='0x530600', stride=8, source='local/reference/pcsx2/characters/zoe/countdown.p2s', table=uber_points) if uber_points else None,
        screens=screens)
    (OUT / 'character-select.json').write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(f"Select Character: {', '.join(f'{k} {len(v['elements'])}' for k, v in screens.items())}, faces {len(faces)}, pages {sorted(used)} -> {OUT / 'character-select.json'}")


if __name__ == '__main__':
    main()
