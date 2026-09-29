#!/usr/bin/env python3
"""Section activation (0x101B60) of the streamed Peak 1 world (docs/peak-mountain.md "Section activation").

Same model as tools/export_sections.py (engine/section_streaming.hpp) for one world that holds every resident Peak 1
location at once: every location's instances are inserted into the one activation octree when its chunk read completes
(resolver kind 3 -> 328C20) and leave it at the eviction (3AB498 -> 3284B8); the streamer's row -> 1/2 (230338) sets
A+0xD0 = -1 (rescan) and its row 5 -> 7 (230360 -> 103308) drops the location's instances from the list. The browser
gates the instances by track residency (web/peak_world.inc, engine/world_residency.hpp worldTrackOctree).

Writes web/public/assets/PEAK1/SECTIONS/sections.json (git-ignored):
  instances[]  resource, cell (0x328F28 of the package bounds), slot1/slot3 = (track << 16) | program (the programs of
               one location's stage are numbered per track; web/stage_script_gameplay.inc masks the low 16 bits),
               entity_at_start null (every location loads without entities)
  programs{}   the same encoded ids: builtins, gameplay-RNG draws of a run, creates (entity kind the slot-1 program
               leaves on its instance, most common over the free-ride / peak-run savestates below)
Cells are checked against the live octree of every savestate (resident locations), handler rows against the runtime
lookup. Savestates are only read.

    .venv/bin/python tools/export_peak_sections.py [--peak N]   (N = 3: web/public/assets/PEAK3, docs/peak3.md)
"""
import argparse, glob, hashlib, json, struct, sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from export_sections import Memory, instance_rows, cell_from_bounds, program_summary  # noqa: E402
from export_stage_scripts import stage_records, parse_stage  # noqa: E402

WEB = ROOT / 'web/public/assets/PEAK1'
STATES = ['local/ps2-capture/peak1/*.p2s', 'local/ps2-capture/menus/fr/*.p2s', 'local/ps2-capture/menus/streamres/*.p2s',
          'local/ps2-capture/menus/ctm/state-*.p2s']
PEAK_STATES = {2: ['local/ps2-capture/peak2/*.p2s', 'local/ps2-capture/runs/peak2/fr-*.tick*.p2s'],   # Peak 2 free ride (docs/peak2.md)
               3: ['local/ps2-capture/peak3/*.p2s', 'local/ps2-capture/runs/peak3/fr-*.tick*.p2s']}   # Peak 3 free ride (docs/peak3.md)
WIDE = {0x14, 0x15, 0x16, 0x17, 0x1D, 0x24, 0x25, 0x26, 0x27}


def disassemble(words):
    """Instruction list of one program (tools/disassemble_stage_scripts.py): code from word 4 to the code end."""
    code_end = words[1] // 4; out = []; i = 4
    while i < code_end:
        w = words[i]; op = w & 255; width = 2 if op in WIDE else 1
        e = dict(word=w, opcode=op, inline_words=words[i + 1:i + width])
        if op == 0x21: e['builtin'] = (w >> 16) & 255
        out.append(e); i += width
    return out


def main():
    global WEB, STATES
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--peak', type=int, default=1, choices=[1, 2, 3]); peak = p.parse_args().peak
    if peak != 1: WEB = ROOT / f'web/public/assets/PEAK{peak}'; STATES = PEAK_STATES.get(peak, [])
    stages, locs = stage_records()
    manifest = json.loads((WEB / 'peak.json').read_text())
    codes = {l['track']: l['code'] for l in manifest['locations']}
    instances, programs, packages = [], {}, {}
    for track, code in sorted(codes.items()):
        world = json.loads((WEB / code / 'world_collision.json').read_text())
        progs, _, rows = parse_stage(stages[track])
        for inst in world['instances']:
            if inst['track'] != track: continue
            resource = (inst['rid'] << 8) | track
            packages[resource] = inst
            r08 = world['bindings'][str(track)]['descriptors'][inst['collision_descriptor']]['resource08']
            row = rows[r08 >> 8] if r08 != 0xFFFFFFFF else None
            slot = lambda k: -1 if row is None or row[k] == 0xFFFFFFFF else (track << 16) | (row[k] >> 8)
            s1, s3 = slot(1), slot(3)
            if s1 < 0 and s3 < 0: continue
            instances.append(dict(resource=resource, name=inst.get('name'), cell=list(cell_from_bounds(inst['bounds_min_cm'], inst['bounds_max_cm'])),
                                  slot1=s1, slot3=s3, entity_at_start=None))
            for s, k in ((s1, 1), (s3, 3)):
                if s >= 0: programs.setdefault(s, dict(program_summary(dict(instructions=disassemble(progs[s & 0xFFFF]))), slots=set()))['slots'].add(k)
    by_resource = {i['resource']: i for i in instances}
    # live octree checks + slot-1 entity kinds (instances in the activation list whose slot-1 program ran)
    kinds = defaultdict(Counter); checked = 0; states = []; movers = {}
    for path in sorted(p for pattern in STATES for p in glob.glob(str(ROOT / pattern))):
        try: m = Memory(path)
        except Exception: continue
        try:
            if struct.unpack_from('<b', m.ee, 0x535C10)[0] not in (4, 5, 6): continue  # free ride / peak runs
            rows = instance_rows(m); act = set(m.active_list())
        except (struct.error, IndexError, KeyError): continue
        states.append(Path(path).name)
        for r in rows.values():
            mine = by_resource.get(r['resource'])
            if r['resource'] in packages:
                p = packages[r['resource']]
                if tuple(cell_from_bounds(p['bounds_min_cm'], p['bounds_max_cm'])) != tuple(r['cell']):
                    if r['entity'] is None: raise ValueError(f'{Path(path).name}: octree cell of {r["resource"]:#x} differs from its package bounds')
                    movers[r['resource']] = r['entity_class']  # entity-routed movers (0x3291E0: entity bounds)
                else: checked += 1
            if mine and r['stage']:
                if ((mine['slot1'] & 0xFFFF) if mine['slot1'] >= 0 else -1) != (r['slot1'] >> 8 if r['slot1'] != -1 else -1):
                    raise ValueError(f'{Path(path).name}: slot-1 row of {r["resource"]:#x} differs from the stage record')
                if mine['slot1'] >= 0 and r['inst'] in act: kinds[mine['slot1']][r['entity']] += 1
    for pid, c in kinds.items():
        (kind, _), = c.most_common(1)
        programs[pid]['creates'] = kind; programs[pid]['creates_observed'] = {str(k): v for k, v in c.items()}
    out = dict(version=1, location=f'PEAK{peak}', name=f'Peak {peak} (streamed world)',
               schema='tools/export_peak_sections.py; instances/programs as tools/export_sections.py with slot ids (track << 16) | program',
               provenance=dict(savestates=states, cells_checked=checked, movers={f'{k:#x}': v for k, v in sorted(movers.items())}, elf_sha256=hashlib.sha256((ROOT / 'local/disc/SLUS_207.72').read_bytes()).hexdigest()),
               programs={str(k): dict(v, slots=sorted(v['slots'])) for k, v in sorted(programs.items())},
               instances=sorted(instances, key=lambda i: i['resource']), chunks=[], chunk_trees=[])
    target = WEB / 'SECTIONS/sections.json'; target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(out, separators=(',', ':')) + '\n')
    print(json.dumps(dict(output=str(target.relative_to(ROOT)), instances=len(instances), programs=len(programs), savestates=len(states), cells_checked=checked, movers=len(movers),
                          creates=dict(Counter(v.get('creates') for v in programs.values())))))


if __name__ == '__main__':
    main()
