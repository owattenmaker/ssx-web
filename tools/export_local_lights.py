#!/usr/bin/env python3
"""Export authored kind-6 light records and audit captured selections against them.

Usage: python3 tools/export_local_lights.py [--location X] [--output FILE] [--audit FILE]

Record range. ARA1 (default, unchanged) reads the course chunk range only (190 records);
its event light octree (tools/export_light_tree.py, RAM) indexes 188 of them, all but
the per-location pair rid 0/1 (kind 3 and kind 0 at the origin, resources 8/264). The
ARA1 connectors A_ARA1/ARA1_B hold only that same pair, so the course range and the
event residency (world_assets.event_locations) index the same lights on Snow Jam.
Other locations use the event residency, like tools/export_light_glow.py and
tools/import_rails.py: their connectors author real spot lights (B_BRA2 8, B_BHP1 13
kind-2 records) that sit in the resident event octree, and export_light_tree.py needs
every indexed light to resolve to a unique authored record.

Audit: ARA1 checks the snow-jam glide/jump-31/jump-90 savestates. Other locations
audit only locations.state(X, 'glide') when that savestate exists (same actor layout
assumed), otherwise the audit is skipped. For X != ARA1 the published browser copy
web/public/assets/X/local-lights.json (the web/prepare-rider-lighting.py format) is
written too.
"""
import argparse
import hashlib
import json
import math
import struct
import zipfile
from pathlib import Path
from world_assets import locations,records,world_chunks,event_locations
from import_sky import chunk_range
ROOT=Path(__file__).resolve().parents[1]

def export(location='ARA1',output=None,audit_path=None):
    source=ROOT/'local/assets/source/ps2';locs=locations(source/'bam.sdb')
    if location=='ARA1':
        _,begin,end=chunk_range(locs,'ARA1');ranges=[(begin,end)];resident=None
    else:
        resident=event_locations(locs,location);ranges=[(b,e) for _,_,b,e in resident]
    lights=[];lookup={}
    for chunk,raw in enumerate(world_chunks(source/'bam.ssb')):
        if chunk>max(e for _,e in ranges):break
        if not any(b<=chunk<=e for b,e in ranges):continue
        for kind,track,rid,data in records(raw):
            if kind!=6:continue
            if len(data)!=112:raise ValueError('Unexpected authored light extent')
            f=lambda at,n=1:list(struct.unpack_from('<'+'f'*n,data,at))
            row=dict(resource=(rid<<8)|track,track=track,rid=rid,chunk=chunk,kind=struct.unpack_from('<I',data,16)[0],
                     intensity=f(20)[0],brightness=f(24)[0],radius=f(28)[0],color=f(32,3),axis=f(44,3),position=f(56,3),
                     bounds_min=f(68,3),bounds_max=f(80,3),inner_cosine=f(92)[0],outer_cosine=f(96)[0],
                     distance_mode=struct.unpack_from('<b',data,100)[0],angular_mode=struct.unpack_from('<b',data,101)[0],
                     source_body_sha256=hashlib.sha256(data[16:]).hexdigest())
            if row['kind'] not in (0,1,2,3) or not all(math.isfinite(x)for x in f(20,20)):raise ValueError('Unsupported light parameters')
            key=data[16:]
            lookup.setdefault(key,[]).append(row['resource']);lights.append(row)
    from locations import state
    if location=='ARA1':snapshots=[(name,ROOT/f'local/reference/pcsx2/snow-jam-{name}.p2s') for name in ['glide','jump-31','jump-90']]
    else:snapshots=[('glide',state(location,'glide'))] if state(location,'glide').exists() else []
    audits=[]
    for name,snapshot in snapshots:
        with zipfile.ZipFile(snapshot) as archive:memory=archive.read('eeMemory.bin')
        u=lambda at:struct.unpack_from('<I',memory,at)[0]
        from locations import human_rider
        actor=0x14701a0 if location=='ARA1' else human_rider(memory);scope=u(actor+0x860);geometry=u(actor+0x780)
        head=struct.unpack_from('<3f',memory,u(geometry+0x2c)+u(actor+0x89c)*32)
        def resource(pointer):
            matches=lookup.get(memory[pointer+16:pointer+112],[])
            if len(matches)!=1:raise ValueError('Captured light does not uniquely match authored bytes')
            return matches[0]
        count=u(scope+0x210)
        if count>4096:raise ValueError('Unexpected query-scope node count')
        candidates=[]
        for i in range(count):
            pointer=u(scope+0x214+i*4)
            if u(pointer+8)==6:candidates.append(resource(pointer))
        selected=[resource(u(actor+0x794+i*4)) for i in range(8) if u(actor+0x794+i*4)]
        audits.append(dict(snapshot=name,ee_sha256=hashlib.sha256(memory).hexdigest(),head_cm=head,candidates=candidates,selected=selected))
    from locations import native_dir,web_dir
    output=Path(output) if output else native_dir(location)/'local-lights.json'
    document=dict(version=1,source_sha256=hashlib.sha256((source/'bam.ssb').read_bytes()).hexdigest(),lights=lights,
                  scope='Authored records only; runtime spatial query-scope generation remains separate')
    if resident is not None:document['event_locations']=[dict(name=n,track=t,chunks=[b,e]) for t,n,b,e in resident]
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(document,indent=2)+'\n')
    if location!='ARA1' and audit_path is None:
        web=web_dir(location);web.mkdir(parents=True,exist_ok=True)
        (web/'local-lights.json').write_text(json.dumps(dict(version=1,source_sha256=document['source_sha256'],lights=lights),separators=(',',':'))+'\n')
    if snapshots:
        audit=Path(audit_path) if audit_path else ROOT/('local/rider-lighting/light-selection-checkpoints.json' if location=='ARA1' else f'local/rider-lighting/{location}/light-selection-checkpoints.json')
        audit.parent.mkdir(parents=True,exist_ok=True);audit.write_text(json.dumps(audits,indent=2)+'\n')
    else:print(f'No {location} savestate ({state(location,"glide").name}); selection audit skipped')
    print(json.dumps(dict(lights=len(lights),kinds={k:sum(l['kind']==k for l in lights) for k in range(4)},checkpoints=audits),indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--location',default='ARA1');parser.add_argument('--output',type=Path,help='local-lights.json override (skips the browser copy)')
    parser.add_argument('--audit',type=Path,help='light-selection-checkpoints.json override (skips the browser copy)')
    args=parser.parse_args();export(args.location,args.output,args.audit if args.audit or not args.output else args.output.with_name('light-selection-checkpoints.json'))
