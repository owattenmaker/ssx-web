#!/usr/bin/env python3
"""Compare every selected native rider against one original frame endpoint.

This checks only the exported fields. Shared RNG is a separate required check;
an exact pose/motion result does not imply the whole game is equivalent.
"""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path
from compare_reference import field_differences
from reference_probes import riders
from reference_npc import extract_npcs
from reference_race_event import extract_race_event
from reference_pair_collision import extract_pair_collision


def compare_world(telemetry, snapshot):
    with zipfile.ZipFile(snapshot) as archive:
        memory = archive.read('eeMemory.bin')
    native = telemetry['frames'][-1]
    discovered = {r['rider_slot']: r for r in riders(memory)}
    race = extract_race_event(memory)
    npcs = extract_npcs(memory)
    pairs = extract_pair_collision(memory)
    if len(native['participants']) != len(race['participants']):
        raise ValueError('Native and original selected rosters differ')
    results = []
    for slot, actual in enumerate(native['participants']):
        if actual['slot'] != slot:
            raise ValueError('Native roster order differs')
        original = discovered[slot]
        route = npcs['participant_routes'][slot]
        course = race['participants'][slot]
        base = int(original['address'], 0)
        animator = struct.unpack_from('<I', memory, base+0x784)[0]
        expected = dict(slot=slot, position_cm=list(original['position_cm']),
            velocity_cmps=list(original['integration_vector_1e0']),
            quaternion=list(original['source_quaternion']), control_state=original['control_state'],
            grounded=original['motion_mode'] == 0, time_scale=original['frame_time_multiplier'],
            body_available=True,
            body_spheres_cm=[s['center_cm'] for s in pairs['participants'][slot]['body']['spheres']],
            animation=struct.unpack_from('<i', memory, animator+8)[0],
            course_path=course['path_index'], course_remaining=course['remaining'],
            course_best_remaining=course['best_remaining'], ai_path=route['path_index'],
            ai_distance=route['current_distance'], ai_previous_distance=route['previous_distance'],
            ai_closest=route['closest_point'], ai_lookahead=route['lookahead_point'],
            ai_previous_lookahead=route['previous_lookahead_point'],
            ai_lateral=route['lateral_distance'], ai_heading=route['heading'])
        differences = field_differences(actual, expected)
        results.append(dict(slot=slot, exact=not differences, differences=differences,
            compared_fields=list(expected),
            commands_native=actual['commands'], commands_comparison='Separate original provider oracle required; NPCs have no human recorder'))
    global_differences = field_differences(native, dict(tick=race['clock']['total_ticks'],
        race_tick=race['clock']['race_ticks'], random_state=pairs['random_state']))
    return dict(endpoint_ee_sha256=hashlib.sha256(memory).hexdigest(),
        frame=native['frame'], participants=results, shared_differences=global_differences,
        exact=not global_differences and all(p['exact'] for p in results),
        scope='All selected riders: physical state, pose spheres, animation semantic, course and AI route fields, shared clock and RNG. Not full game equivalence.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('telemetry', type=Path)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.resolve() in (args.telemetry.resolve(), args.snapshot.resolve()):
        raise ValueError('Comparison output must not overwrite input')
    report = compare_world(json.loads(args.telemetry.read_text()), args.snapshot)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(json.dumps(dict(exact=report['exact'], riders_exact=sum(r['exact'] for r in report['participants']),
        riders=len(report['participants']), shared_differences=len(report['shared_differences']))))


if __name__ == '__main__':
    main()
