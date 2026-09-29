#!/usr/bin/env python3
"""Export the original Conquer the Mountain / event tables for the browser (read-only).

Everything comes from the user's own disc: the course table and career rule tables
from SLUS_207.72 and the front-end/overlay/common locale files.  The output
(web/public/assets/CAREER/career.json) is git-ignored like every other extracted asset.
See docs/career-events.md for the source addresses and meaning of each table.
"""
import argparse, hashlib, json, struct, sys, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
import loc_file  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
ELF_SHA256 = None  # recorded in the output provenance


class Elf:
    def __init__(self, data):
        self.data = data
        phoff = struct.unpack_from('<I', data, 28)[0]; count = struct.unpack_from('<H', data, 44)[0]
        self.segments = []
        for i in range(count):
            kind, offset, vaddr, _, filesz, _, _, _ = struct.unpack_from('<8I', data, phoff + 32 * i)
            if kind == 1: self.segments.append((offset, vaddr, filesz))

    def read(self, address, size):
        for offset, vaddr, filesz in self.segments:
            if vaddr <= address and address + size <= vaddr + filesz:
                return self.data[offset + address - vaddr: offset + address - vaddr + size]
        raise ValueError(f'{address:#x} is not mapped')

    def words(self, address, count, fmt='i'): return list(struct.unpack('<%d%s' % (count, fmt), self.read(address, 4 * count)))
    def text(self, address, size): return self.read(address, size).split(b'\0')[0].decode('latin1')


# Course table 0x43D950 (stride 0x64), read by the course lookups at 0x144B4C..0x144CCC.
COURSE_TABLE, COURSE_STRIDE, COURSE_COUNT = 0x43D950, 0x64, 23
EVENT_KIND = {'RA': 'race', 'SS': 'slopestyle', 'BA': 'bigair', 'HP': 'superpipe', 'BC': 'backcountry'}

# ---------------------------------------------------------------------------------------------
# Career rule tables (addresses recovered from the game-mode handlers; see docs/career-events.md)
GAME_MODES, GAME_MODE_STRIDE = 0x43E978, 0x54           # 14 x {event type, name}
GOAL_EVENTS = 0x45AAD8                                   # [peak][goal RACE/FREESTYLE][5] x (s8 mode, s8 course), -1 ends
CASH = 0x4405A0                                          # 17 x {course, platinum, gold, silver, bronze} (x100 dollars)
PLATINUM = 0x440E80                                      # 20 x {s16 course, s16 value}
FREESTYLE_AI = 0x440B38                                  # 30 x {s16 course, round, aiScore[5] (x100), time limit s}
AI_SCALE = (0x49C2D8, 0x49C2DC, 0x49C2E0)                # level-0 scale, jitter factor, level-2 scale (floats, gp-0x6E18..)
PEAK_CHALLENGES = 0x440D18                               # 18 x 10 u16 (modes 6..11, tier 1..3)
EARNINGS_GOAL, COLLECTIBLE_MEDALS, CHALLENGE_MEDALS = 0x45B048, 0x45AFE8, 0x45B018
ATTRIBUTE_COST = 0x440550                                # 10 x {level index, cost}
RECORD_DEFAULTS, RECORD_COUNT = 0x43FB28, 130            # 26 slots x 5 x {value, character, name[12]} (copied to 0x535C18)
RECORD_SLOTS = 0x45A2F8                                  # 17 x {mode, slot, mode, slot}; 26 = none
# Instruction immediates checked in place (address, expected word): race results 0x23A760
CODE_CONSTANTS = {
    'race_dnf_ticks': ((0x23A860, 0x3C040005), (0x23A868, 0x34847E40)),     # lui/ori a0 = 0x57E40 (360000)
    'race_level_up_margin_round1': ((0x23AA48, 0x2C420259),),              # sltiu 601
    'race_level_up_margin_round2': ((0x23AA70, 0x2C42012D),),              # sltiu 301
    'race_level_up_margin_round3': ((0x23AA98, 0x2C4200B5),),              # sltiu 181
    'estimate_min_speed': ((0x122DA0, 0x3C0141F0),),                       # lui at,0x41F0 (30.0 cm/tick) in 0x122D78
}


def f32(elf, address): return struct.unpack('<f', elf.read(address, 4))[0]


