#!/usr/bin/env python3
"""Read the original rider's nearby rail layers from saved snapshots; no emulator writes."""
from pathlib import Path
import collections
import hashlib
import json
import struct
import zipfile

root=Path(__file__).resolve().parents[1]
rows=[]
for path in sorted((root/'local/reference/pcsx2').glob('snow-jam-*.p2s')):
    with zipfile.ZipFile(path) as archive:
        ram=archive.read('eeMemory.bin')
    def u(address):
        if not 0 <= address <= len(ram)-4:
            raise ValueError(f'Invalid RAM address {address:#x} in {path.name}')
        return struct.unpack_from('<I',ram,address)[0]
    rider=0x14701a0
    # This census is restricted to the reference human allocation, not menus
    # or states where the level has not constructed that rider yet.
    if u(rider+0x6c0)!=0x4583a8:
        continue
    cache=u(rider+0x860)
    count=u(cache+0x210)
    if count>4096:
        raise ValueError(f'Invalid layer count {count} in {path.name}')
    kinds=collections.Counter()
    objects=[]
    for index in range(count):
        layer=u(cache+0x214+index*4)
        kind=u(layer+8)
        kinds[kind]+=1
        if kind in (2,3):
            obj=u(layer+12)
            record={'layer':layer,'kind':kind,'object':obj}
            if kind==2:
                record.update(packedRailId=u(obj+0x30),instance=u(obj+0x40),binding=u(obj+0x34))
            else:
                record.update(flags=u(obj+0x24),curvePointCount=u(obj+0x20),coefficients=u(obj+0x50))
            objects.append(record)
    rows.append(dict(snapshot=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                     rider=rider,cache=cache,positionCm=struct.unpack_from('<3f',ram,rider+0x110),
                     layerCounts=dict(kinds),objectRails=objects))
report=dict(scope='Saved human-rider nearby caches only; not full-course availability or event eligibility. Snapshots may share paths or include isolated probes.',
            snapshots=len(rows),snapshotsWithObjectRails=sum(bool(r['objectRails']) for r in rows),rows=rows)
output=root/'local/browser-validation/original-rail-layers.json'
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k!='rows'})
