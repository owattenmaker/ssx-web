#!/usr/bin/env python3
"""Equip Gear / outfits: the original wardrobe of every character as browser assets (docs/characters.md,
"Equip Gear and outfits").

Source (read-only): DATA/CHAR/BOLTPS2.DAT (local/sam-ps2/original copy, else the ISO) for the item database,
the GameCube twins of the models and textures (local/gamecube/disc/files/data/char: mdlngc.big, <X>txn.big; the
same geometry/texture data as the PS2 MPF/SSH, see tools/rider_assets.py), and the locale files for the menu folders.

Output (git-ignored) per character id 0..29 (the ten riders and the twenty cheat skins; Sam keeps his own mesh):

  web/public/assets/WARDROBE/<ID>/wardrobe.json   items, equip rules, default outfit, NIS->race model table, parts
                                                   (bones, batches, offsets into parts.bin), textures, icons
  web/public/assets/WARDROBE/<ID>/parts.bin       per part: vertices f32 x10 (pos3 normal3 uv2 uv1, Y-up metres,
                                                   the package layout), u32 local indices, skin records (u8 count,
                                                   3 pad, then 4 x {u8 file, u8 bone, i16 source weight}), and for
                                                   the NIS head/hands dense f32 xyz morph deltas per vertex
  web/public/assets/WARDROBE/<ID>/textures/<stem>.png (PS2 texel domain), icons/<iNNN>.png, packed at the end into
                                                   textures.tex / icons.tex (tools/export_rider_textures.py)
  web/public/assets/WARDROBE/equip-screen.json     FE.LUI 12equ_char + the Equip Gear 3D preview constants

The rules the browser applies (web/wardrobe.js) are the original code's:
  - inventory flags per item (profile record R+0x290 rows): 0x2 owned, 0x10 equipped, 0x4 committed, 0x20 default.
    Init 0x1513B8 (default table + price 0/-1), equip 0x151C90 + rule table 0x151EF0, UI equip 0x14AFB0 (equip,
    then 0x1521F0 required slots / 0x1520E8 flag-0x1000 rules, else restore), commit 0x14AEA8 (0x4 = 0x10),
    restore 0x14AF10 (0x10 = 0x4).
  - race assembly 0x14D068 (0x22ED5C): equipped = committed; for every (NIS model, race model) row of table 2 whose
    NIS entry is committed, the race model is equipped; item 4 (the PDA) is equipped.
  - model parts 0x11BBE8 / 0x11C138: every equipped entry adds its four LOD models (+0x18..+0x24) at part slot
    (+0x10 low byte); a cheat skin (setup slot +0x12 != 0) takes EVERY entry of its bucket. Race hides slots 5, 6,
    8, 9, 11 (0x11C61C: NIS head, eyes, NIS hands, NIS right hand, PDA). Parts are ordered by slot (geometry map
    +0x1C), bone slots follow the part order (hidden parts keep theirs).
  - textures 0x11BE88 / 0x14B988: an entry's texture (+0x2C, texture group +3 != -1) with '$' wildcards takes the
    characters at the same positions of another EQUIPPED entry's texture of the same group (e.g. top colour
    zoe_Suit_B01_$$$ + bottom colour zoe_Suit_$$$_B01 -> zoe_Suit_B01_B01). Cheat skins use the raw names.
    A model material binds the loaded texture whose SSH entry name (4 chars, e.g. 'suit') is the material name.
  - secondary motion enables (0x11CF70..0x11D1B8): channel 0 = any active part slot 0x0F..0x1D except 0x12,
    channel 1 = 0x1E/0x1F/0x20/0x22, channel 2 = 0x24/0x26/0x27/0x2A.

python3 tools/export_wardrobe.py [--character zoe ...] [--verify]   (--verify: default assemblies vs the packages)
"""
import argparse, hashlib, json, re, shutil, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from compare_character_assets import big_members  # noqa: E402
from rider_assets import decode_high_model, decode_rider_texture  # noqa: E402
from world_assets import refpack  # noqa: E402

GC = ROOT / 'local/gamecube/disc/files/data/char'
WEB = ROOT / 'web/public/assets'
OUT = WEB / 'WARDROBE'
ORIGINAL = ROOT / 'local/sam-ps2/original'
NATIVE_SAM = ROOT / 'local/assets/native'
from disc_paths import ps2_iso;ISO = ps2_iso()
HIDDEN_SLOTS = (5, 6, 8, 9, 11)                   # 0x11C61C..0x11C684: 30EA80(geometry, slot, -1) in rider init
PDA_ITEM = 4                                       # 0x14D1A4: lookup[4] |= 0x10 in 0x14D068
SECONDARY = ([s for s in range(0x0F, 0x1E) if s != 0x12], [0x1E, 0x1F, 0x20, 0x22], [0x24, 0x26, 0x27, 0x2A])  # 0x11CF70..
ITEM_LIMIT, ITEM_LIMIT_BASE = 3392, 1500             # *(gp-0x1818) (0x4A18D8) and the 0x5DC bar origin (0x19BC90/0x19BD48)
BONE_SLOT_FIELD = 'geometry part +4 base slot: bones of the parts in slot order, hidden parts included'