def characters(disc):
    data = disc.file('DATA/BE/CHARDB.DBL'); out = []
    for i in range(len(data) // 0x88):
        r = data[0x88 * i:0x88 * (i + 1)]
        out.append(dict(index=i, name=r[:0x20].split(b'\0')[0].decode('latin1'), first=r[0x20:0x30].split(b'\0')[0].decode('latin1')))
    return out


def rules(elf):
    for name, words in CODE_CONSTANTS.items():
        for address, word in words:
            if elf.words(address, 1, 'I')[0] != word: raise ValueError(f'{name}: unexpected instruction at {address:#x}')
    code = lambda a: elf.words(a, 1, 'I')[0] & 0xFFFF
    modes = [dict(index=i, event_type=elf.words(GAME_MODES + GAME_MODE_STRIDE * i, 1)[0], name=elf.text(GAME_MODES + GAME_MODE_STRIDE * i + 4, 40)) for i in range(14)]
    goals = []
    for peak in range(3):
        for goal in range(2):
            events = []
            for k in range(5):
                mode, course = struct.unpack('bb', elf.read(GOAL_EVENTS + 40 * peak + 10 * goal + 2 * k, 2))
                if mode < 0: break
                events.append(dict(mode=mode, course=course if mode < 6 else None))
            goals.append(dict(peak=peak + 1, goal=('race', 'freestyle')[goal], events=events))
    cash = []
    for i in range(17):
        course, *values = elf.words(CASH + 20 * i, 5)
        if course != i: raise ValueError('cash table order')
        cash.append([100 * v for v in values])
    platinum = [list(struct.unpack('<2h', elf.read(PLATINUM + 4 * i, 4))) for i in range(20)]
    ai = []
    for i in range(30):
        v = struct.unpack('<8h', elf.read(FREESTYLE_AI + 16 * i, 16))
        ai.append(dict(course=v[0], round=v[1], scores=[100 * x for x in v[2:7]], time_limit=v[7]))
    peak_challenges = [list(struct.unpack('<10H', elf.read(PEAK_CHALLENGES + 20 * i, 20))) for i in range(18)]
    records = []
    for i in range(RECORD_COUNT):
        value, character = elf.words(RECORD_DEFAULTS + 20 * i, 2)
        records.append(dict(value=value, character=character, name=elf.text(RECORD_DEFAULTS + 20 * i + 8, 12)))
    slots = [elf.words(RECORD_SLOTS + 16 * i, 4) for i in range(17)]
    return dict(
        game_modes=modes, goal_events=goals, cash=cash, platinum=platinum, freestyle_ai=ai,
        ai_score=dict(level0_scale=f32(elf, AI_SCALE[0]), jitter=f32(elf, AI_SCALE[1]), level2_scale=f32(elf, AI_SCALE[2])),
        peak_challenges=peak_challenges,
        earnings_goal=elf.words(EARNINGS_GOAL, 3), collectible_medals=[elf.words(COLLECTIBLE_MEDALS + 16 * p, 4) for p in range(3)],
        challenge_medals=[elf.words(CHALLENGE_MEDALS + 16 * p, 4) for p in range(3)],
        attribute_cost=[elf.words(ATTRIBUTE_COST + 8 * i, 2)[1] for i in range(10)],
        records=[records[5 * s:5 * s + 5] for s in range(26)], record_slots=slots,
        race=dict(dnf_ticks=(code(0x23A860) << 16) | code(0x23A868),
                  level_up_margin=[code(0x23AA48), code(0x23AA70), code(0x23AA98)],
                  estimate_min_speed=struct.unpack('<f', struct.pack('<I', code(0x122DA0) << 16))[0]))


# DATA/BE/RWRDPS2.DAT, loaded by 0x15AD58 and parsed by 0x15A818: nine sections {u32 type, u32 count, records},
# then u32 pool size and a string pool (string fields are 1-based pool offsets). Shop records (types 3..8) hold
# s16 price/10 at +0xC (-1 = not for sale) and s8 lodge peak at +0xE; cheat characters also u8 character id +0xF.
REWARD_SECTIONS = ('trophy', 'medal', 'peak_pass', 'poster', 'trading_card', 'art', 'video', 'toy', 'cheat_character')
REWARD_SIZES = (12, 12, 8, 16, 16, 16, 16, 16, 16)
REWARD_SAVE = {'poster': 0xF30, 'trading_card': 0xF36, 'art': 0xF45, 'video': 0xF52, 'toy': 0xF53, 'cheat_character': 0xF57}


def rewards(data):
    at, sections = 0, []
    for _ in range(9):
        kind, count = struct.unpack_from('<II', data, at); at += 8
        sections.append((kind, [data[at + REWARD_SIZES[kind] * i: at + REWARD_SIZES[kind] * (i + 1)] for i in range(count)]))
        at += REWARD_SIZES[kind] * count
    pool = at + 4
    def text(offset):
        if offset == 0: return None
        return data[pool + offset - 1: data.index(b'\0', pool + offset - 1)].decode('latin1')
    out = {}
    for kind, records in sections:
        name, items = REWARD_SECTIONS[kind], []
        for i, r in enumerate(records):
            words = struct.unpack('<%dI' % (len(r) // 4), r)
            item = dict(index=i, name=text(words[0]), image=text(words[1]))
            if kind >= 3:
                price, peak = struct.unpack_from('<hb', r, 12)
                item.update(price=price * 10 if price > 0 else None, peak=peak if peak > 0 else None)
                if kind == 8: item.update(help=text(words[2]), character=r[15])
                else: item['code'] = text(words[2])
            elif kind != 2: item['code'] = text(words[2])
            items.append(item)
        out[name] = dict(save=REWARD_SAVE.get(name), items=items)
    return out


def courses(elf):
    rows = []
    for i in range(COURSE_COUNT):
        a = COURSE_TABLE + COURSE_STRIDE * i
        index = elf.words(a, 1)[0]
        if index != i: raise ValueError(f'course table row {i} holds index {index}')
        code = elf.text(a + 0x34, 16)
        f54, station, f5c, f60 = elf.words(a + 0x54, 4)
        rows.append(dict(index=i, name=elf.text(a + 4, 32), short=elf.text(a + 0x24, 16), code=code,
                         world=elf.text(a + 0x44, 16), kind=EVENT_KIND.get(code[1:3], 'station' if station else 'debug'),
                         field54=f54, station=station, field5c=f5c, field60=f60))
    return rows


def locale(disc):
    out = {}
    for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
        data = disc.file(f'DATA/LOCALE/{name}.LOC')
        out[name] = {f'{k:08x}': v for k, v in loc_file.entries(data).items()}
    return out


def png(width, height, rgba):
    def chunk(tag, body): return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)
    rows = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def big_entries(data):
    if data[:4] != b'BIGF': raise ValueError('Expected a BIGF archive')
    count = struct.unpack_from('>I', data, 8)[0]; at = 16; out = {}
    for _ in range(count):
        offset, size = struct.unpack_from('>II', data, at); at += 8
        end = data.index(b'\0', at); out[data[at:end].decode('ascii')] = data[offset:offset + size]; at = end + 1
    return out


_UNSWIZZLE = {}


def unswizzle8(pixels, width, height):
    """PSMT8 GS swizzle (SHPS flag 0x2000 at +0xC): the byte order a PSMCT32 upload leaves in memory."""
    key = (width, height)
    if key not in _UNSWIZZLE:
        order = []
        for y in range(height):
            for x in range(width):
                block = (y & ~0xf) * width + (x & ~0xf) * 2
                swap = (((y + 2) >> 2) & 1) * 4
                row = (((y & ~3) >> 1) + (y & 1)) & 7
                order.append(block + row * width * 2 + ((x + swap) & 7) * 4 + ((y >> 1) & 1) + ((x >> 2) & 2))
        _UNSWIZZLE[key] = order
    return bytes(map(pixels.__getitem__, _UNSWIZZLE[key]))


def shps_image(data, gs_clut=False):
    """First image of a PS2 SHPS container: 8-bit CLUT (PSMT8 CLUT swizzle) or 32-bit; GS alpha 0..128 doubled.
    gs_clut: read the 8-bit CLUT as the GS holds it, 256 entries (a 16x16 PSMCT32 CLUT) from the start of the CLUT
    block, whatever its count (+4 x +6) says. Texels may index entries past the count: the file's block padding holds
    real colours there, and the GS VRAM of a PS2 savestate holds exactly those bytes (docs/xbox-textures.md section 6).
    Default False keeps the old count-limited read (the entries past the count decode as transparent black)."""
    if data[:4] != b'SHPS': raise ValueError('Expected a SHPS container')
    d = data[struct.unpack_from('<I', data, 20)[0]:]
    kind, size = d[0], int.from_bytes(d[1:4], 'little'); width, height = struct.unpack_from('<HH', d, 4)
    out = bytearray()
    if kind == 2:
        pw, ph = struct.unpack_from('<HH', d, size + 4)
        palette = d[size + 16:size + 16 + 4 * (256 if gs_clut else pw * ph)]
        table = []
        for index in range(256):
            e = (index & 0xe7) | ((index & 8) << 1) | ((index & 16) >> 1)
            r, g, b, a = (palette[4 * e:4 * e + 4] + b'\0\0\0\0')[:4]; table.append(bytes((r, g, b, min(255, 2 * a))))
        pixels = d[16:16 + width * height]
        if struct.unpack_from('<H', d, 12)[0] & 0x2000: pixels = unswizzle8(pixels, width, height)
        if gs_clut and len(palette) < 1024:   # the member ends inside the CLUT: an entry past its end is unknown
            limit = len(palette) // 4
            if any(((i & 0xe7) | ((i & 8) << 1) | ((i & 16) >> 1)) >= limit for i in set(pixels)):
                raise ValueError('Texel indexes a CLUT entry past the end of the shape')
        out = bytearray(b''.join(table[i] for i in pixels))
    elif kind == 5:
        for i in range(width * height):
            r, g, b, a = d[16 + 4 * i:20 + 4 * i]; out += bytes((r, g, b, min(255, 2 * a)))
    else:
        raise ValueError(f'Unsupported SHPS image kind {kind}')
    return width, height, bytes(out)


def export_pictures(disc, folder):
    """Course pictures (DATA/UI/COURSPIC.BIG) and mountain/peak/session maps (DATA/UI/MAPGFX.BIG)."""
    names = []
    for archive in ('COURSPIC', 'MAPGFX'):
        for name, data in big_entries(disc.file(f'DATA/UI/{archive}.BIG')).items():
            stem = f'{archive}_{Path(name).stem}'; (folder / f'{stem}.png').write_bytes(png(*shps_image(data))); names.append(stem)
    return names


def export_reward_images(disc, catalog, folder):
    """Reward pictures (DATA/CHAR/RWRDPS2.BIG) referenced by the catalog, for the lodge Rewards viewer."""
    archive = big_entries(disc.file('DATA/CHAR/RWRDPS2.BIG')); lower = {k.lower(): k for k in archive}
    folder.mkdir(exist_ok=True); written = 0
    for section in catalog.values():
        for item in section['items']:
            image = (item.get('image') or '').split('|')[-1].lower()
            if image not in lower: continue
            try: (folder / f"{Path(image).stem}.png").write_bytes(png(*shps_image(archive[lower[image]], gs_clut=True))); item['picture'] = Path(image).stem; written += 1   # 17 pictures index CLUT entries past the count
            except ValueError: pass
    return written


def export(output):
    disc = Disc(ISO)
    try:
        elf_bytes = disc.file('SLUS_207.72'); strings = locale(disc); roster = characters(disc); catalog = rewards(disc.file('DATA/BE/RWRDPS2.DAT'))
        output.parent.mkdir(parents=True, exist_ok=True); pictures = export_pictures(disc, output.parent)
        reward_pictures = export_reward_images(disc, catalog, output.parent / 'REWARDS')
    finally:
        disc.close()
    elf = Elf(elf_bytes)
    career = dict(
        provenance=dict(elf_sha256=hashlib.sha256(elf_bytes).hexdigest(), course_table=hex(COURSE_TABLE),
                        locale='DATA/LOCALE/{FEAMER,OVAMER,CMNAMER}.LOC (hash -> UTF-16 text, keyed by loc_file.name_hash of the kT_ id)'),
        courses=courses(elf), characters=roster, rules=rules(elf), rewards=catalog, pictures=pictures, strings=strings)
    output.write_text(json.dumps(career, ensure_ascii=False, separators=(',', ':')))
    return career


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, default=ROOT / 'web/public/assets/CAREER/career.json')
    a = p.parse_args(); career = export(a.output)
    print(f"career tables: {len(career['courses'])} courses, strings {', '.join(f'{k} {len(v)}' for k, v in career['strings'].items())} -> {a.output}")


if __name__ == '__main__':
    main()
