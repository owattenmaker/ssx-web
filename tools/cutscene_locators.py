#!/usr/bin/env python3
"""Decode the NIS anchor locators (world record kind 18) and AIP start grids for every BAM location.

kind-18 record (one per SDB location, 72 bytes) = u32 handle[18]; handle 0xFFFFFFFF = none;
handle = track | rid << 8  -> the kind-3 world instance (track = SDB location index, rid = resource id).
The in-memory world object is the record payload: 4x4 matrix at +0x10 (row vectors, row 3 = position at +0x40).
Lookup 0x27BB08: pos = row 3, yaw = atan2(m01, m00), pitch = asin(m02) (0x31C128 polynomial asin), roll = 0.
Used by tools/export_cutscenes.py (CUTSCENES/locators.json); docs/cutscenes.md "Anchors".
"""
import json, math, struct, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'tools'))
from world_assets import world_chunks, records, locations, world_resource_names  # noqa: E402
from race_event_assets import decode_aip  # noqa: E402

SRC = REPO / 'local/assets/source/ps2'
# ScriptEngine +0x558[] slot order = streaming table 0x442168 (+4 = SDB index); slots 0..21 are the course ids
# returned by 0x144BC0 (table 0x482660 is identity for 0..21), slot 43 = TRANSP (the "set 43" of looping scripts).
STREAM_ORDER = ['ARA1', 'BRA2', 'CRA3', 'DRA4', 'ERA5', 'ASS1', 'DSS2', 'ESS3', 'ABA1', 'CBA2', 'EBA3', 'BHP1', 'CHP2',
                'EHP3', 'ABC1', 'DBC2', 'EBC3', 'A', 'B', 'C', 'D', 'E']
# anchor id -> (locator type, extra) from jump table 0x481D00 / 0x27A0D8
ANCHORS = {19: (7, 'snap'), 20: (0, 'podium (-20,0,340)'), 21: (0, 'podium (-20,-275,280)'), 22: (0, 'podium (-20,275,280)'),
           23: (0, 'snap'), 24: (8, ''), 25: (1, 'z-1000 then snap'), 26: (2, 'snap'), 27: (3, 'snap'), 28: (4, 'snap'),
           29: (5, 'TRANSP set if loop flag'), 30: (6, 'TRANSP set if loop flag'), 31: (9, 'snap'), 32: (10, ''), 33: (11, ''),
           34: (12, 'snap'), 35: (13, 'snap'), 36: (14, 'snap'), 37: (15, ''), 38: (16, ''), 39: (17, '')}
PODIUM = {20: (-20.0, 0.0, 340.0), 21: (-20.0, -275.0, 280.0), 22: (-20.0, 275.0, 280.0)}  # table 0x4D3770


def f(v):
    return float(f'{v:.9g}')


def decode():
    locs = locations(SRC / 'bam.sdb')
    names = world_resource_names((SRC / 'bam.phm').read_bytes(), (SRC / 'bam.psm').read_bytes())
    inst_names = names[1]
    tables, instances, aips = {}, {}, {}
    for chunk in world_chunks(SRC / 'bam.ssb'):
        for kind, t, rid, d in records(chunk):
            if kind == 18:
                tables[t] = struct.unpack('<18I', d[:72])
            elif kind == 3:
                instances[(t, rid)] = d
            elif kind == 14 and rid == 0:
                aips[t] = d
    out = dict(
        note=__doc__.strip(), units='cm, Z up, radians', stream_slot_order=STREAM_ORDER,
        anchor_ids={str(a): dict(locator_type=v[0], rule=v[1]) for a, v in ANCHORS.items()},
        locations={})
    for li, loc in enumerate(locs):
        if li not in tables:
            continue
        entry = dict(sdb_index=li, stream_slot=STREAM_ORDER.index(loc['name']) if loc['name'] in STREAM_ORDER else
                     (43 if loc['name'] == 'TRANSP' else None), locators={})
        for ty, h in enumerate(tables[li]):
            if h == 0xFFFFFFFF:
                continue
            track, rid = h & 0xFF, h >> 8
            d = instances.get((track, rid))
            rec = dict(handle=f'{h:#010x}', track=track, track_location=locs[track]['name'], rid=rid,
                       instance_name=inst_names.get((track, rid)))
            if d is None:
                rec['error'] = 'instance record not found'
            else:
                m = struct.unpack_from('<16f', d, 16)
                rec.update(pos=[f(m[12]), f(m[13]), f(m[14])], yaw=f(math.atan2(m[1], m[0])),
                           pitch=f(math.asin(max(-1.0, min(1.0, m[2])))), matrix=[f(x) for x in m],
                           scale=f(struct.unpack_from('<f', d, 132)[0]))
                a = [a for a, v in ANCHORS.items() if v[0] == ty]
                rec['anchor_ids'] = a
                if ty == 1:
                    rec['anchor25_before_snap'] = [rec['pos'][0], rec['pos'][1], f(m[14] - 1000.0)]
                if ty == 0:
                    rec['podium_steps'] = {}
                    for pid, (ox, oy, oz) in PODIUM.items():
                        # offset (w=0) rotated by the locator matrix (row vector) and added to row 3
                        rec['podium_steps'][str(pid)] = [f(m[12] + ox * m[0] + oy * m[4] + oz * m[8]),
                                                         f(m[13] + ox * m[1] + oy * m[5] + oz * m[9]),
                                                         f(m[14] + ox * m[2] + oy * m[6] + oz * m[10])]
            entry['locators'][str(ty)] = rec
        if li in aips:
            try:
                aip = decode_aip(aips[li])
            except ValueError:
                aip = dict(regions=[])
            grid = []
            for r in aip['regions']:
                if r[1] == 0:  # AIP region row {index, kind, pos, dir, reset path, race path}; kind 0 = start grid
                    dx, dy, dz = r[5:8]
                    grid.append(dict(slot=r[0], pos=[f(x) for x in r[2:5]], dir=[f(x) for x in r[5:8]],
                                     yaw=f(math.atan2(dy, dx)), pitch=f(math.atan2(dz, math.hypot(dx, dy))),
                                     reset_path=r[8], race_path=r[9]))
            if grid:
                entry['start_grid'] = sorted(grid, key=lambda g: g['slot'])
        out['locations'][loc['name']] = entry
    # live checks (Snow Jam RAM dumps)
    snow = out['locations']['ARA1']
    out['verification'] = dict(
        ARA1_locator1=dict(decoded=snow['locators']['1']['pos'] + [snow['locators']['1']['yaw']],
                           live=[-131779.23, 13946.86, -228782.7, 0.15010],
                           anchor25_live_after_snap=[-131779.234, 13946.858, -228770.812]),
        ARA1_start_grid_live={'0': [-131878.56, 13856.27, -228770.83], '5': [-131950.92, 14290.22, -228770.83],
                              'heading_deg': -170.49})
    return out


def main():
    out = decode()
    for n, e in out['locations'].items():
        print(n, {k: (v.get('instance_name'), v.get('pos')) for k, v in e['locators'].items()}, 'grid', len(e.get('start_grid', [])))


if __name__ == '__main__':
    main()
