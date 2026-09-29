#!/usr/bin/env python3
"""Export the per-location world audio of SSX 3 (PS2) for web/audio-world.js and web/audio-painters.js.

Reads local/assets/source/ps2/bam.ssb + bam.sdb (extracted from the ISO) and DATA/CONFIG/WATRIG.ADL from the ISO.
Writes web/public/assets/AUDIO/:
  world/<EVENT>.json   one file per course location (SDB index < 22 plus its connectors, tools/world_assets
                       event_locations): {event, locations: {<LOC>: {track, locationId, banks, ambience, thunder,
                       avalanche, emitters, contacts, painters}}, watrig}
  world/PEAK<n>.json   the same format with every location of a peak (streamed free ride / peak runs)
  banks/<LOC>_slot8.bnk / _slot9.bnk (+ .json in the tools/export_audio.py bank format): the per-location banks

Sources (docs/audio-logic.md section 5.5):
  SSB kind 20  per-location sound banks, rid 0 -> bank slot 8, rid 1 -> slot 9 (loader 286CA8).
  SSB kind 13  instance sound data (2B6B08/2B6B50): per instance ref (rid<<8)|track a block {u32 nContact, u32 nEmit,
               u32 contactId[nContact] (watrig ids, 296088), emitter records}; emitter offsets are added to the
               instance translation (kind-3 matrix row 3) without rotation; units cm, original Z-up.
               type 0 sphere (0x1C): adl, off[3], radius, falloff; type 1 ellipsoid (0x30): adl, off[3], ext[3], D[3],
               falloff; type 2 cone (0x30): adl, off[3], radius, dir[3], distFalloff, cosHalf, angleFalloff; type 3
               zone (0x18): adl, off[3], radius.
  SSB kind 15  world painters (types 0 MusicTrigger, 1 Mix, 2 Ambience, 3 Speech): record +8 section offsets;
               section {header size, count, 0xC, (kind, payload offset) x count}, tree at header size (2C1CD8).
  WATRIG.ADL   trigger table (2B5A18): type 1 {bank slot, sound}, type 3 named bank (slot 3, sound 0), type 4 special.

  python3 tools/export_world_audio.py [--iso PATH] [--source DIR] [--out DIR]
"""
import argparse, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations, event_locations  # noqa: E402
from export_audio import parse_bank, ISO, OUT  # noqa: E402
from inspect_disc import Disc  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
# ELF location table 0x43E250 (location id = index; courses 0-16, hubs 17-21, connectors 22-42, TRANSP 43).
ELF_LOC_IDS = ['ARA1', 'BRA2', 'CRA3', 'DRA4', 'ERA5', 'ASS1', 'DSS2', 'ESS3', 'ABA1', 'CBA2', 'EBA3', 'BHP1', 'CHP2', 'EHP3', 'ABC1',
               'DBC2', 'EBC3', 'A', 'B', 'C', 'D', 'E', 'EBC3_E', 'E_ERA5', 'E_ESS3', 'E_EHP3', 'E_EBA3', 'ERA5_C', 'C_CRA3', 'C_CHP2',
               'C_CBA2', 'CRA3_D', 'DBC2_D', 'D_DRA4', 'D_DSS2', 'DRA4_A', 'ABC1_A', 'A_ARA1', 'A_ASS1', 'A_ABA1', 'ARA1_B', 'B_BRA2',
               'B_BHP1', 'TRANSP']
# 2A1E20 (jump table 0x483010): the peak of each course / hub location id.
PEAK_LOCATIONS = {'PEAK1': [0, 1, 5, 8, 11, 14, 17, 18], 'PEAK2': [2, 3, 6, 9, 12, 15, 19, 20], 'PEAK3': [4, 7, 10, 13, 16, 21]}
EMITTER_SIZE = {0: 0x1C, 1: 0x30, 2: 0x30, 3: 0x18}
PAINTER_TYPES = {0: ('musicTrigger', 2), 1: ('mix', 1), 2: ('ambience', 1), 3: ('speech', 1)}


