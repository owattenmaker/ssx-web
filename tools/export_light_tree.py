#!/usr/bin/env python3
"""Extract and cross-check the original static Snow Jam light acceleration index.

Only authored light IDs and region topology are exported. Captured rider poses,
candidate lists, coefficient banks and heap pointers are not runtime inputs.
"""
import argparse,hashlib,json,struct,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from locations import state as location_state,human_rider

def export(location='ARA1'):
    catalog=json.loads((ROOT/f'local/assets/native/{location}/local-lights.json').read_text())
    lookup={}
    for row in catalog['lights']:lookup.setdefault(row['source_body_sha256'],[]).append(row['resource'])
    previous=None;provenance=[];fixtures=[]
    # Snow Jam: three riding checkpoints must agree; other events: their glide checkpoint (tools/locations.py).
    snapshots=[(name,ROOT/f'local/reference/pcsx2/snow-jam-{name}.p2s') for name in ['glide','jump-31','jump-90']] if location=='ARA1' else [('glide',location_state(location,'glide'))]
    if not all(path.exists() for _,path in snapshots):
        # No event savestate yet: an explicitly empty index (no local light reaches the rider).
        output=dict(version=1,source_sha256=catalog['source_sha256'],roots=[dict(node=-1,exponent=0,cell=[0,0,0]) for _ in range(8)],nodes=[],light_count=0,
                    excluded_resources=sorted(row['resource'] for row in catalog['lights']),provisional=f'no {location} event savestate; runtime light index unknown')
        (ROOT/f'local/assets/native/{location}/light-tree.json').write_text(json.dumps(output,indent=2)+'\n');print(json.dumps(dict(location=location,provisional=True)));return
    for name,snapshot in snapshots:
        with zipfile.ZipFile(snapshot) as archive:memory=archive.read('eeMemory.bin')
        actor=human_rider(memory)
        def u(at):
            if not 0<=at<=len(memory)-4:raise ValueError('Tree address outside original memory')
            return struct.unpack_from('<I',memory,at)[0]
        manager=u(u(actor+0x860));nodes=[];seen=set();light_ids=set();pointer_map={}
        def visit(pointer,exponent):
            if not pointer:return -1
            if pointer in seen:raise ValueError('Shared/cyclic original octree node')
            seen.add(pointer);lights=[];obj=u(pointer+40);chain=set()
            while obj:
                if obj in chain:raise ValueError('Cyclic original extra-node list')
                chain.add(obj)
                if u(obj+8)==6:
                    key=hashlib.sha256(memory[obj+16:obj+112]).hexdigest();matches=lookup.get(key,[])
                    if len(matches)!=1:raise ValueError('Indexed light lacks unique authored record')
                    resource=matches[0]
                    if resource in light_ids:raise ValueError('Light indexed more than once')
                    light_ids.add(resource);lights.append(resource);pointer_map[str(obj)]=resource
                obj=u(obj)
            children=[visit(u(pointer+i*4),exponent-1)for i in range(8)]
            if not lights and all(child<0 for child in children):return -1
            if exponent<11:raise ValueError('Light below original query minimum depth')
            index=len(nodes);nodes.append(dict(children=children,lights=lights));return index
        roots=[]
        for i in range(8):
            pointer=u(manager+i*20+16)
            if not pointer:roots.append(dict(node=-1,exponent=0,cell=[0,0,0]));continue
            exponent,*cell=struct.unpack_from('<4i',memory,manager+i*20)
            node=visit(pointer,exponent)
            roots.append(dict(node=node,exponent=exponent if node>=0 else 0,cell=cell if node>=0 else [0,0,0]))
        tree=dict(roots=roots,nodes=nodes)
        if previous is not None and tree!=previous:raise ValueError('Light-only topology changes across reference checkpoints')
        previous=tree
        excluded=sorted(row['resource']for row in catalog['lights']if row['resource']not in light_ids)
        if location=='ARA1' and excluded!=[8,264]:raise ValueError('Unexpected authored lights missing from event index')
        provenance.append(dict(snapshot=name,ee_sha256=hashlib.sha256(memory).hexdigest(),full_nodes=len(seen)))
        fixtures.append(dict(snapshot=name,manager=manager,pointer_to_resource=pointer_map,
                             minimum=list(struct.unpack_from('<3f',memory,actor+0x400)),maximum=list(struct.unpack_from('<3f',memory,actor+0x410))))
    output=dict(version=1,source_sha256=catalog['source_sha256'],**previous,light_count=len(light_ids),excluded_resources=excluded,
                provenance=provenance,scope=f'Static light index for the captured {"Snow Jam" if location=="ARA1" else location} event state; insertion, activation and streaming transitions are not implemented')
    path=ROOT/f'local/assets/native/{location}/light-tree.json';path.write_text(json.dumps(output,indent=2)+'\n')
    (ROOT/('local/rider-lighting/light-tree-fixtures.json' if location=='ARA1' else f'local/rider-lighting/light-tree-fixtures-{location}.json')).write_text(json.dumps(fixtures,indent=2)+'\n')
    print(json.dumps(dict(nodes=len(nodes),lights=len(light_ids),excluded=excluded,identical_checkpoints=len(provenance))))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--location',default='ARA1');export(p.parse_args().location)
