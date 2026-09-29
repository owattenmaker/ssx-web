#!/usr/bin/env python3
"""Read-only world-region and retained-light timing evidence; not runtime assets."""
from pathlib import Path
import hashlib,json,math,struct,zipfile
ROOT=Path(__file__).resolve().parents[1]
snapshots=[]
for name in ['glide','jump-31','jump-90']:
    with zipfile.ZipFile(ROOT/f'local/reference/pcsx2/snow-jam-{name}.p2s') as archive:memory=archive.read('eeMemory.bin')
    def read(fmt,at):
        if at<0 or at+struct.calcsize(fmt)>len(memory):raise ValueError('Scope pointer outside EE memory')
        return struct.unpack_from(fmt,memory,at)
    u=lambda at:read('<I',at)[0]
    rider=0x14701a0;scope=u(rider+0x860);manager=u(scope);regions=[]
    for index in range(8):
        base=manager+index*20;root=u(base+16)
        if not root:continue
        exponent,x,y,z=read('<4i',base)
        if not 0<=exponent<=30:raise ValueError('Unexpected active region exponent')
        regions.append(dict(index=index,exponent=exponent,cell=[x,y,z],root=hex(root)))
    snapshots.append(dict(snapshot=name,ee_sha256=hashlib.sha256(memory).hexdigest(),scope=hex(scope),manager=hex(manager),
                          query_min=list(read('<3f',rider+0x400)),query_max=list(read('<3f',rider+0x410)),
                          regions=regions,candidate_count=u(scope+0x210)))

catalog=json.loads((ROOT/'local/assets/native/ARA1/local-lights.json').read_text())
with zipfile.ZipFile(ROOT/'local/reference/pcsx2/snow-jam-jump-31.p2s') as archive:memory=archive.read('eeMemory.bin')
frames=[json.loads(p.read_text())['cameras'][1] for p in sorted((ROOT/'local/camera-continuous/jump').glob('tick-*.json'))]
if not frames or any(b['tick']!=a['tick']+1 for a,b in zip(frames,frames[1:])):raise ValueError('Continuous light timing trace is missing or has gaps')
timing=[]
for pointer in frames[0]['rider_words'][0x794//4:0x7b4//4]:
    if not pointer:continue
    digest=hashlib.sha256(memory[pointer+16:pointer+112]).hexdigest()
    matches=[row for row in catalog['lights'] if row['source_body_sha256']==digest]
    if len(matches)!=1:raise ValueError('Initial selected light has no unique authored match')
    light=matches[0];outside=None;removed=None;retained=[]
    for frame in frames:
        distance=math.dist(frame['head'][:3],light['position'])
        present=pointer in frame['rider_words'][0x794//4:0x7b4//4]
        if distance>light['radius'] and outside is None:outside=frame['tick']
        if not present and removed is None:removed=frame['tick']
        if present and distance>light['radius']:retained.append(dict(tick=frame['tick'],distance_cm=distance))
    timing.append(dict(resource=light['resource'],radius_cm=light['radius'],first_current_point_outside=outside,
                       first_absent_from_retained_list=removed,retained_outside_frames=retained))
result=dict(regions=snapshots,timing=timing,scope='Captured query-region and light-list timing evidence; no inferred refresh period or runtime spatial tree')
output=ROOT/'local/rider-lighting/query-scope-audit.json';output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(active_region_counts=[len(row['regions'])for row in snapshots],timing=timing),indent=2))
