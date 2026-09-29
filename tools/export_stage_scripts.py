#!/usr/bin/env python3
"""Course stage scripts for the browser core (docs/stage-scripts.md).

Every loaded location track has its own course stage (SSB kind 16 record of that track in the
location's last chunk): the race course (ARA1 track 8, BRA2 16, BHP1 15) and the resident extra
locations of its event world (A_ARA1 3 / ARA1_B 9, B_BRA2 14, B_BHP1 13; world_collision.json
`event_locations`). A stage holds (tools/import_stage_scripts.py):
  +0x10 global handler count/offset, +0x18 handler row count/offset (six words per row),
  +0x38 program count, +0x3C program offset table, +0x40 end of the programs.
An instance's row is collision descriptor resource08 >> 8 of the stage of track resource08 & 255
(always the instance's own track); each slot word is program << 8 | track (0xFFFFFFFF = none).
Slots: 1 section load, 2 selected rider contact (121818 -> 30A060), 4 end/completion, 5 LiveComp timer.

Output web/generated/stage_scripts_seed.hpp, per course namespace browser_stage_<course>:
  stages[]   = {track, first program, program count, first global, global count}
  programs[] = {first word, word count} (header 0x004E554C, code end, extent, extent; code; trailer)
  words[]    = all program words
  globals[]  = global handler program indices (stage-local, -1 none)
  collectible_award = 30B9A0 collectible award amount (151178 by the course table peak level)
  handlers[] = {instance resource (rid << 8 | track), slot programs (stage-local, -1 none)} by resource
  entities[] = {instance resource, contact class} for handler instances that carry an entity at the
               countdown (local/event-activation/[<LOC>/]countdown-instances.json): class 1 = entity
               vtable +0x140 is 355770 (Object 0x490E80, LiveComp 0x490B10, emitter 0x48EE60: the
               30-tick entity+0x20 contact guard, then 30A060), class 0 = a no-op contact (flag cloth
               0x48FC10, type-16 node 0x491800, DeadNode 0x491B00: slot 2 never runs)
SPDX-License-Identifier: GPL-3.0-only
"""
import json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations  # noqa: E402
from locations import activation_dir, LOCATIONS, elf_read  # noqa: E402

COURSES = ('ARA1', 'BRA2', 'BHP1', 'ASS1', 'ABA1', 'ABC1')   # ABC1: Happiness (docs/backcountry.md)


def courses():
    """COURSES plus every other registered location (tools/locations.py: Peak 2 docs/peak2.md, Peak 3) whose event
    evidence is prepared (world_collision.json + countdown audit), in registry order. Each gets
    `#define BROWSER_STAGE_HAVE_<CODE> 1` so web/stage_script_gameplay.inc can select it only when it was exported."""
    extra = [c for c in LOCATIONS if c not in COURSES and (ROOT / f'local/assets/native/{c}/world_collision.json').exists()
             and (activation_dir(c) / 'countdown-instances.json').exists()]
    return COURSES + tuple(extra)
MAGIC = 0x004E554C
# entity vtable -> 121818 contact class (vtable+0x144 target): 355770 gate or a no-op
CONTACT_CLASS = {0x490E80: 1, 0x490B10: 1, 0x48EE60: 1, 0x48FC10: 0, 0x491800: 0, 0x491B00: 0,
                 0x4908F8: 1, 0x4914E0: 0}   # AnimTeeter (A_ASS1 log teeter) gate; one-way volume (Boost) no-op 0x3609F0


