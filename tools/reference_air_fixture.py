#!/usr/bin/env python3
"""Create a guarded, isolated airborne experiment in a derived SSX3 save state.

Reproduces trajectory reset writes from original PS2 0x113198/0x113618.
No disc image or original save state is modified. Actor must already be airborne.
"""
import argparse
import struct
import zipfile
from pathlib import Path
from reference_probes import riders
from reference_replay import patch_state


def create(source,output,height_m=50,upward_mps=10):
    if not 10<=height_m<=1000 or not 0<upward_mps<=30:
        raise ValueError('Experiment requires height10..1000m and upward velocity0..30m/s')
    with zipfile.ZipFile(source) as archive:memory=archive.read('eeMemory.bin')
    if struct.unpack_from('<I',memory,0x113198)[0]!=0xac8000ac:
        raise ValueError('Unexpected original trajectory reset instruction')
    found=[r for r in riders(memory) if r['kind']=='human']
    if len(found)!=1 or found[0]['motion_mode']!=1:
        raise ValueError('Expected one already-airborne human rider')
    base=int(found[0]['address'],0)
    trajectory=struct.unpack_from('<I',memory,base+0x788)[0]
    if not 0<trajectory<len(memory)-0xb0:raise ValueError('Invalid trajectory pointer')
    position=list(struct.unpack_from('<4f',memory,base+0x110));position[2]+=height_m*100
    velocity=list(struct.unpack_from('<4f',memory,base+0x1e0));velocity[2]=upward_mps*100
    patches=[]
    def write(address,data,reason):
        patches.append(dict(address=hex(address),expected=memory[address:address+len(data)].hex(),
                            replacement=data.hex(),reason=reason))
    for address in (base+0x110,trajectory+0x50,trajectory+0x70):
        write(address,struct.pack('<4f',*position),'Elevate actor and trajectory origins above the course')
    for address in (base+0x1e0,trajectory+0x60,trajectory+0x80):
        write(address,struct.pack('<4f',*velocity),'Set known upward velocity for rising/apex/falling experiment')
    for offset in (0xac,0xa0,0x98,0x9c,0xa4):
        write(trajectory+offset,bytes(4),'Original0x113198 trajectory time/status reset')
    write(trajectory+0x20,memory[0x4ff160:0x4ff170],'Original reset world-up vector')
    write(trajectory+0x10,memory[0x4ff120:0x4ff130],'Original reset zero vector')
    for offset in (0x90,0x30):
        write(trajectory+offset,struct.pack('<I',0xffffffff),'Original reset invalid contact marker')
    write(trajectory+0x34,memory[0x4a5960:0x4a5968],'Original reset sentinel')
    patch_state(source,output,patches)
    return patches


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--height-m',type=float,default=50);parser.add_argument('--upward-mps',type=float,default=10)
    args=parser.parse_args();patches=create(args.source,args.output,args.height_m,args.upward_mps)
    print(f'Created derived experiment with {len(patches)} guarded RAM patches: {args.output}')


if __name__=='__main__':main()
