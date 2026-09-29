#!/usr/bin/env python3
"""Run native headless input scenarios and verify exact PCSX2 movie stimulus bytes.

Movement equations remain explicitly provisional. Matching stimulus bytes does
not establish matching native/original-game state or analog response curves.
"""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path
from reference_replay import pad_frame

ROOT=Path(__file__).resolve().parents[1]


def verify_inputs(spec,telemetry):
    records=telemetry['records']
    if telemetry['fps']!=60 or len(records)!=spec['frames']+1:
        raise ValueError('Telemetry cadence/row count mismatch')
    if records[0]['frame']!=0 or records[0]['input_frame']!=-1:
        raise ValueError('Missing initial-state row')
    for frame,row in enumerate(records[1:]):
        buttons=[];left=(127,127);right=(127,127)
        for event in spec.get('events',[]):
            if event['start']<=frame<event['end']:
                buttons.extend(event.get('buttons',[]))
                left=event.get('left',left);right=event.get('right',right)
        expected=pad_frame(buttons,left,right)
        if row['frame']!=frame+1 or row['input_frame']!=frame or bytes.fromhex(row['ps2_pad_hex'])!=expected:
            raise ValueError(f'Native/reference input mismatch at frame {frame}')
    accepted=spec.get('native',{}).get('accepted_input')
    runtime_decoding=bool(accepted and accepted.get('decoding')=='runtime_control_state')
    source='original_raw_commands' if runtime_decoding else 'original_accepted_commands' if accepted else 'requested_pad_timeline'
    report=dict(frames_checked=spec['frames'],requested_pad_bytes_identical=True,input_source=source)
    if not accepted:
        report['note']='Requested movie pad bytes match. Emulator delivery timing and consumed gameplay commands are separate evidence.'
        return report
    if accepted['frames']!=spec['frames'] or not accepted['baseline_prefix']['verified']:
        raise ValueError('Accepted stream frame count or original baseline prefix invalid')
    if runtime_decoding and telemetry.get('input_source')!='original_raw_commands':
        raise ValueError('Telemetry was not produced with native-state command decoding')
    if runtime_decoding and records[0].get('control_state')!=accepted['initial_control_state']:
        raise ValueError('Raw replay initial controller differs from original baseline')
    float_keys=('turn','crouch','brake','boardPress','boardPivot','prewindTurn','spin','flip','airAdjustLR','airAdjustFB')
    bool_keys=('jumpHeld','jumpPressed','boostHeld','boostPressed','handplant','ollieHeld','recoverPressed','pausePressed')
    digest=hashlib.sha256();cursor=0
    for event in accepted['segments']:
        if event['start']!=cursor or not cursor<event['end']<=spec['frames']:
            raise ValueError('Accepted stream must cover each frame exactly once')
        expected=None if runtime_decoding else {**dict.fromkeys(float_keys,0.0),**dict.fromkeys(bool_keys,False),'grabMask':0,**event['controls']}
        words=[int(event[k],0) for k in ('word0','word1')]
        if words[0]&0xfff:raise ValueError('Accepted duration bits must be removed')
        for frame in range(event['start'],event['end']):
            actual=records[frame+1].get('accepted_command')
            if not actual or [int(actual[k],0) for k in ('word0','word1')]!=words or (not runtime_decoding and actual['control_state']!=event['control_state']):
                raise ValueError(f'Accepted original command mismatch at frame {frame}')
            if runtime_decoding:
                if actual['control_state']!=records[frame].get('control_state'):raise ValueError(f'Raw command decoded using a different native controller at frame {frame}')
                digest.update(struct.pack('<II',*words))
                continue
            if set(actual['controls'])!=set(expected):raise ValueError('Accepted control schema mismatch')
            for key,value in expected.items():
                got=actual['controls'][key]
                equal=(struct.pack('<f',value)==struct.pack('<f',got)) if key in float_keys else got==value
                if not equal:raise ValueError(f'Accepted control mismatch at frame {frame}: {key}')
            digest.update(struct.pack('<II',*[int(actual[k],0) for k in ('word0','word1')]))
        cursor=event['end']
    if cursor!=spec['frames'] or digest.hexdigest()!=accepted['input_stream_sha256']:
        raise ValueError('Native accepted command stream hash mismatch')
    if runtime_decoding:
        report.update(accepted_commands_identical=True,decoded_with_native_state=True,input_stream_sha256=digest.hexdigest(),
                      reference_final_control_state=accepted['final_control_state'],native_final_control_state=records[-1].get('control_state'),
                      note='Original raw words and native dispatch states verified. Original per-frame semantic controls are not inferred; native state/physics may diverge and must be compared separately.')
        return report
    report.update(accepted_commands_identical=True,accepted_controls_identical=True,input_stream_sha256=digest.hexdigest(),
                  note='Native controls and expanded command hash match the game-accepted RLE. Requested pad timing is retained separately; no physics equivalence is implied.')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    run=sub.add_parser('run');run.add_argument('scenario',type=Path);run.add_argument('output_prefix',type=Path)
    run.add_argument('--image',type=Path)
    run.add_argument('--binary',type=Path,default=ROOT/'build/metal-engine/ssx3_scene_audit')
    verify=sub.add_parser('verify-inputs');verify.add_argument('scenario',type=Path);verify.add_argument('telemetry',type=Path)
    args=parser.parse_args()
    spec=json.loads(args.scenario.read_text())
    if args.command=='run':
        output=Path(str(args.output_prefix)+'.json')
        if output.resolve()==args.scenario.resolve():raise ValueError('Telemetry must not overwrite the scenario')
        if args.image and args.image.resolve()==args.scenario.resolve():raise ValueError('Image must not overwrite the scenario')
        command=[str(args.binary),'--replay',str(args.scenario),'--telemetry',str(args.output_prefix)]
        if args.image:
            args.image.parent.mkdir(parents=True,exist_ok=True)
            command+=['--image',str(args.image)]
        subprocess.run(command,check=True)
    else:output=args.telemetry
    print(json.dumps(verify_inputs(spec,json.loads(output.read_text())),indent=2))


if __name__=='__main__':main()
