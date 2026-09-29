#!/usr/bin/env python3
"""Prepare and compare native replays against original-game state checkpoints.

Only state fields with established units are compared. This does not establish
equivalence of hidden controller state, surface flags, or actor interactions.
"""
import argparse
import hashlib
import json
import math
import zipfile
from pathlib import Path

from reference_probes import riders
from native_replay import verify_inputs
from probe_rider_pose import animation_inputs
from reference_race_event import extract_race_event
from reference_ground_profile import extract_ground
from reference_grab_lifecycle import extract_grab_lifecycle


def read_human(path):
    with zipfile.ZipFile(path) as archive:
        memory = archive.read('eeMemory.bin')
    found = [r for r in riders(memory) if r['kind'] == 'human']
    if len(found) != 1:
        raise ValueError(f'Expected one human rider, found {len(found)}')
    return found[0], hashlib.sha256(memory).hexdigest()


def read_race(path):
    with zipfile.ZipFile(path) as archive:memory=archive.read('eeMemory.bin')
    return dict(ee_sha256=hashlib.sha256(memory).hexdigest(),original_race_event=extract_race_event(memory))


def prepare(spec, baseline):
    rider, digest = read_human(baseline)
    if rider['motion_mode'] not in (0,1):
        raise ValueError('Native comparison currently supports cruise/airborne initial states only')
    # Original local +Y maps along the board in the captured Snow Jam glide.
    # Keep the interpretation explicit; measured velocity is independent of it.
    x, y, z, w = rider['source_quaternion']
    forward_x, forward_y = 2*(x*y-z*w), 1-2*(x*x+z*z)
    spec = dict(spec, baseline=str(baseline.resolve()))
    spec['native'] = dict(location='ARA1', initial=dict(
        position=rider['native_position_m'], velocity=rider['native_velocity_mps'],
        heading=math.atan2(forward_x, -forward_y),grounded=rider['motion_mode']==0,
        original_jump=rider['original_jump']))
    if rider['original_ground'] is not None:
        spec['native']['initial']['original_ground']=rider['original_ground']
    elif rider['motion_mode']==1:
        with zipfile.ZipFile(baseline) as archive:memory=archive.read('eeMemory.bin')
        profile=extract_ground(memory,int(rider['address'],0));profile['active_physics']=False
        spec['native']['initial']['original_ground']=profile
    if rider['original_boost'] is not None:
        spec['native']['initial']['original_boost']=rider['original_boost']
    if rider['original_landing'] is not None:
        spec['native']['initial']['original_landing']=rider['original_landing']
    if rider['original_air_entry'] is not None:
        spec['native']['initial']['original_air_entry']=rider['original_air_entry']
        if rider['original_air_entry']['provenance']['character']==4:
            spec['native']['initial']['original_animation']=animation_inputs(baseline,int(rider['address'],0),collision_enabled=True)
    if rider['original_air_control'] is not None:
        spec['native']['initial']['original_air_control']=rider['original_air_control']
    spec['native']['initial']['original_race_event']=read_race(baseline)
    with zipfile.ZipFile(baseline) as archive:memory=archive.read('eeMemory.bin')
    spec['native']['initial']['original_grab_control']=extract_grab_lifecycle(memory,int(rider['address'],0))
    spec['reference_initialization'] = dict(ee_sha256=digest, rider_address=rider['address'],
        heading_basis='Original quaternion rotated local +Y; horizontal projection.',
        limitations='Original motion mode sets grounded/airborne; native terrain supplies ground normal. Hidden original motion state and NPCs are not restored.')
    return spec



def quaternion_error_degrees(a,b):
    scale=math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    if not scale:raise ValueError('Cannot compare a zero quaternion')
    cosine=min(1.0,abs(sum(x*y for x,y in zip(a,b)))/scale)
    return 2*math.acos(cosine)*180/math.pi


def field_differences(native,original,path=''):
    """Retain every differing recovered field; do not hide drift in a norm."""
    result=[]
    if isinstance(original,dict):
        for key,value in original.items():
            at=f'{path}.{key}' if path else key
            if not isinstance(native,dict) or key not in native:
                result.append(dict(field=at,original=value,native=None,missing=True))
            else:result.extend(field_differences(native[key],value,at))
    elif isinstance(original,list):
        if not isinstance(native,list) or len(native)!=len(original):
            result.append(dict(field=path,original=original,native=native,shape_mismatch=True))
        else:
            for i,(a,b) in enumerate(zip(native,original)):result.extend(field_differences(a,b,f'{path}[{i}]'))
    elif native!=original:result.append(dict(field=path,original=original,native=native))
    return result


