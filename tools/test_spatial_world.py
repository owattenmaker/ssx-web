#!/usr/bin/env python3
"""Validate native bounds-derived terrain ordering against original octree lists."""
import json,struct,subprocess,zipfile
from pathlib import Path

def main():
    root=Path(__file__).resolve().parents[1];package=json.loads((root/'local/assets/native/ARA1/terrain.json').read_text())
    patches={p['resource_id']:(i,p) for i,p in enumerate(package['patches'])}
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:m=z.read('eeMemory.bin')
    def u(a):return struct.unpack_from('<I',m,a)[0]
    scene=u(u(0x14701a0+0x860));records=[];visited=set()
    def walk(node,cell):
        if not node:return
        if node in visited:raise ValueError('Repeated original octree node')
        visited.add(node);p=u(node+0x24);seen=set()
        while p:
            if p in seen:raise ValueError('Cycle in original terrain list')
            seen.add(p);rid=u(p+0x150)
            if rid in patches:
                index,source=patches[rid];lo=source['authored_bounds_min'];hi=source['authored_bounds_max']
                bounds=[lo[0]*100,-hi[2]*100,lo[1]*100,hi[0]*100,-lo[2]*100,hi[1]*100]
                records.append([rid,index,*bounds,*cell])
            p=u(p)
        for child in range(8):walk(u(node+child*4),[cell[0]-1,*[cell[k+1]*2+((child>>(2-k))&1) for k in range(3)]])
    for slot in range(8):
        at=scene+slot*20;node=u(at+16)
        if node:walk(node,list(struct.unpack_from('<4i',m,at)))
    path=root/'local/reference/terrain/spatial-world-fixture.txt';path.write_text(str(len(records))+'\n'+'\n'.join(' '.join(map(str,r)) for r in records)+'\n')
    binary=root/'build/ssx3_spatial_world';subprocess.run(['clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off',str(root/'tests/spatial_world.cpp'),'-o',str(binary)],check=True);subprocess.run([str(binary),str(path)],check=True)

if __name__=='__main__':main()
