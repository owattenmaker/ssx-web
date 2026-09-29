#!/usr/bin/env python3
"""Locate original SSX3 rider objects in a PCSX2 state and export verified fields.

Object signature: cRider constructor at PS2 0x11B698.
Position/quaternion: updateOrientationImplicit at 0x11E098.
"""
import argparse
import hashlib
import json
import math
import struct
import zipfile
from pathlib import Path
from reference_ground_profile import extract_ground
from reference_air_control import extract_air_control
from reference_air_entry import extract_air_entry
from reference_landing import extract_landing
from reference_boost import extract_boost


def riders(memory):
    # cRider's shared interface survives the human/computer derived constructors;
    # the interfaces at+6C0/+6E8 are replaced by those derived classes.
    signature=struct.pack('<I',0x00459b90)
    result=[];at=0
    while True:
        at=memory.find(signature,at)
        if at<0:break
        base=at-0x6d0;at+=4
        if base<0 or base%16 or base+0xb40>len(memory):continue
        interface=struct.unpack_from('<I',memory,base+0x6c0)[0]
        companion=struct.unpack_from('<I',memory,base+0x6e8)[0]
        kind={ (0x4583a8,0x458360):'human', (0x458660,0x458618):'computer',
               (0x459c18,0x459bd0):'base' }.get((interface,companion))
        if not kind:continue
        position=struct.unpack_from('<4f',memory,base+0x110)
        orientation=struct.unpack_from('<4f',memory,base+0x120)
        velocity=struct.unpack_from('<4f',memory,base+0x1e0)
        motion_owner=struct.unpack_from('<I',memory,base+0x77c)[0]
        motion_mode=(struct.unpack_from('<I',memory,motion_owner+0xde0)[0]
                     if 0<motion_owner<len(memory)-0xde4 else None)
        if not all(math.isfinite(x) for x in (*position,*orientation)):continue
        def scalar(offset):return struct.unpack_from('<f',memory,base+offset)[0]
        def vector(offset):return struct.unpack_from('<3f',memory,base+offset)
        def pointer(at):
            if not 0<=at<=len(memory)-4:raise ValueError('Original pointer outside EE memory')
            return struct.unpack_from('<I',memory,at)[0]
        global_object=pointer(0x4a30f0-0x848)
        race_object=pointer(global_object+0x84)
        tick_object=pointer(race_object+0x0c)
        tick=pointer(tick_object+8)
        ground_focus_tick=pointer(motion_owner+0x10)
        normal=vector(0x370)
        surface_velocity=vector(0x3d0)
        result.append(dict(address=f'0x{base:08x}',kind=kind,source_axis='Z-up',
                           position_cm=position[:3],native_position_m=[position[0]/100,position[2]/100,-position[1]/100],
                           source_quaternion=orientation,native_quaternion=[orientation[0],orientation[2],-orientation[1],orientation[3]],
                           raw_130=struct.unpack_from('<3f',memory,base+0x130),
                           raw_140=struct.unpack_from('<4f',memory,base+0x140),
                           integration_vector_1e0=velocity[:3],
                           native_velocity_mps=[velocity[0]/100,velocity[2]/100,-velocity[1]/100],
                           speed_mps=math.sqrt(sum(v*v for v in velocity[:3]))/100,
                           motion_owner=f'0x{motion_owner:08x}',motion_mode=motion_mode,control_state=pointer(motion_owner+0xde4),
                           motion_label={0:'cruise',1:'airborne',2:'wipeout',3:'start'}.get(motion_mode,'unclassified'),
                           source_contact_normal=normal,native_contact_normal=[normal[0],normal[2],-normal[1]],
                           source_forward_tangent=vector(0x3a0),source_lateral_tangent=vector(0x3b0),
                           source_surface_velocity=surface_velocity,
                           source_contact_distance_cm=scalar(0x454),source_contact_compression=scalar(0x758),
                           source_speed_limit_cmps=scalar(0x2e4),frame_time_multiplier=scalar(0x300),
                           surface_index=struct.unpack_from('<i',memory,base+0x438)[0],
                           controls=dict(turn_1f0=scalar(0x1f0),brake_214=scalar(0x214),brake_target_21c=scalar(0x21c),auxiliary_220=scalar(0x220)),
                           original_jump=dict(position=position[:3],velocity=velocity[:3],normal=normal,
                              forward=vector(0x3a0),takeoffNormal=vector(0x380),boardUp=vector(0x1c0),
                              speedLimit=scalar(0x2e4),charge=scalar(0x220),ticksSinceGroundFocus=(tick-ground_focus_tick)&0xffffffff,
                              riderState=pointer(base+0x434),flags=struct.unpack_from('<H',memory,base+0x2d4)[0],
                              motionMode=motion_mode,previousHeld=scalar(0x228)>0),
                           source_tick=tick,source_ground_focus_tick=ground_focus_tick,
                           original_ground=extract_ground(memory,base) if kind=='human' and motion_mode==0 else None,
                           original_boost=extract_boost(memory,base) if kind=='human' else None,
                           original_landing=extract_landing(memory,base) if kind=='human' else None,
                           original_air_entry=extract_air_entry(memory,base) if kind=='human' else None,
                           original_air_control=extract_air_control(memory,base) if kind=='human' and pointer(motion_owner+0xde4)==5 else None,
                           rider_slot=struct.unpack_from('<i',memory,base+0x86c)[0],
                           note='Position/quaternion and derived interface signatures verified. +1E0 centimeters/second confirmed by consecutive Snow Jam glide states: old velocity/60 predicts displacement within 0.014 cm per component; contact corrections and float quantization remain.'))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('state',type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    with zipfile.ZipFile(args.state) as archive:memory=archive.read('eeMemory.bin')
    report=dict(state=str(args.state.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),riders=riders(memory))
    text=json.dumps(report,indent=2)+'\n'
    if args.output:args.output.write_text(text)
    print(text)


if __name__=='__main__':main()