def disc_file(name, local):
    if local.exists(): return local.read_bytes()
    from inspect_disc import Disc
    disc = Disc(ISO)
    try: return disc.file(name)
    finally: disc.close()


# ---- BOLTPS2.DAT -------------------------------------------------------------------------------------
class Bolt:
    """56-byte entries, then three tables (12-byte rules, 8-byte NIS->race models, 8-byte default outfit), string pool."""
    def __init__(self, data):
        self.data = data; _, n = struct.unpack_from('<2I', data, 0); p = 8
        raw = [data[p + i * 56:p + (i + 1) * 56] for i in range(n)]; p += n * 56
        self.tables = []
        for stride in (12, 8, 8):
            m = struct.unpack_from('<I', data, p)[0]; p += 4
            self.tables.append([data[p + k * stride:p + (k + 1) * stride] for k in range(m)]); p += m * stride
        self.pool = p + 4
        self.entries = [self.parse(i, r) for i, r in enumerate(raw)]
        self.rules = [dict(char=t[0], on=struct.unpack('b', t[1:2])[0], item=struct.unpack_from('<h', t, 2)[0], cond_state=struct.unpack('b', t[5:6])[0],
                           cond=struct.unpack_from('<h', t, 6)[0], desired=struct.unpack('b', t[9:10])[0], target=struct.unpack_from('<h', t, 10)[0])
                      for t in self.tables[0]]
        self.race_models = [struct.unpack('<BBhhh', t) for t in self.tables[1]]   # (char, 0, 0, NIS item, race item)
        self.defaults = [struct.unpack('<BBhhh', t) for t in self.tables[2]]      # (char, 0, 0, folder, item)

    def string(self, o):
        if not o: return None
        a = self.pool + o - 1
        return self.data[a:self.data.index(b'\0', a)].decode('latin1')

    def parse(self, i, r):
        item, cls, parent, order, weight, cost = struct.unpack_from('<6h', r, 4)
        offs = struct.unpack_from('<9I', r, 0x14)
        return dict(index=i, char=r[0], b1=r[1], tier=struct.unpack('b', r[2:3])[0], group=struct.unpack('b', r[3:4])[0], item=item, cls=cls,
                    parent=parent, order=order, weight=weight, cost=cost, w10=struct.unpack_from('<I', r, 0x10)[0], name=self.string(offs[0]),
                    lods=[self.string(x) for x in offs[1:5]], path=self.string(offs[5]), texture=self.string(offs[6]), icon=self.string(offs[7]),
                    flags=offs[8])


def locale():
    import loc_file
    out = {}
    for name in ('OVAMER', 'FEAMER', 'CMNAMER'):
        for k, v in loc_file.entries(disc_file(f'DATA/LOCALE/{name}.LOC', ORIGINAL / f'{name}.LOC')).items(): out.setdefault(k, v)
    return out, loc_file.name_hash


def kt_hash(t):
    x = 0
    for c in t.encode('latin1'):
        c = c - 256 if c > 127 else c
        x = ((x << 4) + c) & 0xFFFFFFFF
        g = x & 0xF0000000
        if g: x = (x ^ (g >> 23)) ^ g
    return x


