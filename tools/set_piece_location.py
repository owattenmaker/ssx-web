#!/usr/bin/env python3
"""Per-location inputs of the set-piece exporters (export_flags.py, export_uv_scroll.py,
export_livecomp.py, export_set_pieces.py, export_spline_setpieces.py, export_rail_teeters.py).

Snow Jam keeps every historical input and output path (outputs stay byte-identical);
other locations resolve the course stage from the SDB location table like
tools/import_stage_scripts.py: the kind-16 stage record of the location's own track in
the location's last chunk (ARA1 track 8 / chunk 33, BRA2 track 16 / chunk 56, BHP1
track 15 / chunk 49). Stage header: +0x10 global handler count/offset (entry 2 = race
GO program), +0x18 handler row count/offset (six words per row, row index =
collision descriptor resource08 >> 8, resource08 & 255 = the location track).
Inputs per location: local/assets/native/<LOC>/world_collision.json,
local/browser-pickups/<LOC>/{disassembly,scripts}.json (ARA1: ara1-*.json),
local/event-activation/<LOC>/countdown-instances.json (ARA1: flat folder).

SPDX-License-Identifier: GPL-3.0-only
"""
import json, struct, sys
from functools import cached_property
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations  # noqa: E402
from locations import location as location_info, pickup_file, activation_dir  # noqa: E402

GP = 0x4A30F0
RUNS = ROOT / 'local/ps2-capture/runs'
# Kept PS2 snapshots of a race-event run per location (--keep-states), used for verification.
SNAPSHOTS = {
    'ARA1': str(RUNS / 'setpieces/race.tick*.p2s'),
    'BRA2': str(RUNS / 'setpieces-bra2/full.tick*.p2s'),   # full course, finish at tick 13316
    'BHP1': str(RUNS / 'setpieces-bhp1/full.tick*.p2s'),   # first run only (ticks 418..4420)
    'ASS1': str(RUNS / 'setpieces-ass1/full.tick*.p2s'),   # R&B tuck run from r-and-b-countdown-anchor (--isolate, section watches)
    'ABA1': str(RUNS / 'setpieces-aba1/full.tick*.p2s'),   # Crow's Nest tuck run from crows-nest-countdown-anchor
    'ABC1': str(RUNS / 'setpieces-abc1/full.tick*.p2s'),   # Happiness Rival Time tuck race from happiness-ready (docs/backcountry.md)
    # Peak 2 (docs/peak2.md): tuck/weave runs from each countdown anchor (--isolate, section/chunk/viewer watches, kept
    # states every 400 ticks); Ruthless (DBC2) from the Rival Time rolling start
    'CRA3': str(RUNS / 'peak2/cra3-full.tick*.p2s'),
    'DRA4': str(RUNS / 'peak2/dra4-full.tick*.p2s'),
    'DSS2': str(RUNS / 'peak2/dss2-full.tick*.p2s'),
    'CBA2': str(RUNS / 'peak2/cba2-full.tick*.p2s'),
    'CHP2': str(RUNS / 'peak2/chp2-full.tick*.p2s'),
    'DBC2': str(RUNS / 'peak2/dbc2-full.tick*.p2s'),
    'ERA5': str(RUNS / 'peak3/gravitude-full.tick*.p2s'),   # Peak 3 (docs/peak3.md): tuck run from the countdown anchor / ready state
    'ESS3': str(RUNS / 'peak3/kick-doubt-full.tick*.p2s'),   # Peak 3 (docs/peak3.md): tuck run from the countdown anchor / ready state
    'EBA3': str(RUNS / 'peak3/much-2-much-full.tick*.p2s'),   # Peak 3 (docs/peak3.md): tuck run from the countdown anchor / ready state
    'EHP3': str(RUNS / 'peak3/perpendiculous-full.tick*.p2s'),   # Peak 3 (docs/peak3.md): tuck run from the countdown anchor / ready state
    'EBC3': str(RUNS / 'peak3/the-throne-full.tick*.p2s'),   # Peak 3 (docs/peak3.md): tuck run from the countdown anchor / ready state
}


