#!/usr/bin/env python3
"""Classify the authored dynamic (descriptor flag 0x40000000) Snow Jam instances by their
original contact handler (development export; writes local/event-activation/scripted-instances.json).

At the audited countdown every such instance has runtime flags 0x40214023 (static body route, bit
0x20) and no entity. A rider contact copies the selected packet to rider+9E0/A60 and 121818 runs
authored handler slot 2 through 30A060 (docs/pickup-recovery.md). Verified classes:
* roller: slot 2 is exactly builtin0() builtin15() return (program 49). builtin0 (2FC0D0) constructs
  the Object entity 356DB0 (vtable 490E80); builtin15 (2FD250) calls 355DB8, which allocates the
  0x2D0-byte "RollerModifier" (35DA70, vtable 48F080) and attaches it (3554B0).
* none: no slot-2 handler (3A6B78 returns -1, 30A060 runs nothing): the instance stays static.
Anything else is rejected, so an unported handler can never be silently treated as static.
"""
import hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records  # noqa: E402


def meshanim_only(words):
    """LUN words: only pushes (0x25 int / 0x26 float with one inline word), one builtin13 call (opcode 0x21, builtin index
    in bits 16..23 = 13) and the return 0xFF2A (tools/disassemble_stage_scripts.py encoding)."""
    k, calls = 0, 0
    while k < len(words):
        w = words[k]; op = w & 0xFF
        if op in (0x25, 0x26): k += 2; continue
        if op == 0x21 and (w >> 16) & 0xFF == 13: calls += 1; k += 1; continue
        if w == 0xFF2A and k == len(words) - 1: return calls == 1
        return False
    return False