# ---- the original rules (Python reference; web/wardrobe.js is the browser port) ------------------------
class Inventory:
    def __init__(self, bolt, ch, flags=None):
        self.ch = ch; self.ents = [e for e in bolt.entries if e['char'] == ch]; self.by = {}
        for e in self.ents: self.by.setdefault(e['item'], e)
        self.rules = [r for r in bolt.rules if r['char'] == ch]
        self.defaults = [d for d in bolt.defaults if d[0] == ch]; self.race = [r for r in bolt.race_models if r[0] == ch]
        self.f = {e['item']: 0 for e in self.ents}
        if flags is not None: self.f.update(flags)

    def equip(self, item, on):                     # 0x151C90
        e = self.by[item]; was = self.f[item] & 0x10
        self.f[item] = self.f[item] | 0x10 if on else self.f[item] & ~0x10
        if on:
            if was: return 1
            if e['flags'] & 1:
                for s in self.ents:
                    if s['cls'] == e['cls'] and s['item'] != item and ((self.f[s['item']] & 0x10) or (s['flags'] & 8)):
                        if not self.equip(s['item'], 0): return 0
            return self.apply(item, 1)
        if was or (e['flags'] & 8):
            for k in self.ents:
                if k['cls'] == item and not self.equip(k['item'], 0): return 0
        return self.apply(item, 0) if was else 1

    def apply(self, item, on):                     # 0x151EF0
        for r in self.rules:
            if r['item'] != item or (r['on'] != 0) != bool(on) or r['target'] == -1: continue
            if r['cond'] != -1 and bool(self.f.get(r['cond'], 0) & 0x10) != (r['cond_state'] != 0): continue
            tgt, des = r['target'], r['desired'] != 0; cur = bool(self.f.get(tgt, 0) & 0x10)
            if cur == des:
                if cur: continue
                te = self.by.get(tgt)
                if not te or not (te['flags'] & 8): continue
            if tgt != item:
                if not self.equip(tgt, des): return 0
            else: self.f[item] = self.f[item] | 0x10 if des else self.f[item] & ~0x10
        return 1

    def init(self):                                # 0x1513B8
        for d in self.defaults: self.equip(d[4], 1)
        for k in self.f:
            if self.f[k] & 0x10: self.f[k] |= 0x26
        found = 0
        for e in self.ents:
            if e['cost'] == 0: self.f[e['item']] |= 2
            elif e['cost'] == -1: self.f[e['item']] |= 2; self.equip(e['item'], 1); found = 1
        if found:
            for k in self.f: self.f[k] = self.f[k] | 6 if self.f[k] & 0x10 else self.f[k] & ~4
        return self

    def race_equipped(self):                       # 0x14D068
        f = {k: (v | 0x10 if v & 4 else v) for k, v in self.f.items()}
        for _, _, _, nis, race in self.race:
            if f.get(nis, 0) & 4 and race >= 0 and race in f: f[race] |= 0x10
        if PDA_ITEM in f: f[PDA_ITEM] |= 0x10
        return [e for e in self.ents if f[e['item']] & 0x10]


def resolve_texture(entry, equipped):
    """0x14B988: '$' wildcards filled from another equipped entry of the same texture group (+3)."""
    if entry['group'] == -1 or not entry['texture']: return None
    out = list(entry['texture']); start = entry['texture'].find('$')
    if start < 0: return entry['texture']
    for o in equipped:
        if o['group'] != entry['group'] or not o['texture'] or o['item'] == entry['item']: continue
        k = start
        while k < len(out) and out[k] != '.':
            if out[k] == '$' and k < len(o['texture']) and o['texture'][k] != '$': out[k] = o['texture'][k]
            k += 1
        if '$' not in ''.join(out)[start:].split('.')[0]: break
    return ''.join(out)


def assemble(inv, cheat=False):
    """Equipped entries -> (parts [(slot, entry)], textures [resolved names]) like 0x11BBE8/0x11C138/0x11BE88."""
    entries = inv.ents if cheat else inv.race_equipped()
    first = {}                                     # 0x30D8B8: the first model added for a file id (slot) makes the part
    for e in entries:
        if e['path'] and (e['w10'] & 0xFF) != 0xFF: first.setdefault(e['w10'] & 0xFF, (gc_resource(e['path'])[1], e))
    parts = sorted(((slot, res), e) for slot, (res, e) in first.items())
    textures = []
    for e in entries:
        t = (e['texture'] if cheat else resolve_texture(e, entries))
        if t and gc_resource(t)[1] not in textures: textures.append(gc_resource(t)[1])
    return [(slot, res, e) for (slot, res), e in parts], textures


# ---- assets -------------------------------------------------------------------------------------------
def gc_resource(ps2):
    """'data/char/zoetxp.big|zoe_Suit_B01_$$$.ssh' -> ('zoetxn.big', 'zoe_suit_b01_$$$.gsh')."""
    archive, name = ps2.split('|')
    archive = Path(archive).name.lower().replace('txp.big', 'txn.big').replace('mdlps2.big', 'mdlngc.big')
    return archive, name.lower().replace('.ssh', '.gsh').replace('.mpf', '.mnf')


def texture_name(data):
    data = refpack(data) if data[:2] == b'\x10\xfb' else data
    return data, data[16:20].decode('latin1')


