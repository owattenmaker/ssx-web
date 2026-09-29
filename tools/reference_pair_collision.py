#!/usr/bin/env python3
"""Read original shared rider pair state for native seeds and conformance fixtures."""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path


def extract_pair_collision(memory):
    def read(fmt,at):return struct.unpack_from('<'+fmt,memory,at)
    def u(at):return read('I',at)[0]
    def i(at):return read('i',at)[0]
    def f(at):return read('f',at)[0]
    def byte(at):return read('b',at)[0]
    def vec(at):return list(read('3f',at))
    manager=u(u(u(0x4a30f0-0x848)+0x84)+0xc)
    count=i(manager+0x78)
    if not 0<=count<=6:raise ValueError('Original pair manager count exceeds six slots')
    participants=[]
    for slot in range(count):
        base=u(manager+0x28+slot*4)
        if not base:raise ValueError('Missing original pair participant')
        if i(base+0x86c)!=slot:raise ValueError('Original pair participant slot mismatch')
        owner=u(base+0x77c);body=u(base+0xaa0);entry=i(0x5305b0+slot*4);record=0x535b20+entry*28
        character=byte(record+17);bank=2 if i(record+12)==-1 else u(record+16)&1;override=i(base+0xb34)
        numerator=override if override>0 else int(byte(0x535538+bank*70+character*7+5)/5)
        denominator=byte(0x5308d8+character*15+13)
        if denominator<=0:raise ValueError('Invalid original collision stat denominator')
        resolved=.5 if u(0x534fe0+entry*28+16)&4 else struct.unpack('<f',struct.pack('<f',numerator/denominator))[0]
        sphere_count=u(body+0x2c)
        if sphere_count>20:raise ValueError('Original collision body sphere count exceeds twenty')
        records=[]
        for other in range(6):
            p=base+other*36
            records.append(dict(slot=other,enabled=bool(u(p)),interface_value=hex(u(p+4)),planar_distance_cm=f(p+8),bearing=f(p+12),
                                last_contact_tick=i(p+16),last_checked_tick=i(p+20),last_attack_tick=i(p+24),raw1c=u(p+28),raw20=u(p+32)))
        participants.append(dict(slot=slot,rider=hex(base),kind880=i(base+0x880),disabled=bool(i(base+0x878)),
            character_id=character,weight_attribute=i(0x530970+character*0x88+0x40),resolved_collision_stat=resolved,resolved_attack_stat=resolved,boost=f(base+0x2fc),
            facing340=vec(base+0x340),strength350=f(base+0x350),
            motion_mode=i(owner+0xde0),control_state=i(owner+0xde4),ragdoll_submode=i(owner+0x30),
            position_cm=vec(base+0x110),velocity_cmps=vec(base+0x1e0),ground_normal=vec(base+0x370),physical_up=vec(base+0x1c0),
            presentation=dict(right=vec(base+0x160),forward=vec(base+0x170),up=vec(base+0x180),origin=vec(base+0x190)),
            body=dict(broad_center_cm=vec(body+16),broad_radius_cm=f(body+32),active_mask=u(body+40),
                      spheres=[dict(center_cm=vec(body+48+n*32),radius_cm=f(body+64+n*32),bone=u(body+68+n*32)) for n in range(sphere_count)]),
            records=records))
    return dict(tick=i(manager+8),manager=hex(manager),active_count=count,excluded_tail_count=i(manager+0x84),
                knockdown_cheat=bool(u(0x5308d0)&32),random_state=list(read('6I',0x4ff030)),participants=participants,
                provenance=dict(pair_dispatch='0x107888',impulse='0x107E70',body_overlap='0x329F98',weight='0x11FF98/0x148F50/0x14EF30',
                                proximity='0x10F560',source_axis='Z-up cm',captured_body_geometry='Initial fixture/seed only; runtime requires native generated poses'))


def main():
    parser=argparse.ArgumentParser();parser.add_argument('snapshot',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    with zipfile.ZipFile(args.snapshot) as z:memory=z.read('eeMemory.bin')
    result=dict(snapshot=str(args.snapshot.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),original_pair_collision=extract_pair_collision(memory))
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    pairs=result['original_pair_collision'];print(f"Extracted {len(pairs['participants'])} original pair participants at tick {pairs['tick']}")
if __name__=='__main__':main()