def stage_records():
    """{track: stage bytes} for every location's own-track stage in its last chunk."""
    locs = locations(ROOT / 'local/assets/source/ps2/bam.sdb')
    last = {i: l['chunk_end'] for i, l in enumerate(locs)}
    out = {}
    for ci, chunk in enumerate(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb')):
        for kind, track, rid, data in records(chunk):
            if kind == 16 and last.get(track) == ci:
                assert track not in out
                out[track] = bytes(data)
    return out, locs


def parse_stage(stage):
    u = lambda p: struct.unpack_from('<I', stage, p)[0]
    count, table, end = u(0x38), u(0x3C), u(0x40)
    offsets = list(struct.unpack_from(f'<{count}I', stage, table)) + [end]
    assert offsets == sorted(set(offsets)) and offsets[0] == (table + count * 4 + 15) & ~15
    programs = []
    for i, (start, stop) in enumerate(zip(offsets, offsets[1:])):
        magic, code_end, extent, other = struct.unpack_from('<4I', stage, start)
        assert magic == MAGIC and 16 <= code_end <= extent <= stop - start and other == extent and code_end % 4 == 0
        assert extent == stop - start or (i == count - 1 and not any(stage[start + extent:stop]))
        programs.append(list(struct.unpack_from(f'<{extent // 4}I', stage, start)))
    gcount, gtable = u(0x10), u(0x14)
    globals_ = list(struct.unpack_from(f'<{gcount}I', stage, gtable))
    rcount, rbase = u(0x18), u(0x1C)
    rows = [struct.unpack_from('<6I', stage, rbase + 24 * i) for i in range(rcount)]
    return programs, globals_, rows


def export(course, stages, locs):
    world = json.loads((ROOT / f'local/assets/native/{course}/world_collision.json').read_text())
    tracks = [e['track'] for e in world['event_locations']]
    assert world['location'] == course and all(locs[t]['name'] == e['name'] for t, e in zip(tracks, world['event_locations']))
    stage_rows, program_rows, words, globals_out, handlers = [], [], [], [], []
    for track in tracks:
        programs, globals_, rows = parse_stage(stages[track])
        first_program, first_global = len(program_rows), len(globals_out)
        for body in programs:
            program_rows.append((len(words), len(body)))
            words += body
        for w in globals_:
            assert w == 0xFFFFFFFF or (w & 255) == track
            globals_out.append(-1 if w == 0xFFFFFFFF else w >> 8)
        stage_rows.append((track, first_program, len(programs), first_global, len(globals_)))
        for inst in world['instances']:
            if inst['track'] != track:
                continue
            r08 = world['bindings'][str(track)]['descriptors'][inst['collision_descriptor']]['resource08']
            if r08 == 0xFFFFFFFF:
                continue
            assert r08 & 255 == track and (r08 >> 8) < len(rows), (course, inst['name'], hex(r08))
            row = rows[r08 >> 8]
            slots = []
            for w in row:
                assert w == 0xFFFFFFFF or (w & 255) == track, (course, inst['name'], hex(w))
                slots.append(-1 if w == 0xFFFFFFFF else w >> 8)
                assert slots[-1] < len(programs)
            if any(s >= 0 for s in slots):
                handlers.append(((inst['rid'] << 8) | track, slots))
    handlers.sort()
    assert len({r for r, _ in handlers}) == len(handlers)
    audit = {r['resource']: r for r in json.loads((activation_dir(course) / 'countdown-instances.json').read_text())['instances']}
    entities = []
    for resource, _ in handlers:
        r = audit.get(resource)
        if r and r['node_vtable'] is not None:
            entities.append((resource, CONTACT_CLASS[r['node_vtable']]))
    return stage_rows, program_rows, words, globals_out, handlers, entities


def collectible_award(course):
    """30B9A0 -> 10F338 amount: 151178(144C98()) = 500/1000/2000 by course table 0x43D950 row +0x54 (0..2),
    the row of the current course 0x535C08 (= the location's course index)."""
    level = struct.unpack('<i', elf_read(0x43D950 + 0x64 * LOCATIONS[course]['course_index'] + 0x54, 4))[0]
    return 1000 if level == 1 else 2000 if level == 2 else 500


def main():
    stages, locs = stage_records()
    out = ['// Generated by tools/export_stage_scripts.py: course stage LUN programs and handler rows (docs/stage-scripts.md).',
           '#pragma once', '#include <array>', '#include <cstdint>',
           'struct BrowserStageInfo {uint32_t track,firstProgram,programCount,firstGlobal,globalCount;};',
           'struct BrowserStageProgram {uint32_t first,count;};',
           'struct BrowserStageHandler {uint32_t resource;std::array<int16_t,6> slots;};']
    for course in courses():
        stage_rows, programs, words, globals_, handlers, entities = export(course, stages, locs)
        ns = 'browser_stage_' + course.lower()
        out.append(f'#define BROWSER_STAGE_HAVE_{course} 1')
        out.append(f'namespace {ns} {{')
        out.append(f'inline constexpr std::array<BrowserStageInfo,{len(stage_rows)}> stages={{{{' + ','.join('{' + ','.join(f'{x}u' for x in s) + '}' for s in stage_rows) + '}};')
        out.append(f'inline constexpr std::array<BrowserStageProgram,{len(programs)}> programs={{{{' + ','.join(f'{{{a}u,{b}u}}' for a, b in programs) + '}};')
        out.append(f'inline constexpr std::array<uint32_t,{len(words)}> words={{{{' + ','.join(f'0x{w:x}u' for w in words) + '}};')
        out.append(f'inline constexpr std::array<int16_t,{len(globals_)}> globals={{{{' + ','.join(str(g) for g in globals_) + '}};')
        out.append(f'inline constexpr std::array<BrowserStageHandler,{len(handlers)}> handlers={{{{' + ','.join(f'{{{r}u,{{{{{",".join(str(s) for s in slots)}}}}}}}' for r, slots in handlers) + '}};')
        out.append(f'inline constexpr int32_t collectible_award={collectible_award(course)};')
        out.append(f'inline constexpr std::array<std::array<uint32_t,2>,{len(entities)}> entities={{{{' + ','.join(f'{{{{{r}u,{c}u}}}}' for r, c in entities) + '}};')
        out.append('}')
        print(course, len(entities), 'countdown entities', 'stages', [(locs[s[0]]['name'], s[0], s[2]) for s in stage_rows], len(words), 'words', len(handlers), 'handler rows')
    target = ROOT / 'web/generated/stage_scripts_seed.hpp'
    target.write_text('\n'.join(out) + '\n')
    print('wrote', target.relative_to(ROOT))


if __name__ == '__main__':
    main()
