#!/usr/bin/env python3
"""Export typed initial hard-crash states; runtime poses must be native generated."""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path


def extract_crash(memory,rider):
    def read(fmt,p):return struct.unpack_from('<'+fmt,memory,p)
    def u(p):return read('I',p)[0]
    def i(p):return read('i',p)[0]
    def f(p):return read('f',p)[0]
    def vec(p):return list(read('3f',p))
    def quaternion(p):return list(read('4f',p))
    def pose(p):return dict(position=vec(p),quaternion=quaternion(p+16))
    owner=u(rider+0x77c);motion=owner+0x30;control=owner+0x2c0
    return dict(motion_mode=i(owner+0xde0),control_state=i(owner+0xde4),
        actor=dict(position=vec(rider+0x110),velocity=vec(rider+0x1e0),quaternion=quaternion(rider+0x120),
                   ground_normal=vec(rider+0x370),surface_velocity=vec(rider+0x3d0),contact_point=vec(rider+0x460),
                   contact_distance=f(rider+0x454),surface=i(rider+0x438),detached=bool(i(rider+0x150)),
                   detached_position=vec(rider+0x130),detached_quaternion=quaternion(rider+0x140),time_scale=f(rider+0x300),
                   spin_rate=f(rider+0x2dc),flip_rate=f(rider+0x2e0),offset9d0=vec(rider+0x9d0),low400=vec(rider+0x400),high410=vec(rider+0x410)),
        motion=dict(submode=i(motion),flag4=i(motion+4),detached_velocity=vec(motion+16),detached_angular_velocity=vec(motion+32),
                    angular_velocity=vec(motion+48),low=vec(motion+80),high=vec(motion+96)),
        control=dict(phase=i(control),previous_primary=pose(control+16),previous_secondary=pose(control+48),
                     previous_progress=f(control+0x50),impact_pending54=i(control+0x54),impact_velocity60=vec(control+0x60),recovery70=f(control+0x70)),
        inputs=dict(rider_category_b20=i(rider+0xb20),reset_permission470=f(rider+0x470),primary_bone=u(rider+0x89c),secondary_bone=u(rider+0x8a4)),
        provenance=dict(rider=hex(rider),motion=hex(motion),control=hex(control),source_axis='Z-up centimeters',
                        use='Initial state only; no per-frame captured bone positions are used by native simulation',
                        motion_entry='0x136C40',control_entry='0x12CA30',first_phases=['0x136F30','0x137750','0x137D18'],second_phases=['0x137138','0x137860','0x138640']))


def main():
    parser=argparse.ArgumentParser();parser.add_argument('snapshot',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    with zipfile.ZipFile(args.snapshot) as z:memory=z.read('eeMemory.bin')
    def u(p):return struct.unpack_from('<I',memory,p)[0]
    manager=u(u(u(0x4a30f0-0x848)+0x84)+12);count=u(manager+0x78)
    if count>6:raise ValueError('Invalid original roster count')
    entries=[]
    for slot in range(count):
        rider=u(manager+0x28+slot*4);owner=u(rider+0x77c)
        if u(owner+0xde0)==2 or u(owner+0xde4)==8:entries.append(dict(slot=slot,original_crash=extract_crash(memory,rider)))
    result=dict(snapshot=str(args.snapshot.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),tick=u(manager+8),riders=entries)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n');print(f"Exported {len(entries)} original crash states at tick {result['tick']}")
if __name__=='__main__':main()