def compare(spec, telemetry, checkpoints):
    input_check = verify_inputs(spec, telemetry)
    samples = []
    for frame, path in checkpoints:
        if not 0 <= frame <= spec['frames']:
            raise ValueError('Checkpoint frame outside scenario')
        original, digest = read_human(path)
        native = telemetry['records'][frame]
        position_error = [a-b for a,b in zip(native['position'], original['native_position_m'])]
        velocity_error = [a-b for a,b in zip(native['velocity'], original['native_velocity_mps'])]
        samples.append(dict(frame=frame, time_s=frame/60, state=str(path.resolve()),
            ee_sha256=digest, original=original, native=native,
            position_error_m=position_error, position_error_norm_m=math.sqrt(sum(x*x for x in position_error)),
            velocity_error_mps=velocity_error, velocity_error_norm_mps=math.sqrt(sum(x*x for x in velocity_error))))
        if original['motion_mode']==0 and native.get('grounded') and native.get('original_ground'):
            samples[-1]['ground_orientation_error_degrees']=quaternion_error_degrees(native['original_ground']['quaternion'],original['source_quaternion'])
            samples[-1]['ground_quaternion_components_equal']=list(native['original_ground']['quaternion'])==list(original['source_quaternion'])
        if original['motion_mode']==1 and not native.get('grounded') and native.get('original_air'):
            air=native['original_air'];expected=original.get('original_air_control')
            samples[-1]['air_physical_orientation_error_degrees']=quaternion_error_degrees(air['physical_quaternion'],original['source_quaternion'])
            samples[-1]['air_physical_quaternion_components_equal']=list(air['physical_quaternion'])==list(original['source_quaternion'])
            if expected:
                samples[-1]['air_control_differences']=field_differences(air.get('control'),expected['state'])
                samples[-1]['air_trajectory_differences']=field_differences(air.get('trajectory'),expected['trajectory'])
        if original.get('original_air_entry') and native.get('original_air'):
            samples[-1]['prewind_differences']=field_differences(native['original_air'].get('prewind'),original['original_air_entry']['prewind'])
        if original.get('original_boost') and native.get('original_boost'):
            samples[-1]['boost_differences']=field_differences(native['original_boost'],original['original_boost']['state'])
        if native.get('original_race'):
            race=read_race(path)['original_race_event'];human=[r for r in race['participants'] if r['human']]
            if len(human)!=1:raise ValueError('Race comparison requires one human')
            expected=dict(clock=race['clock'],human={k:human[0][k] for k in ('path_index','remaining','best_remaining','path_cache','finish_elapsed','finish_ticks','penalty_ticks')},
                          checkpoint_mask=race['checkpoints']['human_masks'][0],pending_human_mask=race['checkpoints']['pending_human_mask'])
            samples[-1]['race_differences']=field_differences(native['original_race'],expected)
        samples[-1]['control_state_equal']=native.get('control_state')==original['control_state']
        samples[-1]['body_collision_coverage']=native.get('body_collision')
    return dict(input_check=input_check, samples=samples,
        note='Measured discrepancies, not a conformance pass. Equal controller bytes do not imply equal hidden controller state or collision behavior.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    prep=sub.add_parser('prepare');prep.add_argument('scenario',type=Path);prep.add_argument('baseline',type=Path);prep.add_argument('output',type=Path)
    comp=sub.add_parser('compare');comp.add_argument('scenario',type=Path);comp.add_argument('telemetry',type=Path)
    comp.add_argument('--checkpoint',action='append',required=True,help='FRAME=STATE_PATH')
    comp.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();spec=json.loads(args.scenario.read_text())
    if args.command=='prepare':
        report=prepare(spec,args.baseline)
    else:
        checkpoints=[]
        for item in args.checkpoint:
            frame,path=item.split('=',1);checkpoints.append((int(frame),Path(path)))
        report=compare(spec,json.loads(args.telemetry.read_text()),checkpoints)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(args.output)


if __name__=='__main__':main()