def export_character(bolt, ch, rid, names, png, models):
    from export_characters import ps2_texel_rgba
    ents = [e for e in bolt.entries if e['char'] == ch]
    folder = OUT / rid.upper()
    if folder.exists(): shutil.rmtree(folder)
    (folder / 'textures').mkdir(parents=True); (folder / 'icons').mkdir()
    blob = bytearray(); parts = {}; model_members = {n.lower(): (n, d) for n, d in models.items()}
    for e in ents:
        slot = e['w10'] & 0xFF
        if not e['path'] or slot == 0xFF: continue
        archive, res = gc_resource(e['path'])
        if res in parts: continue
        if res not in model_members: raise ValueError(f'{rid}: no GameCube model {res}')
        member, data = model_members[res]; model = decode_high_model(data)
        if model['name'].lower() != (e['lods'][0] or '').lower(): raise ValueError(f'{rid}: {res} first model {model["name"]} != LOD0 {e["lods"][0]}')
        if any(b['file'] != slot for b in model['bones']): raise ValueError(f'{rid}: {res} bone file id differs from part slot {slot}')
        info = dict(resource=member, name=model['name'], slot=slot, bone_count=len(model['bones']), morph_count=model['morph_count'],
                    source_sha256=model['source_sha256'])
        if slot in HIDDEN_SLOTS: info['hidden'] = True        # race-hidden (the NIS head/eyes/hands/PDA): the FE preview draws them
        start = len(blob)
        for v in model['vertices']: blob += struct.pack('<10f', v[0], v[2], -v[1], v[3], v[5], -v[4], *v[6:])
        index_at = len(blob); batches = []; first = 0
        for group in model['material_batches']:
            blob += struct.pack(f'<{len(group["indices"])}I', *group['indices'])
            batches.append(dict(material=group['material'], first=first, count=len(group['indices']))); first += len(group['indices'])
        skin_at = len(blob)
        for influences in model['skin']:
            rec = struct.pack('<B3x', len(influences)) + b''.join(struct.pack('<BBh', x['file'], x['bone'], x['source_weight']) for x in influences)
            blob += rec.ljust(20, b'\0')
        bones = [{k: b[k] for k in ('name', 'file', 'index', 'mirror_index', 'parent_file', 'parent_index', 'dof_flags', 'translation', 'rotation',
                                     'source_translation_cm', 'source_rotation', 'source_translation_bits', 'source_rotation_bits',
                                     'mirror_quaternion_map', 'mirror_translation_scale')} for b in model['bones']]
        parts[res] = dict(info, vertex_offset=start, vertex_count=len(model['vertices']), index_offset=index_at, index_count=first,
                          skin_offset=skin_at, batches=batches, bones=bones, morph_count=model['morph_count'])
        # morph targets (NIS head / hands): dense float32 xyz deltas per output vertex, Y-up metres (tools/export_fe_preview.py)
        morphs = []
        for m in model['morphs']:
            dense = [0.0] * (3 * len(model['vertices'])); by_position = dict(zip(m['positions'], m['deltas']))
            for k, position in enumerate(model['vertex_positions']):
                d = by_position.get(position)
                if d: dense[3 * k:3 * k + 3] = [d[0], d[2], -d[1]]
            morphs.append(dict(channel=m['channel'], offset=len(blob), vertices=len(m['positions'])))
            blob += struct.pack(f'<{len(dense)}f', *dense)
        parts[res]['morphs'] = morphs
    # textures reachable from the entries: '$' is a one-character wildcard over the character's texture archive
    archives = {}; textures = {}
    def members(archive):
        if archive not in archives: archives[archive] = {n.lower(): (n, d) for n, d in big_members((GC / archive).read_bytes())}
        return archives[archive]
    patterns = sorted({gc_resource(e['texture']) for e in ents if e['texture']})
    for archive, pattern in patterns:
        rx = re.compile('^' + re.escape(pattern).replace(r'\$', '.') + '$')
        hits = [n for n in members(archive) if rx.match(n)]
        for n in hits:
            stem = Path(n).stem
            if stem in textures: continue
            member, data = members(archive)[n]; data, kind = texture_name(data)
            if struct.unpack_from('>I', data, 8)[0] != 1: raise ValueError(f'{n}: expected one texture')
            w, h, rgba = decode_rider_texture(data[struct.unpack_from('>I', data, 20)[0]:])
            rgba, exact = ps2_texel_rgba(member, w, h, rgba)      # the PS2 TXP texels (128 = 1.0), as the RIDER_* packages
            (folder / f'textures/{stem}.png').write_bytes(png(w, h, rgba))
            textures[stem] = dict(name=kind, width=w, height=h, archive=archive, resource=member, texel_domain='ps2', ps2_rgb=exact)
    # gear icons (0x14B700: the character's texture archive, <prefix>_icon_NNN; none for ids >= 10)
    icons = {}
    base_archive = next((gc_resource(e['texture'])[0] for e in ents if e['texture'] and 'othertxp' not in e['texture']), None)
    prefix = next((gc_resource(e['path'])[1].split('_')[0] for e in ents if e['path'] and not gc_resource(e['path'])[1].startswith('board_')), rid)
    if ch < 10 and base_archive:
        for icon in sorted({e['icon'] for e in ents if e['icon']}):
            n = f'{prefix}_icon_{icon[1:]}.gsh'
            if n not in members(base_archive): continue
            data, kind = texture_name(members(base_archive)[n][1])
            w, h, rgba = decode_rider_texture(data[struct.unpack_from('>I', data, 20)[0]:])
            (folder / f'icons/{icon}.png').write_bytes(png(w, h, rgba)); icons[icon] = dict(width=w, height=h, resource=members(base_archive)[n][0])
    (folder / 'parts.bin').write_bytes(bytes(blob))
    text = lambda n: names(n) if n and n.startswith('kT_') else n
    doc = dict(version=1, character=ch, rider=rid, prefix=prefix, cheat=ch >= 10,
               source=dict(bolt_sha256=hashlib.sha256(bolt.data).hexdigest(), tool='tools/export_wardrobe.py'),
               # [item, class +6, menu parent +8, order +0xA, flags +0x34, price field +0xE, tier +2, name, texture group +3,
               #  part slot (+0x10 low byte, 255 none), model resource key or null, texture pattern (GameCube stem) or null, icon,
               #  item-limit weight +0xC (0x14B478 sums the equipped ones; Equip Gear limit *(gp-0x1818) = 3392, bars from 1500)]
               entries=[[e['item'], e['cls'], e['parent'], e['order'], e['flags'], e['cost'], e['tier'], text(e['name']), e['group'], e['w10'] & 0xFF,
                         gc_resource(e['path'])[1] if e['path'] and (e['w10'] & 0xFF) != 0xFF else None,
                         Path(gc_resource(e['texture'])[1]).stem if e['texture'] else None, e['icon'], e['weight']] for e in ents],
               rules=[[r['on'], r['item'], r['cond'], r['cond_state'], r['target'], r['desired']] for r in bolt.rules if r['char'] == ch],
               defaults=[[d[3], d[4]] for d in bolt.defaults if d[0] == ch],
               race_models=[[r[3], r[4]] for r in bolt.race_models if r[0] == ch],
               item_limit=dict(limit=ITEM_LIMIT, base=ITEM_LIMIT_BASE), hidden_slots=list(HIDDEN_SLOTS), pda_item=PDA_ITEM, secondary_slots=[list(s) for s in SECONDARY],
               parts=parts, textures=textures, icons=icons, parts_bytes=len(blob),
               # the FE preview block of the rider's default FE package (tools/export_fe_preview.py: IRR record, camera,
               # root, rim, clips); an outfit's FE package reuses it (same character, same preview slot)
               fe=fe_block(rid))
    (folder / 'wardrobe.json').write_text(json.dumps(doc, separators=(',', ':')))
    return doc


