#!/usr/bin/env python3
"""The selectable roster for the browser: web/public/assets/riders.json (read-only; docs/characters.md).

Original data (the user's own disc, never modified):
* the ten riders of DATA/BE/CHARDB.DBL (10 x 0x88, copied to 0x530970 by 0x149C84) in the Select Character
  order table 0x440F68 = [4,0,8,5,9,6,7,3,2,1]; names kT_CHAR<Name> (CMNAMER, getter 0x14EEC8), the card text
  of 08sel_char widget 0x69020+i (FEAMER, hash in the widget definition), kT_FULLBIO1<Name> and kT_DNA1..8<Name>;
* the twenty cheat characters (ids 10..29): DATA/BE/RWRDPS2.DAT cheat_character records (name, face image, lodge
  price/peak, LOCKED help key kT_CHTHELP*), the 8-character short names at 0x43FA38 + id*8, and the Options >
  Enter Cheat codes checked by 0x187D38 (text lowercased, hashed with 0x317670 = loc_file.name_hash, one code per
  character, unlocking it for every rider of profile 0; JP, Marisol, Seeiah and Far East Myth have none, and the
  text of Canhuck's code 0x0DD36D88 is not recovered);
* the front-end clips of every rider (fe.afl FE_GEAR_<PREFIX>_CYC idle, FE_CHARSEL_<NAME> cheer; semantics
  434/435, variant masks 0x19EB08) and its IRR.DAT preview lighting record (0x19EE88; Nate and Psymon use Elise's).

Sam is the port's own rider (sam_character/, config/characters/sam.json): the eleventh entry, as in the Sam PS2
build (tools/sam_ps2), with Mac's gameplay slot (web/career.js RIDER_CHARACTER).
"""
import json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from export_career import Elf, rewards  # noqa: E402
import loc_file  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = ROOT / 'web/public/assets/riders.json'
ORDER_TABLE = 0x440F68
SHORT_NAMES = 0x43FA38
H = loc_file.name_hash
# Card text key hashes: 08sel_char description widgets 0x69020 + screen index (definition +36).
CARD = ['01aa3455', '0aa295b9', '022af91e', '0a1b8c86', '0a29fe9f', '0a2133d5', '0aa21885', '01aa2673', '0531eaa1', '0a2485c9']
# CHARDB id -> web id, model prefix, fe.afl idle suffix, IRR.DAT record (0x19EE88 clamps Nate/Psymon to Elise).
BASE = {0: ('moby', 'moby', 'MOBY', 'moby'), 1: ('kaori', 'kaori', 'KAORI', 'kaori'), 2: ('allegra', 'arielle', 'ARIELLE', 'allegra'),
        3: ('mac', 'mac', 'MAC', 'mac'), 4: ('zoe', 'zoe', 'ZOE', 'zoe'), 5: ('griff', 'grommet', 'GROMMET', 'griff'),
        6: ('elise', 'elise', 'ELISE', 'elise'), 7: ('nate', 'rocco', 'ROCCO', 'elise'), 8: ('psymon', 'psymon', 'PSYMON', 'elise'),
        9: ('viggo', 'deiter', 'DEITER', 'viggo')}
# Cheat id -> web id, model prefix, Enter Cheat code text (None: no code / text not recovered).
CHEATS = {10: ('brodi', 'brodi', 'zenmaster'), 11: ('eddie', 'eddie', 'worm'), 12: ('jp', 'jp', None), 13: ('luther', 'luther', 'bronco'),
          14: ('marisol', 'marisol', None), 15: ('marty', 'marty', 'back2future'), 16: ('seeiah', 'seeiah', None), 17: ('hiro', 'hiro', 'slicksuit'),
          18: ('jurgen', 'jurgen', 'brokenleg'), 19: ('sveltluther', 'luthern', 'notsosvelte'), 20: ('stretch', 'stretch', 'windmilldunk'),
          21: ('cudmore', 'bessy', 'milkemdaisy'), 22: ('bunnysan', 'bunny', 'wheresyourtail'), 23: ('churchill', 'churchill', 'tankengine'),
          24: ('gutless', 'skel', 'boneyardreject'), 25: ('snowballs', 'snowman', 'betyouveneverseen'), 26: ('nwlegend', 'yeti', 'callhimgeorge'),
          27: ('unknownrider', 'unknown', 'finallymadeitin'), 28: ('canhuck', 'beaver', None), 29: ('fareastmyth', 'abom', None)}
CODE_RANGE = (0x187D38, 0x188400)   # Enter Cheat handler: the code hashes are lui/ori immediates in here


