#!/usr/bin/env python3
"""Conquer the Mountain Message Center data for the browser (web/career-messages.js; docs/characters.md
"Relationship messages").

Sources (read-only): the ELF, DATA/UI/OV.LUI + OV_1.SSH, the locale files, and the runtime message database of a
Conquer the Mountain savestate (local/ps2-capture/menus/ctm/state-mcomm.p2s):

  0x441630  61 categories x 20 bytes {type, first item, count, kind, fixed folder}; counts set by 0x1E12F0,
            first = prefix sums. Kind 1 backcountry reminder, 2 FAQ folder, 3 backcountry result, 4 rival challenge,
            5 aggression (the relationship messages, categories 12 + character), 6 peak challenge, 7 flag only,
            8 beat the peak (categories 52..60; 59/60 = items 254/255).
  0x4C6C08  256 records x 0x18 {item, category, flag, sender (0..9 rider, 10 game title, 11 DJ, 12 folder),
            subject key hash, body key hash}; built at boot (0x2155xx).
  inbox     profile 0x4A6CA8 + bank*0x9B50 + char*0xF88 + 0xE38: 25 x {item, variant}, +0xC8 read bits,
            +0xCC count, +0xD0.. per-category posted bits (0x147908 / 0x147980 / 0x147A30).
Output: web/public/assets/CAREER/messages.json.
"""
import hashlib, json, re, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from export_character_select import H, ISO, decode  # noqa: E402
from inspect_disc import Disc  # noqa: E402
from export_loading_screen import lui_screens, lui_objects, shps_pages  # noqa: E402
import loc_file  # noqa: E402

STATE = ROOT / 'local/ps2-capture/menus/ctm/state-mcomm.p2s'
OUT = ROOT / 'web/public/assets/CAREER/messages.json'
KEYS = dict(folder='kT_FAQFolder', view='kT_BTNView', delete='kT_BTNDelete', previous='kT_BTNPrevious', select='kT_BTNSelect',
            keep='kT_BTNKeepMsg', collapse='kT_BTNCollapse', expand='kT_BTNExpand', unread='kT_OVRCMNUnread', total='kT_OVRCMNTotalMsg', title='kT_TITLEMessages', help='kT_OVRHELPMessages')
HASHED = dict(select_message=0x6059147, message_center=0x77B81C2, message_number=0xBBAAF2F, from_label=0x7372A9E, subject_label=0xCF3679E,
              delete_message=0xCC07BB7)


