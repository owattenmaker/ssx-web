#!/usr/bin/env python3
"""Compare retained original flight/contact fields after a recorded landing.

These fields are read at their real original addresses even when the snapshot
is now grounded. They are historical checkpoints, not current airborne state.
"""
import argparse, hashlib, json, struct, zipfile
from pathlib import Path
from reference_input import float32_zero


def audit(snapshot, native):
    with zipfile.ZipFile(snapshot) as archive:
        memory = archive.read('eeMemory.bin')
    if len(memory) != 32 * 1024 * 1024:
        raise ValueError('Expected32MiB EE memory')
    def read(fmt, at):
        if at < 0 or at + struct.calcsize(fmt) > len(memory):
            raise ValueError('Original pointer outside EE memory')
        return struct.unpack_from(fmt, memory, at)
    rider = 0x14701a0
    owner = read('<I', rider + 0x77c)[0]
    trajectory = read('<I', rider + 0x788)[0]
    if read('<i', owner + 0xde0)[0] != 0:
        raise ValueError('Snapshot must already be grounded')
    data = json.loads(Path(native).read_text())
    records = data['records']
    final = records[-1]
    if not final['grounded']:
        raise ValueError('Native replay must already be grounded')
    current = final['original_air']['trajectory']
    fields = []
    def compare(name, expected, actual):
        fields.append(dict(field=name, reference=expected, native=actual, exact=expected == actual))
    for name, offset in [('hit_position', 0), ('heading', 0x10), ('normal', 0x20),
            ('apex_position', 0x40), ('prediction.position', 0x50),
            ('prediction.velocity', 0x60), ('integrated.position', 0x70),
            ('integrated.velocity', 0x80)]:
        actual = current
        for key in name.split('.'):
            actual = actual[key]
        compare('retained_trajectory.' + name, list(read('<3f', trajectory + offset)), actual)
    for name, offset, fmt in [('patch_id', 0x30, 'i'), ('patch_u', 0x34, 'f'),
            ('patch_v', 0x38, 'f'), ('surface', 0x90, 'i'), ('patch_flags', 0x94, 'h'),
            ('predicted_time', 0x98, 'f'), ('apex_time', 0x9c, 'f'), ('elapsed', 0xa0, 'f'),
            ('integrated_time', 0xa4, 'f'), ('speed_limit', 0xa8, 'f'), ('status', 0xac, 'i')]:
        compare('retained_trajectory.' + name, read('<' + fmt, trajectory + offset)[0], current[name])
    landings = [row for row in records if row.get('original_landing', {}).get('last_landing_tick', 0)]
    if not landings:
        raise ValueError('Native replay has no recovered landing event')
    landing = landings[0]
    if final['original_landing']['last_landing_tick'] != landing['original_landing']['last_landing_tick']:
        raise ValueError('Multiple native landings need separate checkpoint selection')
    event = landing['original_landing']
    compare('ground_focus_tick', read('<I', owner + 0x10)[0], event['ground_focus_tick'])
    compare('ground_leave_tick', read('<I', owner + 0x14)[0], event['last_ground_leave_tick'])
    velocity = landing['original_air']['trajectory']['integrated']['velocity']
    normal = event['probe_normal']
    surface = event['probe_surface_velocity_cmps']
    terms = [float32_zero(float32_zero(v - sv) * n) for v, sv, n in zip(velocity, surface, normal)]
    impact = -float32_zero(float32_zero(float32_zero(terms[0] + terms[1]) + terms[2]) + 0.0)
    compare('retained_impact_normal_speed', read('<f', rider + 0x770)[0], impact)
    game = read('<I', read('<I', read('<I', 0x4a30f0 - 0x848)[0] + 0x84)[0] + 0xc)[0]
    own_slot = read('<I', rider + 0x86c)[0]
    interactions = []
    for slot in range(6):
        record = rider + slot * 0x24
        if not read('<I', record)[0]:
            continue
        last_tick = read('<I', record + 0x10)[0]
        if not last_tick:
            continue
        peer = read('<I', game + 0x28 + slot * 4)[0]
        counterpart = read('<I', peer + own_slot * 0x24 + 0x10)[0]
        interactions.append(dict(opponent_slot=slot, opponent_address=f'0x{peer:08x}',
            last_impulse_tick=last_tick, counterpart_last_impulse_tick=counterpart,
            after_landing=last_tick > event['ground_focus_tick'],
            source='107888:107BCC..107BD4 writes both pair records after107E70 impulses'))
    return dict(snapshot=str(Path(snapshot).resolve()), ee_sha256=hashlib.sha256(memory).hexdigest(),
        native=str(Path(native).resolve()), native_sha256=hashlib.sha256(Path(native).read_bytes()).hexdigest(),
        native_landing_frame=landing['frame'], original_rider_pair_collisions=interactions, exact=all(field['exact'] for field in fields), fields=fields,
        scope='Retained trajectory and landing-impact fields only; does not establish touchdown quaternion or later ground equivalence.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('native', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = audit(args.snapshot, args.native)
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    print(text)
    if not result['exact']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
