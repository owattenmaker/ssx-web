#!/usr/bin/env python3
"""Extract typed original board-trail context; captured geometry is audit-only."""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path

def extract_memory(m,rider=0x14701a0,*,audit_pose=False):
    def u(a):return struct.unpack_from('<I',m,a)[0]
    def f(a,n=1):
        v=list(struct.unpack_from('<'+'f'*n,m,a));return v[0] if n==1 else v
    def i(a):return struct.unpack_from('<i',m,a)[0]
    geometry=u(rider+0x780);animation=u(rider+0x784);motion=u(rider+0x77c)
    vtable=u(rider+0x6c0);adjust=struct.unpack_from('<h',m,vtable+0x38)[0]
    if u(vtable+0x3c)!=0x140b80:raise ValueError('Unrecovered environment-index accessor')
    environment_index=u(rider+0x6c0+adjust+0x86c)
    if 0x4fa398+environment_index*0xf0+16>len(m):raise ValueError('Environment colour address outside original memory')
    main=u(u(animation+0x50)+2*8+4);flags=struct.unpack_from('<Q',m,main+0xb0)[0] if main else 0
    bone_fields={'board':0x8a4,'bone8B0':0x8b0,'bone8B8':0x8b8,'bone918':0x918,'bone8E8':0x8e8}
    result=dict(rider=rider,visual_rng_word=u(0x4a30f0+0xa0c),environment_index=environment_index,
        environment_argb=f(0x4fa398+environment_index*0xf0,4),
        bone_indices={name:u(rider+offset)for name,offset in bone_fields.items()},
        initial_context=dict(motion=u(motion+0xde0),crash_submode=u(motion+0x30),surface=i(rider+0x438),semantic=u(animation+8),
            marker0=bool(flags&1),marker1=bool(flags&2),flagAC4=bool(u(rider+0xac4)),flagAD0=bool(u(rider+0xad0)),flagAFC=bool(u(rider+0xafc)),flagB00=bool(u(rider+0xb00)),
            manual=bool(u(rider+0x330)),reverse_stance=bool(u(rider+0x320)),detached=bool(u(rider+0x150)),contact_distance_cm=f(rider+0x454)),
        provenance=dict(update='0x2E8938',environment_accessor='0x140B80',environment_argb_address=hex(0x4fa398+environment_index*0xf0),
            visual_rng_address='0x4A3AFC',bone_matrix='geometry+0x34; source bone index *64',pose_policy='Runtime must use live native generated matrices/contact/velocity, not these captured audit values.'))
    if audit_pose:
        matrices=u(geometry+0x34)
        result['audit_pose']={name:dict(axis0=f(matrices+u(rider+off)*64,3),axis2=f(matrices+u(rider+off)*64+32,3),position=f(matrices+u(rider+off)*64+48,3))for name,off in bone_fields.items()}
        result['audit_pose'].update(contact=f(rider+0x460,3),normal=f(rider+0x370,3),velocity=f(rider+0x1e0,3))
    return result

def extract(path,rider=0x14701a0,*,audit_pose=False):
    with zipfile.ZipFile(path)as archive:m=archive.read('eeMemory.bin')
    result=extract_memory(m,rider,audit_pose=audit_pose);result['provenance'].update(snapshot=str(path),ee_sha256=hashlib.sha256(m).hexdigest());return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=Path);p.add_argument('--rider',type=lambda s:int(s,0),default=0x14701a0);p.add_argument('--output',type=Path,required=True);p.add_argument('--audit-pose',action='store_true');a=p.parse_args();result=extract(a.snapshot,a.rider,audit_pose=a.audit_pose);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(f'Extracted original trail context for rider{a.rider:#x}; environment{result["environment_index"]}, motion{result["initial_context"]["motion"]}')
if __name__=='__main__':main()
