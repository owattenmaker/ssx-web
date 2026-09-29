#!/usr/bin/env python3
"""Validate exported Sam assets and render every available clip with native Metal.

No app window is opened. Requires the already generated private native packages.
"""
import argparse, hashlib, json, math, os, struct, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PACKAGES=['RIDER_SAM','RIDER_SAM_PACKERS','RIDER_SAM_FISHING','RIDER_SAM_FISHING_TUBE']

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def check_package(package):
    folder=ROOT/'local/assets/native'/package
    world=json.loads((folder/'world.json').read_text());rider=json.loads((folder/'rider.json').read_text())
    vertices=(folder/'vertices.bin').read_bytes();indices=(folder/'indices.bin').read_bytes()
    assert len(vertices)==world['vertex_count']*40 and len(indices)==world['index_count']*4
    assert all(math.isfinite(x) for (x,) in struct.iter_unpack('<f',vertices))
    assert all(x<world['vertex_count'] for (x,) in struct.iter_unpack('<I',indices))
    assert len(rider['skin'])==world['vertex_count'] and len(rider['bones'])==26
    for skin in rider['skin']:
        assert 1<=len(skin)<=4 and abs(sum(w for _,w in skin)-1)<1e-4
        assert all(0<=bone<26 and math.isfinite(w) and w>=0 for bone,w in skin)
    glb=ROOT/'local/sam-model'/package/'sam.glb';data=glb.read_bytes()
    magic,version,total=struct.unpack_from('<III',data);size,kind=struct.unpack_from('<I4s',data,12)
    assert magic==0x46546c67 and version==2 and total==len(data) and kind==b'JSON'
    doc=json.loads(data[20:20+size]);assert len(doc['skins'][0]['joints'])==26
    for node in doc['nodes']:
        if 'rotation' in node:assert abs(sum(x*x for x in node['rotation'])-1)<1e-8
    for a in doc['accessors']:
        view=doc['bufferViews'][a['bufferView']]
        assert view['byteOffset']+view['byteLength']<=doc['buffers'][0]['byteLength']
    return dict(package=package,vertices=world['vertex_count'],triangles=world['index_count']//3,
                rig_bones=26,world_sha256=digest(folder/'world.json'),mesh_sha256=digest(folder/'vertices.bin'),glb_sha256=digest(glb))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,default=ROOT/'build/sam-client/ssx3_scene_audit')
    parser.add_argument('--output',type=Path,default=ROOT/'sam_character/model/qa/polish')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    report=dict(binary_sha256=digest(args.binary),packages=[],renders=[],scope='Structural skin/GLB checks and first-frame renders of all eight imported clips; not an exhaustive clipping or likeness proof.')
    environment={k:v for k,v in os.environ.items() if not k.startswith('SSX_')}
    for package in PACKAGES:
        report['packages'].append(check_package(package))
        clips=json.loads((ROOT/'local/assets/native'/package/'animation-samples.json').read_text())['clips']
        cases=[(clip['name'],{'SSX_AUDIT_MODEL_VIEW':'full','SSX_AUDIT_CLIP':clip['name']}) for clip in clips]
        cases += [('closeup',{'SSX_AUDIT_CLOSEUP':'1'}),('back',{'SSX_AUDIT_MODEL_VIEW':'back'})]
        for label,extra in cases:
            image=args.output/f'{package}-{label}.png'
            result=subprocess.run([str(args.binary),package,str(image)],env={**environment,**extra},capture_output=True,text=True)
            if result.returncode:raise RuntimeError(result.stdout+result.stderr)
            report['renders'].append(dict(package=package,pose=label,image=str(image.relative_to(ROOT)),sha256=digest(image),exit_code=0))
    assert report['binary_sha256']==digest(args.binary),'Binary changed during validation'
    for package in report['packages']:assert package==check_package(package['package']),'Mesh changed during validation'
    (args.output/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"Validated {len(report['packages'])} skinned GLBs and {len(report['renders'])} native Metal renders.")

if __name__=='__main__':main()