def code_constants(elf):
    """32-bit constants built by lui/ori(addiu) pairs in the Enter Cheat handler."""
    words = elf.words(CODE_RANGE[0], (CODE_RANGE[1] - CODE_RANGE[0]) // 4, 'I'); out = set(); hi = {}
    for w in words:
        op, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == 0x0F: hi[rt] = imm << 16
        elif op in (0x0D, 0x09) and rs in hi: out.add((hi[rs] | imm) if op == 0x0D else (hi[rs] + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF)
    return out


def main():
    disc = Disc(ISO)
    try:
        elf = Elf(disc.file('SLUS_207.72')); chardb = disc.file('DATA/BE/CHARDB.DBL'); catalog = rewards(disc.file('DATA/BE/RWRDPS2.DAT'))
        fe = loc_file.entries(disc.file('DATA/LOCALE/FEAMER.LOC')); cmn = loc_file.entries(disc.file('DATA/LOCALE/CMNAMER.LOC'))
    finally:
        disc.close()
    order = list(elf.read(ORDER_TABLE, 10))
    if order != [4, 0, 8, 5, 9, 6, 7, 3, 2, 1]: raise ValueError('Unexpected Select Character order table')
    text = lambda at, n, row: row[at:at + n].split(b'\0')[0].decode('latin1')
    riders = []
    for index, cid in enumerate(order):
        row = chardb[cid * 0x88:(cid + 1) * 0x88]; first = text(0x20, 16, row); web, prefix, idle, irr = BASE[cid]
        weight, stance, size = struct.unpack_from('<3I', row, 0x40)
        package = WEB_PACKAGE(web)
        riders.append(dict(
            id=web, name=cmn[H(f'kT_CHAR{first}')], package=package, kind='rider', character=cid, screen_index=index,
            long_name=text(0, 32, row), nickname=text(0x30, 16, row), weight=weight, stance='goofy' if stance else 'regular', model_size=size,
            female=bool(row[0x5C]), age=struct.unpack_from('<I', row, 0x60)[0], height=text(0x64, 16, row).strip('"').replace('""', '"'), nationality=text(0x74, 16, row).strip(),
            card=[fe[int(CARD[index], 16)]], bio=fe[H(f'kT_FULLBIO1{first}')], dna=[fe[H(f'kT_DNA{k}{first}')] for k in range(1, 9)],
            fe=dict(idle=f'FE_GEAR_{idle}_CYC', cheer=f'FE_CHARSEL_{first.upper()}', irradiance=irr), resource_prefix=prefix,
            stats=dict(raw=[5] * 7, limit=55)))
    # Sam: the port's own rider, the eleventh Select Character entry (Sam PS2 build: screen index 10, label frame 90).
    sam = json.loads((ROOT / 'config/characters/sam.json').read_text())
    ident = sam['identity']; inches = ident['height_inches']
    riders.append(dict(id='sam', name=sam['display_name'], package='RIDER_SAM', kind='custom', character=3, screen_index=10, nickname=sam.get('nickname'),
                       stance=ident['stance'], height=f"{inches // 12}'{inches % 12}\"", weight_lb=ident['weight_lb'], weight=70, model_size=96,
                       template=dict(scale='elise (same height 5\'11\" = model size 96)', character='mac', rule='tools/export_characters.py sam_settings'), card=['A chopped unc from Wisconsin. Prefers uphill', 'and keeps his head above his board.'],
                       bio=' '.join(sam['bio_short'].split()), fe=dict(idle='FE_GEAR_MAC_CYC', cheer='FE_CHARSEL_MAC', irradiance='fe_map'),
                       stats=dict(raw=[5] * 7, limit=55), source='sam_character/ (the port\'s own rider; Zoe\'s gameplay profile, Mac\'s career slot)'))
    constants = code_constants(elf); items = catalog['cheat_character']['items']
    for item in items:
        cid = item['character']; web, prefix, code = CHEATS[cid]
        if code is not None and H(code) not in constants: raise ValueError(f'Cheat code {code} not in the Enter Cheat handler')
        riders.append(dict(
            id=web, name=item['name'], short_name=text(0, 8, elf.read(SHORT_NAMES + cid * 8, 8)), package=WEB_PACKAGE(web), kind='cheat', character=cid,
            face=item['image'] + 'face' if not item['image'].endswith('face') else item['image'], resource_prefix=prefix,
            unlock=dict(help=fe[H(item['help'])], price=item.get('price'), peak=item.get('peak'), code=code, save_bit=cid - 10),
            card=[], bio=''))
    for r in riders:   # web/character-roster.js: no settings.json = the course initial.json (Zoe, Sam)
        r['settings'] = (ROOT / 'web/public/assets' / r['package'] / 'settings.json').exists()
    OUT.write_text(json.dumps(riders, indent=1, ensure_ascii=False) + '\n')
    print(f'riders.json: {sum(r["kind"] != "cheat" for r in riders)} Select Character entries, {sum(r["kind"] == "cheat" for r in riders)} cheat characters -> {OUT}')


def WEB_PACKAGE(web): return f'RIDER_{web.upper()}'


if __name__ == '__main__':
    main()