def watrig_table(adl):
    if adl[0] != 0: raise ValueError('WATRIG.ADL flag byte')
    n = struct.unpack_from('<I', adl, 8)[0]; o = 0xC; out = {}
    for i in range(2, n + 2):  # entries 0, 1 unused (2B5A18)
        t = struct.unpack_from('<I', adl, o)[0]
        if t == 1: b, s = struct.unpack_from('<2I', adl, o + 0x10); out[i] = dict(type=1, bank=b, snd=s); o += 0x18
        elif t == 3: out[i] = dict(type=3, bank=adl[o + 0x10:o + 0x30].split(b'\0')[0].decode(), snd=0); o += 0x30
        elif t == 2:
            snd = list(struct.unpack_from('<4I', adl, o + 0x14)); w = list(struct.unpack_from('<4I', adl, o + 0x24))
            out[i] = dict(type=2, bank=struct.unpack_from('<I', adl, o + 0x10)[0], snds=snd, weights=w, total=struct.unpack_from('<I', adl, o + 0x34)[0]); o += 0x38
        else: out[i] = dict(type=4); o += 0x10
    if o != len(adl): raise ValueError(f'WATRIG.ADL size {o} != {len(adl)}')
    return out


def parse_block(rec, off):
    nc, ne = struct.unpack_from('<2I', rec, off)
    contacts = list(struct.unpack_from('<%dI' % nc, rec, off + 8))
    p = off + 8 + 4 * nc; em = []
    for _ in range(ne):
        t, adl = struct.unpack_from('<2I', rec, p)
        f = struct.unpack_from('<%df' % ((EMITTER_SIZE[t] - 8) // 4), rec, p + 8)
        e = dict(adl=adl, offset=list(f[0:3]))
        if t == 0: e.update(shape='sphere', radius=f[3], falloff=int(f[4]))
        elif t == 1: e.update(shape='ellipsoid', extents=list(f[3:6]), axis=list(f[6:9]), falloff=int(f[9]))
        elif t == 2: e.update(shape='cone', radius=f[3], dir=list(f[4:7]), falloff=int(f[7]), cosHalfAngle=f[8], angleFalloff=int(f[9]))
        else: e.update(shape='zone', radius=f[3])
        em.append(e); p += EMITTER_SIZE[t]
    return contacts, em


def parse_painter_section(data, off, nvals):
    sec = data[off:]
    hs, count, tbl = struct.unpack_from('<3I', sec, 0)
    if tbl != 0xC or hs != 0xC + 8 * count: raise ValueError('painter section header')
    payloads = []
    for i in range(count):
        kind, po = struct.unpack_from('<2I', sec, 12 + 8 * i)
        payloads.append(dict(kind=kind, rate=struct.unpack_from('<f', sec, po)[0], values=list(struct.unpack_from('<%di' % nvals, sec, po + 4))))
    scale, ox, oy, n, _ = struct.unpack_from('<3f2I', sec, hs)
    root = struct.unpack_from('<H', sec, hs + 0x14)[0]
    outside = list(struct.unpack_from('<2I', sec, hs + 0x18))
    nodes = [v for i in range(n) for v in struct.unpack_from('<4H', sec, hs + 0x28 + 8 * i)]
    if root >= n: raise ValueError('painter root')
    return dict(scale=scale, origin=[ox, oy], root=root, outside=outside, nodes=nodes,
                payloads=[dict(rate=p['rate'], values=p['values']) for p in payloads], kinds=[p['kind'] for p in payloads])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso', type=Path, default=ISO)
    ap.add_argument('--source', type=Path, default=SOURCE)
    ap.add_argument('--out', type=Path, default=OUT)
    a = ap.parse_args()
    disc = Disc(a.iso)
    try: watrig = watrig_table(disc.file('DATA/CONFIG/WATRIG.ADL'))
    finally: disc.close()
    locs = locations(a.source / 'bam.sdb')
    loc_of = lambda ci: next(i for i, l in enumerate(locs) if ci <= l['chunk_end'])
    per = {l['name']: dict(track=i, locationId=ELF_LOC_IDS.index(l['name']) if l['name'] in ELF_LOC_IDS else None,
                           banks=dict(slot8=None, slot9=None), emitters=[], contacts={}, painters={}) for i, l in enumerate(locs)}
    inst, blocks = {}, []
    (a.out / 'banks').mkdir(parents=True, exist_ok=True); (a.out / 'world').mkdir(parents=True, exist_ok=True)
    for ci, chunk in enumerate(world_chunks(a.source / 'bam.ssb')):
        li = loc_of(ci); name = locs[li]['name']
        for kind, track, rid, data in records(chunk):
            if kind == 3 and len(data) >= 80: inst[track, rid] = struct.unpack_from('<16f', data, 16)
            elif kind == 13 and data and data[0] == 0:
                n = struct.unpack_from('<I', data, 0xC)[0]
                for k in range(n):
                    ref, off = struct.unpack_from('<Ii', data, 0x10 + 0x18 * k + 0xC)
                    if ref == 0xFFFFFFFF: continue
                    contacts, em = parse_block(data, off)
                    blocks.append((name, ref, contacts, em))
            elif kind == 20 and data:
                slot = 8 + rid; file = f'{name}_slot{slot}.bnk'
                (a.out / 'banks' / file).write_bytes(data)
                bank = parse_bank(data)
                (a.out / 'banks' / f'{name}_slot{slot}.json').write_text(json.dumps(dict(file=file, original=f'bam.ssb kind 20 track {track} rid {rid}', source='SSB', **bank), separators=(',', ':')))
                per[name]['banks'][f'slot{slot}'] = file
                per[name].setdefault('entries', {})[slot] = [bool(e) for e in bank['entries']]
            elif kind == 15 and len(data) >= 0x40:
                if track != li: raise ValueError(f'{name}: painter track {track}')
                offs = struct.unpack_from('<14I', data, 8)
                for t, (key, nv) in PAINTER_TYPES.items():
                    per[name]['painters'][key] = None if offs[t] == 0xFFFFFFFF else parse_painter_section(data, offs[t], nv)
    for name, ref, contacts, em in blocks:
        m = inst.get((ref & 0xFF, ref >> 8))
        if any(contacts): per[name]['contacts'][str(ref)] = contacts
        for e in em:
            if not m: continue
            e['pos'] = [m[12 + i] + e['offset'][i] for i in range(3)]
            per[name]['emitters'].append(dict(resource=ref, **e))
    for name, d in per.items():
        ent = d.pop('entries', {})
        has = lambda slot, i: bool(ent.get(slot) and i < len(ent[slot]) and ent[slot][i])
        lid = d['locationId']
        d['ambience'] = dict(bank=9, snd=0) if lid is not None and lid < 22 and has(9, 0) else None  # 29D370
        d['thunder'] = has(8, 16)    # 291438
        d['avalanche'] = has(8, 2)   # 29DEF0
        d['slotEntries'] = {str(k): len(v) for k, v in ent.items()}
    written = 0
    for i, l in enumerate(locs):
        lid = ELF_LOC_IDS.index(l['name']) if l['name'] in ELF_LOC_IDS else None
        if lid is None or lid >= 22: continue
        members = [n for _, n, _, _ in event_locations(locs, l['name'])]
        doc = dict(event=l['name'], units='source cm, Z-up', watrig={str(k): v for k, v in watrig.items()},
                   locations={n: per[n] for n in members})
        (a.out / 'world' / f"{l['name']}.json").write_text(json.dumps(doc, separators=(',', ':')))
        written += 1
        print(f"{l['name']:5s} " + ', '.join(f"{n}: {len(per[n]['emitters'])} emitters, {len(per[n]['contacts'])} contact instances, banks {per[n]['banks']}" for n in members))
    # Streamed peak worlds (free ride / peak runs, docs/peak-mountain.md): world/PEAK<n>.json holds every location of
    # the peak (the union of its courses' and hubs' event locations); web/audio-world.js gates it by the streaming rows.
    for peak, ids in PEAK_LOCATIONS.items():
        members = []
        for lid in ids:
            for _, n, _, _ in event_locations(locs, ELF_LOC_IDS[lid]):
                if n not in members: members.append(n)
        tracks = [per[n]['track'] for n in members]
        if len(set(tracks)) != len(tracks): raise ValueError(f'{peak}: duplicate tracks')
        doc = dict(event=peak, units='source cm, Z-up', watrig={str(k): v for k, v in watrig.items()},
                   locations={n: per[n] for n in members})
        (a.out / 'world' / f'{peak}.json').write_text(json.dumps(doc, separators=(',', ':')))
        written += 1
        print(f'{peak}: {len(members)} locations ({", ".join(members)})')
    print(f'wrote {written} world audio files to {a.out / "world"}')


if __name__ == '__main__':
    main()
