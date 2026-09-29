#!/usr/bin/env python3
"""Export a data-only native development start from a verified original state.

This preserves resolved original physical parameters and provenance. It does
not replace the unfinished native event initialization/character-selection path.
"""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path
from compare_reference import prepare


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('state',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--location',default='ARA1');args=parser.parse_args()
    if args.output.resolve()==args.state.resolve():raise ValueError('Output must be separate from source')
    result=prepare(dict(frames=1,events=[]),args.state);result.pop('baseline',None)
    from reference_board_trail import extract_memory as extract_trail
    from reference_snow_context import extract_memory as extract_snow
    with zipfile.ZipFile(args.state) as archive:memory=archive.read('eeMemory.bin')
    actor=int(result['reference_initialization']['rider_address'],0)
    result['native']['initial']['original_board_trail']=extract_trail(memory,actor)
    result['native']['initial']['original_snow']=extract_snow(memory,actor)
    from reference_environment_lighting import extract as extract_environment
    result['native']['initial']['original_environment']=extract_environment(args.state)
    result['native']['location']=args.location
    result['reference_start']=dict(description='Original post-gate riding checkpoint; development start, not full event initialization',
        source_state_sha256=hashlib.sha256(args.state.read_bytes()).hexdigest(),
        source_title='SSX 3 USA PS2',source_character_id=result['native']['initial'].get('original_ground',{}).get('provenance',{}).get('character_id'),
        role='Data-only recovered physics, animation sequence and race starting state; no emulator dependency')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n');print(args.output)


if __name__=='__main__':main()
