#!/usr/bin/env python3
"""Prepare physics conformance using the commands SSX3 actually accepted.

Requested emulator pad events remain provenance; the native simulation uses the
independently decoded original game command stream, with its baseline verified.
"""
import argparse
import copy
import hashlib
import json
import zipfile
from pathlib import Path
from compare_reference import prepare, read_human
from reference_input import extract_input, accepted_delta, raw_accepted_delta


def canonical(spec,outcome,*,raw_commands=False):
    baseline=Path(spec['baseline']);before,before_hash=read_human(baseline);after,after_hash=read_human(outcome)
    with zipfile.ZipFile(baseline) as archive:base_memory=archive.read('eeMemory.bin')
    with zipfile.ZipFile(outcome) as archive:end_memory=archive.read('eeMemory.bin')
    decode=raw_accepted_delta if raw_commands else accepted_delta
    accepted=decode(extract_input(base_memory,int(before['address'],0)),extract_input(end_memory,int(after['address'],0)))
    accepted.update(baseline_state=str(baseline.resolve()),baseline_ee_sha256=before_hash,
                    outcome_state=str(outcome.resolve()),outcome_ee_sha256=after_hash)
    count=accepted['frames']
    if count>spec['frames']:raise ValueError('Accepted endpoint exceeds the requested scenario frame range')
    request=copy.deepcopy(spec)
    prefix=copy.deepcopy(spec);prefix['frames']=count
    prefix['events']=[dict(event,end=min(event['end'],count)) for event in spec.get('events',[]) if event['start']<count]
    result=prepare(prefix,baseline)
    result['native']['accepted_input']=accepted
    result['reference_requested_scenario']=request
    result['reference_timing_note']='Native physics consumes original game-accepted commands. Requested emulator pad events are retained separately and are not assumed to arrive at the same logic tick.'
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('scenario',type=Path)
    parser.add_argument('--raw-commands',action='store_true',help='Replay original words through native controller state without inferring original state transitions');parser.add_argument('outcome',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    if args.output.resolve() in (args.scenario.resolve(),args.outcome.resolve()):raise ValueError('Canonical output must be separate from inputs')
    result=canonical(json.loads(args.scenario.read_text()),args.outcome,raw_commands=args.raw_commands)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(args.output)


if __name__=='__main__':main()
