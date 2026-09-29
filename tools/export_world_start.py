#!/usr/bin/env python3
"""Export one verified, data-only six-rider start for the native shared world.

The assembly audit must name this exact EE snapshot. Pose outputs, opaque NPC
words and recorded future commands are intentionally not runtime inputs.
"""
import argparse
import hashlib
import json
import math
import struct
import zipfile
from pathlib import Path

from reference_probes import riders
from reference_ground_profile import extract_ground
from reference_air_entry import extract_air_entry
from reference_air_control import extract_air_control
from reference_boost import extract_boost
from reference_landing import extract_landing
from reference_race_event import extract_race_event
from reference_pair_collision import extract_pair_collision
from reference_npc import extract_npcs
from probe_rider_pose import animation_inputs
from reference_upper_reaction import extract_upper_reaction
from reference_grab_lifecycle import extract_grab_lifecycle

ROOT = Path(__file__).resolve().parents[1]


def export_world_start(snapshot, assembly_audit, location='ARA1'):
    with zipfile.ZipFile(snapshot) as archive:
        memory = archive.read('eeMemory.bin')
    digest = hashlib.sha256(memory).hexdigest()
    if assembly_audit['ee_sha256'] != digest:
        raise ValueError('Assembly identities were not verified against this EE snapshot')
    race = extract_race_event(memory)
    pairs = extract_pair_collision(memory)
    npcs = extract_npcs(memory)
    discovered = {int(r['address'], 0): r for r in riders(memory)}
    assemblies = assembly_audit['participants']
    if len(assemblies) != pairs['active_count'] or len(assemblies) != len(race['participants']):
        raise ValueError('Assembly, race and collision rosters differ')
    participants = []
    for slot, (assembly, pair) in enumerate(zip(assemblies, pairs['participants'])):
        address = int(assembly['rider_address'], 0)
        if assembly['slot'] != slot or int(pair['rider'], 0) != address:
            raise ValueError('Assembly order differs from original selected roster')
        actor = discovered[address]
        if actor['rider_slot'] != slot or actor['kind'] not in ('human', 'computer'):
            raise ValueError('Selected actor is not a supported participant')
        if actor['motion_mode'] not in (0, 1) or actor['control_state'] not in (0, 2, 5):
            raise ValueError(f'Slot {slot} requires an unimplemented initial lifecycle')
        package = 'RIDER_' + assembly['character'].upper()
        rig_path = ROOT / 'local/assets/native' / package
        ground = extract_ground(memory, address)
        if ground['provenance']['character_id'] != assembly['gameplay_character_id']:
            raise ValueError('Gameplay profile identity differs from assembly audit')
        ground['active_physics'] = actor['motion_mode'] == 0
        x, y, z, w = actor['source_quaternion']
        initial = dict(position=actor['native_position_m'], velocity=actor['native_velocity_mps'],
            heading=math.atan2(2*(x*y-z*w), -(1-2*(x*x+z*z))),
            grounded=actor['motion_mode'] == 0, is_human=actor['kind'] == 'human',
            original_jump=actor['original_jump'], original_ground=ground,
            original_air_entry=extract_air_entry(memory, address),
            original_boost=extract_boost(memory, address),
            original_landing=extract_landing(memory, address),
            original_animation=animation_inputs(snapshot, address, rig_path=rig_path))
        if actor['control_state'] == 5:
            initial['original_air_control'] = extract_air_control(memory, address)
        upper=extract_upper_reaction(memory,address)
        for key in ('reaction_mask','lookback_mask'):upper[key]=hex(upper[key])
        participants.append(dict(slot=slot, package=package,
            visual_character=assembly['character'], gameplay_character_id=assembly['gameplay_character_id'],
            control_state=actor['control_state'], motion_mode=actor['motion_mode'],
            #121880 skips112338 when AC4 is nonzero (an inhibition flag).
            track_progress_enabled=struct.unpack_from('<I', memory, address+0xac4)[0] == 0,
            upper_reaction=upper,grab_control=extract_grab_lifecycle(memory,address),uber_enabled=bool(struct.unpack_from('<I',memory,address+0xb2c)[0]&1),initial=initial))
        # Live native poses provide body bounds and physical state. Keep only
        # typed persistent pair metadata, including the previous proximity scan.
        for key in ('body', 'position_cm', 'velocity_cmps', 'ground_normal', 'physical_up',
                    'presentation', 'motion_mode', 'control_state', 'boost', 'rider'):
            pair.pop(key, None)
        for record in pair['records']:
            record.pop('interface_value', None)
    for npc in npcs['riders']:
        for key in ('diagnostic_ai_words', 'accepted_command_history', 'ground'):
            npc.pop(key, None)
    pairs.pop('manager', None)
    pairs['provenance']['captured_body_geometry'] = 'Excluded; supplied by native pose stages each frame'
    return dict(schema_version=1, location=location, participants=participants,
        animation_classes=[struct.unpack_from('<i', memory, 0x446990+n*28)[0] for n in range(0x1b6)],
        original_race_event=dict(ee_sha256=digest, original_race_event=race),
        original_pair_collision=pairs, original_npcs=npcs,
        provenance=dict(ee_sha256=digest, snapshot_sha256=hashlib.sha256(snapshot.read_bytes()).hexdigest(),
            assembly_audit_ee_sha256=assembly_audit['ee_sha256'],
            role='Initial state only; all subsequent controls, poses and motion must run natively'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--assemblies', type=Path, default=ROOT/'local/native-qa/opponent-assemblies.json')
    parser.add_argument('--location', default='ARA1')
    args = parser.parse_args()
    if args.output.resolve() in (args.snapshot.resolve(), args.assemblies.resolve()):
        raise ValueError('Output must be separate from source inputs')
    result = export_world_start(args.snapshot, json.loads(args.assemblies.read_text()), args.location)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    print(f"Exported {len(result['participants'])} native participant seeds: {args.output}")


if __name__ == '__main__':
    main()