def main():
    d = Disc(ISO); strings = {}
    try:
        lui = d.file('DATA/UI/OV.LUI'); ssh = d.file('DATA/UI/OV_1.SSH')
        for n in ('FEAMER', 'OVAMER', 'CMNAMER'):
            for k, v in loc_file.entries(d.file(f'DATA/LOCALE/{n}.LOC')).items(): strings.setdefault(k, v)
    finally:
        d.close()
    memory = zipfile.ZipFile(STATE).read('eeMemory.bin')
    u = lambda a: struct.unpack_from('<I', memory, a)[0]; i32 = lambda a: struct.unpack_from('<i', memory, a)[0]
    categories = [dict(type=i32(0x441630 + 20 * c), first=i32(0x441630 + 20 * c + 4), count=i32(0x441630 + 20 * c + 8), kind=i32(0x441630 + 20 * c + 12),
                       folder=i32(0x441630 + 20 * c + 16)) for c in range(61)]
    total = categories[-1]['first'] + categories[-1]['count']
    records = []
    for k in range(total):
        item, category, flag, sender, subject, body = (u(0x4C6C08 + 0x18 * k + 4 * j) for j in range(6))
        if item != k: raise ValueError('message database order')
        records.append(dict(item=item, category=category, flag=flag, sender=sender, subject=strings.get(subject), body=strings.get(body),
                            subject_hash=f'{subject:08x}', body_hash=f'{body:08x}'))
    # the aggression subjects kT_MSGSubjectAggression0..10 (0x1E4F80 formats kind-5 subjects with the variant r % 11)
    subjects = dict(aggression=[strings.get(H(f'kT_MSGSubjectAggression{v}')) for v in range(11)],
                    bc_reminder=[strings.get(H('kT_MSGSubjectBCReminder'))],
                    foreshadow=[strings.get(H(f'kT_MSGSubjectForeshadow{v}')) for v in range(10)],        # kind 4: r % 10
                    beat_the_peak=[strings.get(H(f'kT_MSGSubjectBeatThePeak{v}')) for v in range(4)],     # kind 8: r & 3
                    bc_win=[strings.get(H(f'kT_MSGSubjectBCFinishWin{v}')) for v in range(10)],           # kind 3: r % 10
                    bc_loss=[strings.get(H(f'kT_MSGSubjectBCFinishLoss{v}')) for v in range(10)])
    if not all(all(v) for v in subjects.values()): raise ValueError('subjects missing')
    # 0x1E4F80: kind-3 items 0x73.. pick Win or Loss by the jump table 0x46E1E0 (0x1E4FD0 Loss, 0x1E4FE0 Win).
    elfdata = (ROOT / 'local/disc/SLUS_207.72').read_bytes(); from export_career import Elf  # noqa: E402
    elfr = Elf(elfdata)
    bc_win_items = [0x73 + k for k in range(24) if int.from_bytes(elfr.read(0x46E1E0 + 4 * k, 4), 'little') == 0x1E4FE0]
    # 0x1E2EA0: the From name of senders 0..12 (key strings 0x46DE60.., jump table 0x46DF30).
    senders = [strings.get(H(elfr.read(0x46DE60 + 16 * k, 16).split(b'\0')[0].decode())) for k in range(13)]
    texts = {k: strings.get(H(v)) for k, v in KEYS.items()}
    texts.update({k: strings.get(h) for k, h in HASHED.items()})
    if not all(texts.values()): raise ValueError(f'missing texts {[k for k, v in texts.items() if not v]}')
    names = [strings.get(H(f'kT_CHAR{n}')) for n in ('Moby', 'Kaori', 'Allegra', 'Mac', 'Zoe', 'Griff', 'Elise', 'Nate', 'Psymon', 'Viggo')]
    pages = shps_pages(ssh); sprites = {}
    for o in lui_objects(lui):
        size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
        sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'OV_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
    screens = lui_screens(lui); layout = {}
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes(); names_by_hash = {}
    for m in re.finditer(rb'[ -~]{2,40}', elf): names_by_hash.setdefault(H(m.group().decode()), m.group().decode())
    for key in ('112messagecenter', '113ViewMessage'):
        elements, events, labels = decode(screens[H(key)], sprites, strings)
        for e in elements:
            n = names_by_hash.get(int(e['name'], 16))
            if n: e['label'] = n
        layout[key] = elements
    result = dict(categories=categories, records=records, subjects=subjects, texts=texts, names=names, senders=senders, bc_win_items=bc_win_items,
                  sprites=dict(mail_icon=sprites[H('mail_icon')]),
                  hud=dict(icon='mail_icon', x=15, y=384, seconds=5.0, blink=0.5, colors=[[1, 1, 1], [0.8613290190696716, 0.38124799728393555, 0.0]],
                           source='HUD event 8 (0x1EC3C4) -> +0x160; 0x1EB6E4: +0x164 phase += 1/60 (wraps at 1), +0x168 += 1/60 until 5.0; '
                                  '0x1F0F3C draws mail_icon (handle +0x4A4) with 0x4C8788 (1,1,1) while phase <= 0.5, else the orange of 0x4C87A8 {1, .861, .381, 0} taken as rgb from its words 1..3 (matches the PS2 frame); '
                                  'position measured on the PS2 frame (lineup-messages / mail capture)'),
                  layout=layout,
                  provenance=dict(state=str(STATE.relative_to(ROOT)), ee_sha256=hashlib.sha256(memory).hexdigest(), tool='tools/export_messages.py'))
    OUT.write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    print(json.dumps(dict(output=str(OUT.relative_to(ROOT)), records=len(records), categories=len(categories), bytes=OUT.stat().st_size)))


if __name__ == '__main__':
    main()
