#!/usr/bin/env python3
"""Recovered SSX3 airborne integration, validated against PCSX2 checkpoints.

Source routine 0x1139A0, called by trajectory updater 0x113648. This offline
conformance tool models the uncapped branch and normal finite float values.
It executes native arithmetic, not guest instructions. Collision, trajectory
interpolation and the speed-cap branch are outside its tested scope.
"""
import argparse
import json
import math
import struct
from pathlib import Path
from compare_reference import read_human


def bits_float(bits):return struct.unpack('<f',struct.pack('<I',bits))[0]


def toward_zero(value):
    if not math.isfinite(value):raise ValueError('Non-finite arithmetic input')
    bits=struct.unpack('<I',struct.pack('<f',value))[0]
    result=bits_float(bits)
    return bits_float(bits-1) if abs(result)>abs(value) else result


# Exact immutable ELF constants, not rounded decimal approximations.
DT=bits_float(0x3c888889)
DRAG=bits_float(0xbb5a740f)
DOWN=bits_float(0xc1fd5556)
UP=bits_float(0xc162aaab)


def step(position,velocity,maximum_speed=3333.33349609375):
    p=[toward_zero(x+toward_zero(v*DT)) for x,v in zip(position,velocity)]
    v=[toward_zero(velocity[0]+toward_zero(velocity[0]*DRAG)),
       toward_zero(velocity[1]+toward_zero(velocity[1]*DRAG)),
       toward_zero(velocity[2]+(UP if velocity[2]>0 else DOWN))]
    if math.sqrt(sum(x*x for x in v))>maximum_speed:
        raise ValueError('Speed-cap branch requires separate conformance; this case exceeds it')
    return p,v


def validate(initial,checkpoints):
    original,digest=read_human(initial)
    if original['motion_mode']!=1:raise ValueError('Initial state must use airborne mode1')
    p=original['position_cm'];v=original['integration_vector_1e0'];frame=0;results=[]
    for target,path in sorted(checkpoints):
        if target<0:raise ValueError('Frame offset must be nonnegative')
        while frame<target:p,v=step(p,v);frame+=1
        expected,state_digest=read_human(path)
        equal=struct.pack('<6f',*p,*v)==struct.pack('<6f',*expected['position_cm'],*expected['integration_vector_1e0'])
        results.append(dict(frames=target,state=str(path.resolve()),ee_sha256=state_digest,
            bit_identical_position_velocity=equal,motion_mode=expected['motion_mode'],
            position_error_cm=[a-b for a,b in zip(p,expected['position_cm'])],
            velocity_error_cmps=[a-b for a,b in zip(v,expected['integration_vector_1e0'])]))
    return dict(initial=str(initial.resolve()),initial_ee_sha256=digest,routine='0x1139A0',
        constants=dict(dt_bits='3c888889',drag_bits='bb5a740f',down_bits='c1fd5556',up_bits='c162aaab'),
        samples=results,all_bit_identical=all(r['bit_identical_position_velocity'] for r in results))


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('initial',type=Path)
    parser.add_argument('--checkpoint',action='append',required=True,help='FRAMES_FROM_INITIAL=STATE_PATH')
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    points=[]
    for item in args.checkpoint:
        frame,path=item.split('=',1);points.append((int(frame),Path(path)))
    report=validate(args.initial,points);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    if not report['all_bit_identical']:raise SystemExit(1)


if __name__=='__main__':main()