def web_asset_dir(code, sub):
    """ARA1 keeps web/public/assets/<SUB>; other locations web/public/assets/<LOC>/<SUB>."""
    location_info(code)
    return ROOT / 'web/public/assets' / sub if code == 'ARA1' else ROOT / 'web/public/assets' / code / sub


class Location:
    def __init__(self, code):
        self.code = code; self.info = location_info(code)
        locs = locations(ROOT / 'local/assets/source/ps2/bam.sdb')
        self.track = next(i for i, l in enumerate(locs) if l['name'] == code)
        self.chunk = locs[self.track]['chunk_end']

    @cached_property
    def _records(self):
        """(stage, models {rid: data}, instances {rid: data}) of the location's own track."""
        stage, models, insts = None, {}, {}
        for ci, chunk in enumerate(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb')):
            for kind, track, rid, data in records(chunk):
                if track != self.track: continue
                if ci == self.chunk and kind == 16: stage = data
                if kind == 2: models.setdefault(rid, data)
                if kind == 3: insts.setdefault(rid, data)
        if stage is None: raise ValueError(f'{self.code}: no stage record')
        return stage, models, insts

    @property
    def stage(self): return self._records[0]

    @property
    def models(self): return self._records[1]

    @property
    def instance_records(self): return self._records[2]

    @property
    def rows(self):
        count, base = struct.unpack_from('<2I', self.stage, 0x18)
        return count, base

    @property
    def globals(self):
        count, table = struct.unpack_from('<2I', self.stage, 0x10)
        return list(struct.unpack_from(f'<{count}I', self.stage, table))

    def row(self, resource08):
        count, base = self.rows
        index = resource08 >> 8
        if index >= count: raise ValueError(f'row {index} out of range')
        return struct.unpack_from('<6I', self.stage, base + index * 24)

    @cached_property
    def world(self):
        return json.loads((ROOT / f'local/assets/native/{self.code}/world_collision.json').read_text())

    @cached_property
    def programs(self):
        return json.loads(pickup_file(self.code, 'disassembly').read_text())['programs']

    @cached_property
    def scripts(self):
        return {p['index']: p for p in json.loads(pickup_file(self.code, 'scripts').read_text())['programs']}

    @cached_property
    def audit(self):
        return json.loads((activation_dir(self.code) / 'countdown-instances.json').read_text())

    @cached_property
    def by_rid(self):
        return {i['rid']: i for i in self.world['instances'] if i['track'] == self.track}

    @cached_property
    def by_resource(self):
        return {(i['rid'] << 8) | i['track']: i for i in self.world['instances']}

    def resource(self, inst):
        return (inst['rid'] << 8) | inst['track']

    def resource08(self, inst):
        return self.world['bindings'][str(inst['track'])]['descriptors'][inst['collision_descriptor']]['resource08']

    def handler_rows(self):
        """(instance, row words) of every own-track instance with a stage handler row."""
        for inst in self.world['instances']:
            if inst['track'] != self.track: continue
            r08 = self.resource08(inst)
            if r08 == 0xFFFFFFFF or r08 & 255 != self.track: continue
            yield inst, self.row(r08)

    def instance_resource_at(self, memory, address):
        """Resource of the runtime instance at `address` (instance +0x78)."""
        return struct.unpack_from('<I', memory, (address + 0x78) & 0x1FFFFFF)[0]


def course_wind_mode(code, elf):
    """0x2D1BA0: course table 0x43D950 row [0x535C08] +0x54, plus 1 (flag manager 0x34C668 wind mode)."""
    u = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    for i in range(64):
        row = 0x43D950 + 0x64 * i
        if elf[row - 0xFF000 + 52:row - 0xFF000 + 68].split(b'\0')[0].decode('latin1') == code:
            return u(row + 0x54) + 1
    raise ValueError(f'{code} not in the course table')