def fe_block(rid):
    path = WEB / f'RIDER_{rid.upper()}/fe/rider.json'
    return json.loads(path.read_text()).get('fe') if path.exists() else None


def verify(bolt, ch, rid):
    """Default outfit assembly (fresh profile) against the live-verified package RIDER_<ID>."""
    rig = json.loads((WEB / f'RIDER_{rid.upper()}/rider.json').read_text()); world = json.loads((WEB / f'RIDER_{rid.upper()}/world.json').read_text())
    inv = Inventory(bolt, ch).init(); parts, textures = assemble(inv, cheat=ch >= 10)
    visible = [res for slot, res, e in parts if slot not in HIDDEN_SLOTS]
    doc = json.loads((OUT / rid.upper() / 'wardrobe.json').read_text())
    package = sorted((p.get('resource') or f"{'board' if p['part'].startswith(('Bindings', 'BoardFlex')) else doc['prefix']}_{p['part']}.mnf").lower() for p in rig['parts'])
    kinds = {}
    for t in textures:
        stem = Path(t).stem
        if stem in doc['textures']: kinds.setdefault(doc['textures'][stem]['name'], stem)
    used = {doc['parts'][r]['resource'].lower(): r for r in visible}
    want = sorted({kinds.get(b['material']) for r in visible for b in doc['parts'][r]['batches']} - {None})
    have = sorted({Path(world['textures'][f"9-{b['texture']}"]['resource']).stem.lower() for b in world['batches']})
    return dict(rider=rid, parts_ok=sorted(used) == package, parts=[doc['parts'][r]['name'] for r in visible], package=package if sorted(used) != package else None,
                textures_ok=want == have, textures=want, package_textures=have if want != have else None)


