#!/usr/bin/env python3
"""Private all-rider motion/pose/route and shared RNG checkpoint."""
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from export_world_start import export_world_start, ROOT
from compare_world_reference import compare_world


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path)
    args=parser.parse_args()
    baseline=ROOT/'local/reference/pcsx2/snow-jam-glide.p2s'
    endpoints={1:ROOT/'local/reference/pcsx2/snow-jam-glide-1.p2s',30:ROOT/'local/reference/pcsx2/snow-jam-glide-30-a.p2s'}
    audit=ROOT/'local/native-qa/opponent-assemblies.json'
    if not all(p.exists() for p in (baseline,*endpoints.values(),audit)):
        print('Private owned-game checkpoint unavailable');return 77
    assembly=json.loads(audit.read_text())
    # An identity audit from a different snapshot must never be reused silently.
    mismatched=dict(assembly,ee_sha256='0'*64)
    try:export_world_start(baseline,mismatched)
    except ValueError:pass
    else:raise AssertionError('Unverified assembly identities accepted')
    seed=export_world_start(baseline,assembly)
    for p in seed['participants']:
        assert 'bones' not in p['initial']['original_animation']
    for p in seed['original_pair_collision']['participants']:
        assert 'body' not in p and 'position_cm' not in p
    for p in seed['original_npcs']['riders']:
        assert 'diagnostic_ai_words' not in p and 'accepted_command_history' not in p
    with tempfile.TemporaryDirectory(prefix='ssx-native-world-') as directory:
        start=Path(directory)/'start.json';result=Path(directory)/'result.json'
        start.write_text(json.dumps(seed,allow_nan=False))
        subprocess.run([str(args.binary.resolve()),str(start),'30',str(result)],check=True)
        telemetry=json.loads(result.read_text());assert telemetry['completed']
        for frame,endpoint in endpoints.items():
            comparison=compare_world(dict(telemetry,frames=telemetry['frames'][:frame+1]),endpoint)
            failures=[p for p in comparison['participants'] if not p['exact']]
            assert not failures,json.dumps(dict(frame=frame,failures=failures),indent=2)
            assert not comparison['shared_differences'],comparison['shared_differences']
        print('Six-rider motion, physical orientation, body pose, animation semantic, course/AI routes and shared RNG are exact at frames1 and30.')
    return 0


if __name__=='__main__':sys.exit(main())
