#!/usr/bin/env python3
"""Preserve original SSX3 bicubic collision surfaces and audit PCSX2 contacts.

Source query: PS2 0x1242B0; patch RID at rider+430, UV at +AAC/+AB0,
contact point at +460. Analytic evaluation is independent of render tessellation.
"""
import argparse
import hashlib
import json
import math
import struct
import zipfile
from pathlib import Path
from world_assets import locations, records, world_chunks
from reference_probes import riders


def patch_record(track, rid, data):
    if len(data) != 432 or not 0 <= track <= 255 or not 0 <= rid < 1 << 24:
        raise ValueError('Invalid PS2 patch record')
    source = [struct.unpack_from('<3f', data, 64 + i * 16) for i in reversed(range(16))]
    if not all(math.isfinite(x) for p in source for x in p):
        raise ValueError('Nonfinite patch coefficient')
    lo=struct.unpack_from('<3f',data,344);hi=struct.unpack_from('<3f',data,356)
    return dict(track=track, rid=rid, resource_id=(rid << 8) | track,
                authored_surface_id=struct.unpack_from('<h',data,8)[0],
                authored_flags=struct.unpack_from('<H',data,10)[0],
                authored_bounds_min=[lo[0]/100,lo[2]/100,-hi[1]/100],
                authored_bounds_max=[hi[0]/100,hi[2]/100,-lo[1]/100],
                coefficients=[[x/100, z/100, -y/100] for x,y,z in source])


def evaluate(coefficients, u, v):
    def value(du,dv):
        return [sum(coefficients[j*4+i][k] * (i if du else 1) * (j if dv else 1)
                    * u**(i-du) * v**(j-dv) for j in range(dv,4) for i in range(du,4)) for k in range(3)]
    p,a,b=value(0,0),value(1,0),value(0,1)
    normal=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    length=math.sqrt(sum(x*x for x in normal))
    return p,[x/length for x in normal] if length else [0,0,0]


def extract(source, location):
    locs=locations(source/'bam.sdb');index=next(i for i,x in enumerate(locs) if x['name']==location)
    begin=locs[index-1]['chunk_end']+1 if index else 0;end=locs[index]['chunk_end']
    result=[]
    for i,chunk in enumerate(world_chunks(source/'bam.ssb')):
        if i>end:break
        if i<begin:continue
        result.extend(patch_record(t,r,d) for k,t,r,d in records(chunk) if k==1)
    return dict(version=1, location=location, units='meters', up_axis='Y',
                basis='source (x,y,z) -> native (x,z,-y)', coefficient_order='u power + 4 * v power',
                source_sha256=hashlib.sha256((source/'bam.ssb').read_bytes()).hexdigest(), patches=result)


def audit(package, states):
    patches={x['resource_id']:x for x in package['patches']};results=[]
    for state in states:
        with zipfile.ZipFile(state) as archive:memory=archive.read('eeMemory.bin')
        rider=next(r for r in riders(memory) if r['kind']=='human');base=int(rider['address'],16)
        rid=struct.unpack_from('<I',memory,base+0x430)[0]
        if rid not in patches:
            results.append(dict(state=str(state),resource_id=rid,skipped='No retained terrain patch'));continue
        uv=struct.unpack_from('<2f',memory,base+0xaac)
        point,normal=evaluate(patches[rid]['coefficients'],*uv)
        reference=rider['native_contact_normal']
        if sum(a*b for a,b in zip(normal,reference))<0:normal=[-x for x in normal]
        x,y,z=struct.unpack_from('<3f',memory,base+0x460);reference_point=[x/100,z/100,-y/100]
        results.append(dict(state=str(state),ee_sha256=hashlib.sha256(memory).hexdigest(),resource_id=rid,
                            track=rid&255,rid=rid>>8,uv=uv,point=point,normal=normal,
                            reference_point=reference_point,reference_normal=reference,
                            point_error_m=math.dist(point,reference_point),normal_error=math.dist(normal,reference),
                            signed_distance_m=sum((a-b)*n for a,b,n in zip(rider['native_position_m'],point,normal)),
                            reference_distance_m=rider['source_contact_distance_cm']/100))
    return dict(note='Saved original UV audit verifies representation; it does not prove native root solving or complete contact response.',cases=results)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path('local/assets/source/ps2'))
    parser.add_argument('--location',default='ARA1');parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--states',nargs='*',type=Path);parser.add_argument('--package',type=Path)
    args=parser.parse_args();package=json.loads(args.package.read_text()) if args.package else extract(args.source,args.location)
    result=audit(package,args.states) if args.states else package
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Wrote {args.output}')


if __name__=='__main__':main()