def equip_preview():
    """The Equip Gear 3D preview (cFEStateCharEquip update 0x19BFE8, FE preview slot *(*(gp-0x848)+0x7C)+0xB0), cm Z-up.
    State +0xAE0: 1 rider view, 2 board view, 3 easing to the board view (entering the Boards folder, id *(gp-0x1F10)
    = 3, 0x199D14), 4 easing back (0x199E30); each eased frame p += (target - p) * 0.2 (gp-0x5990 / gp-0x5984) until
    the squared board (3) / rider (4) distance < 0.2. Rider root (-105, -100 + 320 z, -80 + 65 z * +0xAA8 (-1)) with
    z = +0xAA4 in [0,1] (right stick Y: += -0.1 x stick, 0x199B64), turned qZ((+0xAAC + 80) deg) (right stick X: +3 deg
    per frame, 0x199AAC); board view rider (-250, -650, -215). Board (0x19F548 +0xC50/+0xC60): rider view
    (265.75, -1073.5, -232), board view (-100, 45, 15); q = rotZ(2 x 2.138029) rotY(2 x 2.792527) rotX(+0xAB0 + 90 deg),
    +0xAB0 += 1 deg per frame (mod 360). Camera 0x15E050: eye (-110, 394, 0) -> (-35, 0, 0), 25 deg (gp-0x5974).
    The view matrix bits come from a derived PS2 Equip Gear state (lighting rim, 0x389CB8)."""
    from export_career import Elf
    elf = Elf((ROOT / 'local/disc/SLUS_207.72').read_bytes()); gp = 0x4A30F0
    f = lambda off: struct.unpack('<f', elf.read(gp - off, 4))[0]
    doc = dict(board_folder=struct.unpack('<i', elf.read(gp - 0x1F10, 4))[0], ease=f(0x5990), done=f(0x5984), zoom_rate=f(0x59A8),
               rider=[-105.0, -100.0, -80.0], zoom=[320.0, 65.0, -1.0], rider_yaw=80.0, turn=3.0,
               rider_boards=[-250.0, f(0x5994), -215.0], board=[f(0x59A0), f(0x599C), -232.0], board_boards=[-100.0, 45.0, 15.0],
               board_half_angles=[f(0x597C), f(0x5978)], board_spin=1.0, board_spin_offset=90.0,
               deg=f(0x5980), fov_half_horizontal=f(0x5974), eye=[-110.0, 394.0, 0.0], target=[-35.0, 0.0, 0.0])
    state = ROOT / 'local/reference/pcsx2/characters/zoe/gear/dangerous-trouble-zennish-screen-equip-categories.p2s'
    if state.exists():
        import zipfile
        with zipfile.ZipFile(state) as z: m = z.read('eeMemory.bin')
        u = lambda a: struct.unpack_from('<I', m, a)[0]; manager = u(u(gp - 0x848) + 0x7C); slot = manager + 0xB0
        doc['view_matrix_bits'] = list(struct.unpack_from('<16I', m, manager + 0x50))
        live = dict(rider=list(struct.unpack_from('<3f', m, slot + 0xC30)), board=list(struct.unpack_from('<3f', m, slot + 0xC50)))
        if [round(x, 3) for x in live['rider']] != doc['rider'] or [round(x, 3) for x in live['board']] != [round(x, 3) for x in doc['board']]:
            raise ValueError(f'Equip Gear preview differs from the state: {live}')
        doc['evidence'] = str(state.relative_to(ROOT))
    return doc


def export_screen():
    """FE.LUI 12equ_char (cFEStateCharEquip, Equip Gear / Buy Gear) for web/lui-player.js (the decode of
    tools/export_character_select.py) -> WARDROBE/equip-screen.json."""
    import export_character_select as ecs
    from export_loading_screen import lui_screens, lui_animations, lui_objects, shps_pages
    import loc_file
    lui = disc_file('DATA/UI/FE.LUI', ORIGINAL / 'FE.LUI'); ssh = disc_file('DATA/UI/FE_1.SSH', ORIGINAL / 'FE_1.SSH')
    strings = {}
    for name in ('FEAMER', 'OVAMER', 'CMNAMER'):
        for k, v in loc_file.entries(disc_file(f'DATA/LOCALE/{name}.LOC', ORIGINAL / f'{name}.LOC')).items(): strings.setdefault(k, v)
    pages = shps_pages(ssh); sprites = {}
    for o in lui_objects(lui):
        size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
        sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'FE_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
    screens, animations = lui_screens(lui), lui_animations(lui); h = loc_file.name_hash('12equ_char')
    elements, events, labels = ecs.decode(screens[h], sprites, strings)
    anims = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
    used = sorted({e['sprite']['page'] for e in elements if e.get('sprite')})
    for page in used:
        if not (WEB / 'UI' / f'{page}.png').exists(): raise ValueError(f'UI/{page}.png missing: run web/prepare-ui.py')
    doc = dict(provenance=dict(fe_lui_sha256=hashlib.sha256(lui).hexdigest(), screen='12equ_char', hash=f'0x{h:08X}', tool='tools/export_wardrobe.py'),
               pages=used, preview=equip_preview(), screen=dict(hash=f'{h:08x}', elements=elements, events=events, labels=labels, animations={k: animations[k] for k in sorted(anims) if k in animations}))
    OUT.mkdir(parents=True, exist_ok=True); (OUT / 'equip-screen.json').write_text(json.dumps(doc, ensure_ascii=False, separators=(',', ':')))
    print('12equ_char:', len(elements), 'elements, pages', used)


