#!/usr/bin/env python3
"""Verify native coarse grids against original PCSX2 collision-cache contents."""
import hashlib,json,struct,subprocess,sys,zipfile
from pathlib import Path
from reference_probes import riders
def main():
    root=Path(__file__).resolve().parents[1];folder=root/'local/reference/terrain'
    patches={p['resource_id']:p for p in json.loads((root/'local/assets/native/ARA1/terrain.json').read_text())['patches']}
    states=[Path(s) for s in sys.argv[1:]] or sorted((root/'local/reference/pcsx2').glob('snow-jam-*.p2s'))
    records={};checks=[]
    for state in states:
     with zipfile.ZipFile(state) as z:memory=z.read('eeMemory.bin')
     human=next((r for r in riders(memory) if r['kind']=='human'),None)
     if not human:continue
     def u(a):
      if not 0<=a<=len(memory)-4:raise ValueError('Original cache pointer outside EE RAM')
      return struct.unpack_from('<I',memory,a)[0]
     base=int(human['address'],16);world=u(base+0x860)
     if not world:continue
     manager=u(u(world)+0xa4);buckets=u(manager+8);count=u(manager+12)
     if not 1<=count<=100000:raise ValueError('Invalid original terrain hash-table size')
     for i in range(u(world+0x10c)):
      live=u(world+0x110+4*i);resource=u(live+0x150)
      if resource not in patches:continue
      patch=patches[resource];flags=struct.unpack_from('<H',memory,live+10)[0]
      if flags&~0x40!=patch['authored_flags']&~0x40:raise ValueError('Original authored patch flags differ')
      checks.append(dict(state=str(state),resource=resource,live_flags=flags,authored_flags=patch['authored_flags']))
      entry=u(buckets+(resource%count)*4);seen=set()
      while entry and u(entry)!=resource:
       if entry in seen:raise ValueError('Cycle in original terrain hash bucket')
       seen.add(entry);entry=u(entry+12)
      if not entry:continue
      raw=memory[entry+16:entry+1616]
      if len(raw)!=1600:raise ValueError('Truncated original grid')
      records[resource,hashlib.sha256(raw).hexdigest()]=(patch,raw)
    fixture=folder/'coarse-grid-fixture.txt'
    with fixture.open('w') as f:
     print(len(records),file=f)
     for (resource,_),(p,raw) in records.items():
      print(resource,*(v for x,y,z in p['coefficients'] for v in [x*100,-z*100,y*100]),file=f)
      print(*(v for i in range(100) for v in struct.unpack_from('<3f',raw,i*16)),file=f)
    binary=folder/'terrain_coarse_test'
    subprocess.run(['clang++','-std=c++20','-O2','-ffp-contract=off',str(root/'tests/terrain_coarse_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(fixture)],check=True)
    (folder/'coarse-cache-audit.json').write_text(json.dumps(dict(grid_count=len(records),coordinate_count=len(records)*300,authored_flag_checks=checks),indent=2)+'\n')


if __name__=='__main__':main()