def main(location='ARA1'):
    from locations import location as location_info, activation_dir, pickup_file
    from world_assets import locations as sdb_locations
    location_info(location)
    locs = sdb_locations(ROOT / 'local/assets/source/ps2/bam.sdb')
    TRACK = next(i for i, l in enumerate(locs) if l['name'] == location); LAST = locs[TRACK]['chunk_end']
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    u = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    # Builtin table 441F38: builtin0 = Object node, builtin15 = RollerModifier attach.
    assert u(0x441F38) == 0x2FC0D0 and u(0x441F38 + 15 * 4) == 0x2FD250
    assert u(0x2FD3D8) == 0x0C000000 | (0x355DB8 >> 2)          # 2FD250: jal 355DB8
    assert u(0x355E04) == 0x0C000000 | (0x35DA70 >> 2)          # 355DB8: jal 35DA70 (RollerModifier ctor)
    assert u(0x2FC274) == 0x0C000000 | (0x356DB0 >> 2)          # 2FC0D0: jal 356DB0 (Object node)
    assert elf[0x48E908 - 0xFF000:0x48E908 - 0xFF000 + 15] == b'RollerModifier\0'
    for offset, target in ((0x54, 0x361CD8), (0x4C, 0x361CF8), (0x44, 0x361CA8), (0x64, 0x361D18), (0x9C, 0x361D38),
                           (0xA4, 0x360BC8), (0xB4, 0x360BD8)):
        assert u(0x48F080 + offset) == target, hex(offset)
    for offset, target in ((0x74, 0x355420), (0x144, 0x355770), (0x154, 0x356AE0), (0x164, 0x3569D0), (0x16C, 0x356A00),
                           (0x134, 0x356A28), (0x13C, 0x356A70), (0xCC, 0x356128), (0xD4, 0x360990), (0x194, 0x3568B0)):
        assert u(0x490E80 + offset) == target, hex(offset)
    for pc in (0x361CA8, 0x360BC8, 0x361CF8):                     # rigid / override / script re-run predicates: return 0
        assert u(pc) == 0x03E00008 and u(pc + 4) == 0x0000102D
    stage = None   # the course stage (SSB kind 16): last chunk of the location, its own track (ARA1: chunk 33, track 8)
    for ci, chunk in enumerate(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb')):
        if ci == LAST:
            stage = next(d for k, t, r, d in records(chunk) if k == 16 and t == TRACK)
            break
    handlers = struct.unpack_from('<I', stage, 0x1C)[0]   # handler-row table (ARA1 0x70)
    scripts = json.loads(pickup_file(location, 'scripts').read_text())['programs']
    world_path = ROOT / f'local/assets/native/{location}/world_collision.json'
    world = json.loads(world_path.read_text())
    audit = json.loads((activation_dir(location) / 'countdown-instances.json').read_text())
    if hashlib.sha256(world_path.read_bytes()).hexdigest() != audit['world_package_sha256']:
        raise ValueError('Countdown audit/world package mismatch')
    runtime = {r['resource']: r for r in audit['instances']}
    out = []; live = []
    for inst in world['instances']:
        d = world['bindings'][str(inst['track'])]['descriptors'][inst['collision_descriptor']]
        if not d['flags'] & 0x40000000:
            continue
        resource = (inst['rid'] << 8) | inst['track']
        r = runtime[resource]
        skipped = r['runtime_flags'] & 0x60 == 0 and all(r[k] == 'skip' for k in ('body_route', 'ray_mode0_route', 'ray_mode2_route'))
        # ABC1 mdl_ABC1_os609_full_version_inair: a live type-1 node (vtable 0x490B10) whose collision every collector skips
        # at the start (flags without 0x20/0x40); listed with the live entities like the BHP1 cars (docs/backcountry.md).
        if location != 'ARA1' and r['node_type'] is not None and (r['body_route'] == 'entity' or skipped):
            # Already a live entity at the countdown (e.g. BHP1 mdl_BHP1_caryellowanim_*: type-17 Object 0x490E80,
            # an animated set piece): its update is not ported, so it stays out (listed, never treated as static).
            live.append(dict(resource=resource, name=inst['name'], node_type=r['node_type'], node_vtable=r['node_vtable'], runtime_flags=r['runtime_flags']))
            continue
        # ABA1 (mdl_ABA1_cadilackc_*): an authored dynamic instance can be inert at the countdown (runtime flags without the
        # body bits 0x20/0x40, so every collector skips it; drawn static). Its runtime flags go to the seed like any other.
        inert = r['runtime_flags'] & 0x60 == 0 and all(r[k] == 'skip' for k in ('body_route', 'ray_mode0_route', 'ray_mode2_route'))
        if (r['body_route'] != 'static' and not inert) or r['node_type'] is not None or r['authored_flags'] != d['flags']:
            raise ValueError(f'{inst["name"]}: unexpected countdown state {r}')
        if d['resource08'] & 255 != TRACK:
            raise ValueError(f'{inst["name"]}: handler table of another location (track {d["resource08"] & 255})')
        row = struct.unpack_from('<6I', stage, handlers + (d['resource08'] >> 8) * 24)
        slot2 = row[2]
        if slot2 == 0xFFFFFFFF:
            kind, program = 'none', -1
        else:
            program = slot2 >> 8
            words = scripts[program]['code_words'] if slot2 & 255 == TRACK else None
            if words == [0x221, 0xF0221, 0xFF2A]:
                kind = 'roller'
            elif words and meshanim_only(words):
                # ASS1 mdl_ASS1_trainboxes_*: slot 2 = push args, builtin13 (2FCFF0 MeshAnim break pieces), return. No entity
                # or RollerModifier is built; the stage VM runs the handler from the stored contact (web/stage_script_gameplay.inc
                # case 13), so for the roller system the instance stays static ('none').
                kind = 'none'
            else:
                raise ValueError(f'{inst["name"]}: unported contact handler program {program}')
        out.append(dict(resource=resource, name=inst['name'], authored_flags=d['flags'], runtime_flags=r['runtime_flags'],
                        contact=kind, program=program, type=d['type']))
    # RollerModifier ctor 35DA70 reads the runtime tree object of the hit node (collider+0x98):
    # tree+0x2C centre of mass, +0x38 inertia, +0x5C inverse inertia = kind-12 sphere-tree model
    # header +0x1C/+0x28/+0x4C (the runtime object carries a 0x10-byte prefix). Exact bits.
    wanted = {world['bindings'][str(i['track'])]['descriptors'][i['collision_descriptor']]['collision_resource']
              for i in world['instances'] if (i['rid'] << 8 | i['track']) in {x['resource'] for x in out if x['contact'] == 'roller'}}
    trees = {}
    for chunk in world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb'):
        for kind, track, rid, data in records(chunk):
            resource = (rid << 8) | track
            if kind != 12 or resource not in wanted or resource in trees:
                continue
            fmt, count, start, metadata, _ = struct.unpack_from('<2H3I', data, 0)
            if fmt != 3:
                raise ValueError('Roller collision resource is not a sphere tree')
            models, at = [], start
            for _ in range(count):
                extra, encoded, compressed, depth = struct.unpack_from('<4I', data, at)
                words = lambda offset, n: list(struct.unpack_from(f'<{n}I', data, at + offset))
                models.append(dict(center_of_mass_bits=words(0x1C, 3), inertia_bits=words(0x28, 9), inverse_inertia_bits=words(0x4C, 9)))
                at += 0x70 + (depth + 1) * 12 + encoded + extra
            trees[str(resource)] = models
    if set(map(int, trees)) != wanted:
        raise ValueError('Missing roller collision resources')
    target = activation_dir(location) / 'scripted-instances.json'
    target.write_text(json.dumps(dict(elf_sha256=hashlib.sha256(elf).hexdigest(), world_package_sha256=audit['world_package_sha256'],
                                      source_sha256=audit['source_sha256'], instances=out, roller_trees=trees, **({'live_entities_unported': live} if live else {}),
                                      scope='Authored dynamic instances: countdown runtime flags and verified slot-2 contact class'), indent=2) + '\n')
    print(f'{len(out)} scripted instances:', {k: sum(1 for x in out if x['contact'] == k) for k in ('roller', 'none')})


if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--location', default='ARA1'); main(p.parse_args().location)