SAM_BOLT = ROOT / 'local/sam-ps2/roster/sam-assets/BOLTPS2-preserved.DAT'   # the Sam PS2 build's wardrobe (bucket 30)
SAM_CONFIG = ROOT / 'config/characters/sam.json'
# Which Sam-build items select a Sam outfit package when config/characters/sam.json does not say (outfit
# "equip_items"): the defaults of both (Mac's default top 49 "Midwest Unc" is the Sam build's, rope_tow_regular
# is the config's default), and the one shared name (59 "Lodge Legend"). The Sam build has no item for the other
# config outfits (sunday_unc, creekside_unc): see docs/characters.md "Sam's Equip Gear".
SAM_ITEMS = {'rope_tow_regular': [49], 'lodge_legend': [59]}


def export_sam(png):
    """Sam's Equip Gear: the Sam PS2 build's bucket 30 (tools/sam_ps2/SamWardrobe.cs: Mac's hierarchy, rules and
    defaults with the sam_character/gear-names.json names, unconverted accessories disabled; icons su01/su02 from
    sam_icons.ssh, 0x14B700 hook) for the lists and rules, and Sam's own whole-outfit browser packages (config
    outfits, native_package; built by the Sam agent's tools/build_sam_web.py) for the model. No parts: the browser
    Sam is one baked mesh per outfit."""
    if not SAM_BOLT.exists(): print('sam: no Sam build wardrobe', SAM_BOLT); return None
    bolt = Bolt(SAM_BOLT.read_bytes()); ents = [e for e in bolt.entries if e['char'] == 30]
    if not ents: raise ValueError('Sam build wardrobe has no bucket 30')
    config = json.loads(SAM_CONFIG.read_text())
    folder = OUT / 'SAM'
    if folder.exists(): shutil.rmtree(folder)
    (folder / 'icons').mkdir(parents=True)
    icons = {}
    for icon in sorted({e['icon'] for e in ents if e['icon'] and e['icon'].startswith('su')}):   # sam_icons.ssh (SamTextures.cs)
        src = ROOT / f'local/sam-ps2/roster/sam-assets/sam-top-icon-{icon}.png'
        if src.exists(): shutil.copy2(src, folder / f'icons/{icon}.png'); icons[icon] = dict(resource=f'sam_icons.ssh|{icon}')
    strings, _ = locale(); text = lambda n: (strings.get(kt_hash(n)) or n) if n and n.startswith('kT_') else n
    outfits = []
    for o in config.get('outfits', []):
        pkg = o.get('native_package')
        web = WEB / pkg if pkg else None
        outfits.append(dict(id=o['id'], label=o['label'], default=bool(o.get('default')), package=pkg,
                            items=o.get('equip_items') or SAM_ITEMS.get(o['id'], []),
                            available=bool(web and (web / 'world.json').exists() and (web / 'rider.json').exists()),
                            fe=bool(web and (web / 'fe/rider.json').exists()), native=bool(pkg and (NATIVE_SAM / pkg / 'rider.json').exists())))
    # every native Sam package (outfit + accessory variants, character.json) and whether the browser has it
    packages = []
    for native in sorted(NATIVE_SAM.glob('RIDER_SAM*')):
        info = json.loads((native / 'character.json').read_text()) if (native / 'character.json').exists() else {}
        packages.append(dict(package=native.name, outfit=info.get('outfit_id'), accessories=info.get('accessory_ids', []),
                             web=(WEB / native.name / 'world.json').exists(), fe=(WEB / native.name / 'fe/rider.json').exists()))
    default = next((o['package'] for o in outfits if o['default']), 'RIDER_SAM')
    doc = dict(version=1, character=30, rider='sam', prefix='sam', cheat=False, custom=True,
               source=dict(bolt=str(SAM_BOLT.relative_to(ROOT)), bolt_sha256=hashlib.sha256(bolt.data).hexdigest(), config=str(SAM_CONFIG.relative_to(ROOT)), tool='tools/export_wardrobe.py'),
               entries=[[e['item'], e['cls'], e['parent'], e['order'], e['flags'], e['cost'], e['tier'], text(e['name']), e['group'], e['w10'] & 0xFF,
                         None, None, e['icon'], e['weight']] for e in ents],
               rules=[[r['on'], r['item'], r['cond'], r['cond_state'], r['target'], r['desired']] for r in bolt.rules if r['char'] == 30],
               defaults=[[d[3], d[4]] for d in bolt.defaults if d[0] == 30], race_models=[[r[3], r[4]] for r in bolt.race_models if r[0] == 30],
               item_limit=dict(limit=ITEM_LIMIT, base=ITEM_LIMIT_BASE), hidden_slots=list(HIDDEN_SLOTS), pda_item=PDA_ITEM,
               secondary_slots=[list(s) for s in SECONDARY], parts={}, textures={}, icons=icons, parts_bytes=0, fe=None,
               sam=dict(default_package=default, settings_package='RIDER_SAM', outfits=outfits, packages=packages))
    (folder / 'wardrobe.json').write_text(json.dumps(doc, separators=(',', ':')))
    (folder / 'parts.bin').write_bytes(b'')
    print('sam: entries', len(ents), 'outfits', [(o['id'], o['package'], o['items'], o['available']) for o in outfits])
    return doc


def ground_truth():
    """The PS2 Equip Gear states (local/reference/pcsx2/characters/<id>/gear/*-{countdown,glide}.json + .p2s, derived by
    ps2_navigate) as one fixture for web/test-wardrobe.mjs: each state's own inventory rows, its active LOD0 parts
    (header-matched), bone slot count, channel-1 masks, secondary motion enables, resident textures and the live
    bind bank (geometry +0x38)."""
    import glob, zipfile
    states = ROOT / 'local/reference/pcsx2/characters'; out = []
    for f in sorted(glob.glob(str(states / '*/gear/*-countdown.json'))) + sorted(glob.glob(str(states / '*/gear/*-glide.json'))):
        d = json.loads(Path(f).read_text()); path = Path(d.get('state') or f.replace('.json', '.p2s'))
        if not path.exists(): continue
        with zipfile.ZipFile(path) as z: m = z.read('eeMemory.bin')
        u = lambda a: struct.unpack_from('<I', m, a)[0]
        R = int(d['inventory']['record'], 16); n = struct.unpack_from('<i', m, R + 0x28C)[0]
        rows = {str(struct.unpack_from('<h', m, R + 0x290 + 4 * k)[0]): struct.unpack_from('<H', m, R + 0x292 + 4 * k)[0] for k in range(n)}
        slot = next(s for s in d['setup_slots_0x534FE0'] if s['slot'] == d['race_slot_0x86C'])
        g = int(d['geometry'], 16); bank = u(g + 0x38)
        out.append(dict(state=str(Path(f).relative_to(ROOT)), character=d['inventory']['character'], cheat=slot['cheat'], flags=rows,
                        parts=[dict(slot=p['part_id'], resources=[r.lower().replace('.mpf', '.mnf') for r in p['resource']]) for p in d['active_lod0_parts']],
                        slot_count=d['bone_slot_count'], masks=d['masks'], secondary=d['secondary_motion']['enabled'], resident=d['resident_textures'],
                        bank=[list(struct.unpack_from('<16I', m, bank + 64 * s)) for s in range(u(g + 16))]))
    target = states / 'gear-ground-truth.json'; target.write_text(json.dumps(out, separators=(',', ':')))
    print('ground truth:', len(out), 'states ->', target.relative_to(ROOT))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--character', action='append')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--ground-truth', action='store_true', help='only write local/reference/pcsx2/characters/gear-ground-truth.json')
    args = parser.parse_args()
    if args.ground_truth: return ground_truth()
    from export_opponent_packages import png
    bolt = Bolt(disc_file('DATA/CHAR/BOLTPS2.DAT', ORIGINAL / 'BOLTPS2.DAT'))
    strings, _ = locale(); names = lambda n: strings.get(kt_hash(n)) or n
    roster = json.loads((WEB / 'riders.json').read_text())
    ids = {r['character']: r['id'] for r in roster if r['kind'] in ('rider', 'cheat')}
    models = dict(big_members((GC / 'mdlngc.big').read_bytes()))
    if not args.character and not args.verify: export_screen()
    report = []
    for ch in range(30):
        rid = ids[ch]
        if args.character and rid not in args.character: continue
        if not args.verify or not (OUT / rid.upper() / 'wardrobe.json').exists():
            doc = export_character(bolt, ch, rid, names, png, models)
            print(rid, 'entries', len(doc['entries']), 'parts', len(doc['parts']), 'textures', len(doc['textures']), 'icons', len(doc['icons']), 'bytes', doc['parts_bytes'])
        report.append(verify(bolt, ch, rid))
        print(json.dumps(report[-1]))
    if not args.character or 'sam' in args.character: export_sam(png)
    if not args.character: ground_truth()
    bad = [r['rider'] for r in report if not (r['parts_ok'] and r['textures_ok'])]
    print('default assemblies match the packages:', len(report) - len(bad), '/', len(report), 'mismatch:', bad)
    from export_rider_textures import pack_all; pack_all()   # textures/*.png, icons/*.png -> textures.tex, icons.tex


if __name__ == '__main__':
    main()
