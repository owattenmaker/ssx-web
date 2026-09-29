#!/usr/bin/env python3
"""Recover original rider assemblies and skinning data for native-engine development.

SPDX-License-Identifier: GPL-3.0-only
SSX-Library MNF format research is credited in docs/asset-formats.md.
This selects a test outfit; original default/career outfit selection is unfinished.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from collections import defaultdict
from compare_character_assets import big_members
from world_assets import refpack,gamecube_cmpr
from world_models import Reader


# Active LOD0 resources uniquely matched to the owned Snow Jam glide snapshot's
# PS2 headers (all bytes except runtime pointer fields56..67). Public character
# names differ from original development resource prefixes for Allegra and Griff.
NPC_PRESETS={
    'psymon':dict(prefix='psymon',archive='psymotxn',actor=0x18d0c40,gameplay_character=8,
        parts=('TopA','BindingsA','BoardFlexA','BottomA','HeadA','HandsA','BootsA','Antenna'),hair='Antenna',extras=('eatd',)),
    'allegra':dict(prefix='arielle',archive='allegtxn',actor=0x18e1270,gameplay_character=2,
        parts=('TopA','BindingsA','BoardFlexA','BottomA','HeadA','HandsA','BootsA','PigtailsB','AssHangerB','TopBoltA','TopBoltD'),hair='PigtailsB',extras=('eatb','extc')),
    'moby':dict(prefix='moby',archive='mobytxn',actor=0x18f1d50,gameplay_character=0,
        parts=('TopA','BindingsA','BoardFlexA','BottomA','HeadA','HandsA','BootsA','HeadBoltB4','Tshirt','HeadBoltD2','MobyEarrings'),hair='HeadBoltB4',extras=('eatc',)),
    'griff':dict(prefix='grommet',archive='grifftxn',actor=0x1902830,gameplay_character=5,
        parts=('TopA','BindingsA','BoardFlexA','BottomA','HeadA','HandsA','BootsA','Mop'),hair='Mop',extras=()),
    'luther':dict(prefix='luther',archive='othertxn',actor=0x1913310,gameplay_character=4,
        parts=('TopA','BindingsA','BoardFlexA','BottomA','HeadA','HandsA','Boots','Dreads','HeadBoltB'),hair='Dreads',extras=()),
}
RIDER_NAMES=('mac','zoe',*NPC_PRESETS)

def decode_high_model(data):
    r=Reader(data,'>')
    count,header,base=r.read('HHI',4)
    if not count or header<12: raise ValueError('Invalid MNF header')
    p=header
    name=data[p:p+16].split(b'\0')[0].decode()
    offset,size,bones_at,morph_ids,morph_data,unknown,weights_at,meshes_at,vertices_at,materials_at=r.read('10I',p+16)
    strips,nv,nb,nm,nw,ng,material_flags,file_id=r.read('8H',p+80)
    file_id=int.from_bytes(data[p+94:p+96],'little')
    body=refpack(data[base+offset:base+offset+size]);b=Reader(body,'>')
    bones=[]
    for i in range(nb):
        at=bones_at+i*80
        bone_name=body[at:at+16].split(b'\0')[0].decode()
        parent_file,parent_bone,dof_flags,bone_id=b.read('4h',at+16)
        if dof_flags & ~3: raise ValueError(f'Unsupported bone DOF flags {dof_flags}')
        position=b.read('4f',at+32);rotation=b.read('4f',at+48)
        bones.append(dict(name=bone_name,file=file_id,index=i,mirror_index=bone_id,parent_file=parent_file,
                          parent_index=parent_bone,dof_flags=dof_flags,translation=[v/100 for v in position[:3]],rotation=rotation,
                          source_translation_cm=list(position),source_rotation=list(rotation),
                          source_translation_bits=list(b.read("4I",at+32)),source_rotation_bits=list(b.read("4I",at+48)),
                          mirror_quaternion_map=list(body[at+24:at+28]),
                          mirror_translation_scale=list(b.read('4f',at+64))))
    weights=[]
    for i in range(nw):
        count,at,_,_=b.read('IIHH',weights_at+i*12)
        if not 1<=count<=4: raise ValueError(f'Unsupported skin influence count {count}')
        influences=[]
        for j in range(count):
            value,bone,file=b.read('hBB',at+j*4)
            if value<0: raise ValueError('Negative skin weight')
            influences.append(dict(weight=value,source_weight=value,file=file,bone=bone))
        total=sum(x['weight'] for x in influences)
        if not total: raise ValueError('Zero skin weight group')
        for x in influences:x['weight']/=total
        weights.append(influences)
    raw=[]
    for i in range(nv):
        x,y,z,nx,ny,nz,u,v=b.read('6h2H',vertices_at+i*16)
        raw.append([x/12700,y/12700,z/12700,nx/16384,ny/16384,nz/16384,u/65535,v/65535,0,0])
    vertices,skin,indices,positions=[],[],[],[]
    dedup={}
    material_ids=set();material_indices=defaultdict(list)
    for mesh in range(ng):
        weight_count,group_count,weight_indices_at,groups_at=b.read('4I',meshes_at+mesh*16)
        weight_indices=b.read(f'{weight_count}I',weight_indices_at)
        for group in range(group_count):
            at,byte_length,*mats=b.read('I6H',groups_at+group*16)
            material_ids.add(mats[0])
            command,n=b.read('BH',at)
            if command&0xf8!=0x98: raise ValueError(f'Unsupported MNF primitive {command:x}')
            strip=[]
            for v in range(n):
                matrix,position,normal,uv,_=b.read('BHHHB',at+4+v*8)
                if matrix<30 or (matrix-30)%3: raise ValueError('Invalid MNF skin matrix selector')
                slot=(matrix-30)//3
                if slot>=len(weight_indices): raise ValueError('MNF skin selector outside mesh palette')
                weight=weight_indices[slot]
                if weight>=len(weights) or max(position,normal,uv)>=nv: raise ValueError('MNF vertex reference outside arrays')
                key=(position,normal,uv,weight)
                if key not in dedup:
                    dedup[key]=len(vertices)
                    vertices.append(raw[position][:3]+raw[normal][3:6]+raw[uv][6:])
                    skin.append(weights[weight]);positions.append(position)
                strip.append(dedup[key])
            for i in range(2,len(strip)):
                tri=strip[i-2:i+1]
                if len({tuple(vertices[j][:3]) for j in tri})<3: continue
                triangle=tri[::-1] if i&1 else tri
                indices.extend(triangle);material_indices[mats[0]].extend(triangle)
    materials=[];material_batches=[]
    for i in sorted(material_ids):
        at=materials_at+i*32
        b.read('32B',at)
        materials.append(body[at:at+4].decode())
        material_batches.append(dict(material=materials[-1],indices=material_indices[i]))
    # Morph targets (the NIS heads/hands, tools/export_fe_preview.py): nm 16-byte records at morph_data
    # {count, compact, deltas, indices}: count u16 raw-position indices, then count xyz deltas: int16 in position
    # units (1/12700 m) or, when compact=1, int8 in millimetres (1/1000 m). Checked against the PS2 MPF twins, whose
    # VIF morph packets (UNPACK V4-8 {dx,dy,dz,slot}) hold every morph in one 4 mm unit: GC int16 = 50.8 x PS2,
    # GC int8 = 4.0 x PS2 on the best-fitting vertices (docs/characters.md). morph_ids[i] is record i's channel.
    morphs=[]
    for i in range(nm):
        n,compact,deltas_at,indices_at=b.read('4I',morph_data+i*16)
        if compact not in (0,1) or deltas_at!=indices_at+2*n:raise ValueError('Unsupported MNF morph record')
        targets=b.read(f'{n}H',indices_at);deltas=b.read(f'{3*n}{"b" if compact else "h"}',deltas_at)
        if n and max(targets)>=nv:raise ValueError('MNF morph vertex outside arrays')
        morphs.append(dict(channel=b.read('I',morph_ids+i*4)[0],positions=list(targets),deltas=[[deltas[3*k+j]/(1000 if compact else 12700) for j in range(3)] for k in range(n)],compact=compact))
    return dict(name=name,vertices=vertices,indices=indices,skin=skin,bones=bones,materials=materials,
                material_batches=material_batches,morph_count=nm,source_sha256=hashlib.sha256(data).hexdigest(),
                vertex_positions=positions,morphs=morphs,file=file_id)


def decode_rider_texture(data):
    """Neutral RGBA from CMPR or SHPG type21 (GX RGB5A3) 4x4 tiles."""
    if data and data[0]==30:return gamecube_cmpr(data,header_size=16)
    if len(data)<16 or data[0]!=21:raise ValueError('Unsupported rider texture format')
    width,height=struct.unpack_from('>HH',data,4)
    if not width or not height or width%4 or height%4 or max(width,height)>4096 or len(data)<16+width*height*2:
        raise ValueError('Invalid RGB5A3 texture extent')
    out=bytearray(width*height*4);at=16
    for ty in range(0,height,4):
        for tx in range(0,width,4):
            for dy in range(4):
                for dx in range(4):
                    word=struct.unpack_from('>H',data,at)[0];at+=2
                    if word&0x8000:
                        rgb=[(word>>shift)&31 for shift in (10,5,0)]
                        color=[(v<<3)|(v>>2) for v in rgb]+[255]
                    else:
                        alpha=(word>>12)&7
                        color=[((word>>shift)&15)*17 for shift in (8,4,0)]+[(alpha<<5)|(alpha<<2)|(alpha>>1)]
                    pixel=((ty+dy)*width+tx+dx)*4;out[pixel:pixel+4]=bytes(color)
    return width,height,bytes(out)


def read_bolt_entries(data):
    """Read the GC wardrobe's bounded item/string tables; no equip-rule guesses.

    Layout adapted from SSX-Library BoltPS2Handler with GC big-endian integers.
    """
    reader=Reader(data,'>')
    count=reader.read('I',4)[0]
    position=8+count*56
    reader.read(f'{count*56}s',8)
    for stride in (12,8,8):
        rows=reader.read('I',position)[0]
        position+=4
        reader.read(f'{rows*stride}s',position)
        position+=rows*stride
    string_size=reader.read('I',position)[0];base=position+4
    reader.read(f'{string_size}s',base)
    def string(offset):
        if not offset:return ''
        if not 1<=offset<=string_size:raise ValueError('Bolt string offset outside pool')
        at=base+offset-1;end=data.find(b'\0',at,base+string_size)
        if end<0:raise ValueError('Unterminated bolt string')
        return data[at:end].decode()
    entries=[]
    for index in range(count):
        at=8+index*56
        item,parent=reader.read('2h',at+4)
        offsets=reader.read('8I',at+20)
        values=list(map(string,offsets))
        entries.append(dict(character=data[at],id=item,parent=parent,name=values[0],
                            lods=values[1:5],model=values[5],texture=values[6]))
    return entries


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rider',choices=RIDER_NAMES,default='mac')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();character=args.rider
    preset=NPC_PRESETS.get(character);prefix=preset['prefix'] if preset else character
    texture_archive=preset['archive'] if preset else f'{character}txn'
    root=Path('local/gamecube/disc/files/data/char')
    model_files=dict(big_members((root/'mdlngc.big').read_bytes()))
    texture_files=dict(big_members((root/f'{texture_archive}.big').read_bytes()))
    bolts=(root/'boltngc.dat').read_bytes()
    wardrobe=read_bolt_entries(bolts)
    if character=='mac':
        hair=next(e for e in wardrobe if e['character']==3 and e['name']=='Bedhead')
        hair_model=next(e for e in wardrobe if e['character']==3 and e['parent']==hair['id'] and e['lods'][0]=='Dangle_H')
        parts=('TopA','TopB','BottomA','BootsA','HeadA','HandsA','BindingsA','BoardFlexA','Dangle')
        skip={'TopA'}
    elif character=='zoe':
        # Active model headers in snow-jam-glide EE geometry+0C (LOD0), not
        # guesses from wardrobe item names. Texture colors remain a test choice.
        hair=next(e for e in wardrobe if e['character']==4 and e['id']==458)
        hair_model=hair
        parts=('TopB','BindingsA','BoardFlexA','BottomB','HeadA','HandsB','BootsA','Mop')
        skip=set()
    else:
        parts=preset['parts'];skip=set()
        resource=f"{prefix}_{preset['hair']}.mnf".lower()
        candidates=[e for e in wardrobe if e['model'].split('|')[-1].lower()==resource]
        if not candidates:raise ValueError(f'Missing authored wardrobe model {resource}')
        hair=next((e for e in candidates if e['texture']),candidates[0]);hair_model=hair
    hair_texture_name=hair['texture'].split('|')[1] if hair['texture'] else f'{prefix}_alph_a01.gsh'
    decoded={part:decode_high_model(model_files[f'{"board" if part in ("BindingsA","BoardFlexA") else prefix}_{part}.mnf']) for part in parts}
    bones=[]
    bone_map={}
    for part,model in decoded.items():
        for bone in model['bones']:
            key=(bone['file'],bone['index'])
            if key not in bone_map:
                bone_map[key]=len(bones);bones.append(bone)
    for bone in bones:
        parent=(bone['parent_file'],bone['parent_index'])
        if parent==(-1,-1):bone['parent']=-1
        elif parent in bone_map:bone['parent']=bone_map[parent]
        else:raise ValueError(f'Missing parent bone {parent}')
        # The rider bind mesh is Z-up, while the course renderer uses Y-up.
        # Conjugate every bone's local transform by the same basis conversion.
        x,y,z=bone['translation'];bone['translation']=[x,z,-y]
        x,y,z,w=bone['rotation'];bone['rotation']=[x,z,-y,w]
    # Original PS2 cAnimModel_compile 0x30E140..0x30E190: bone+20 bit0
    # advances three translation channels; bit1 advances three rotation channels.
    # Both board bones carry translations, unlike the body after its root.
    cursors=defaultdict(int)
    for bone in sorted(bones,key=lambda b:(b['file'],b['index'])):
        cursor=cursors[bone['file']]
        bone['animation_translation_channel']=cursor if bone['dof_flags']&1 else -1
        cursor+=3 if bone['dof_flags']&1 else 0
        bone['animation_rotation_channel']=cursor if bone['dof_flags']&2 else -1
        cursors[bone['file']]=cursor+(3 if bone['dof_flags']&2 else 0)
    folder=args.output or Path(f'local/assets/native/RIDER_{character.upper()}');(folder/'textures').mkdir(parents=True,exist_ok=True)
    texfiles={kind:f'{prefix}_{name}.gsh' for kind,name in dict(suit='suit_b01_b01' if character=='zoe' else 'suit_a01_a01',boot='boot_a01_a01',head='head_a01',bord='bord_a01').items()}
    texfiles['alph']=hair_texture_name.lower()
    if character=='zoe':texfiles['eatf']='zoe_eatf_01.gsh'
    if preset:
        for kind in preset['extras']:texfiles[kind]=f'{prefix}_{kind}_01.gsh'
    if character=='luther':texfiles['suit']='luther_body_a01.gsh'
    textures={};texture_ids={}
    for i,(kind,name) in enumerate(texfiles.items()):
        data=texture_files[name];data=refpack(data) if data[:2]==b'\x10\xfb' else data
        count,=struct.unpack_from('>I',data,8)
        if count!=1:raise ValueError('Expected one texture per test outfit resource')
        at,=struct.unpack_from('>I',data,20)
        w,h,rgba=decode_rider_texture(data[at:])
        path=f'textures/9-{i}.rgba';(folder/path).write_bytes(rgba)
        textures[f'9-{i}']=dict(width=w,height=h,path=path,source='gamecube',resource=name,archive=f'{texture_archive}.big',equipped_variant_verified=False)
        texture_ids[kind]=i
    vertices=[];indices=[];skin=[];source_skin=[];batches=[];part_info=[]
    for part,model in decoded.items():
        # TopA supplies the skeleton and only a dummy triangle, not torso art.
        if part in skip:continue
        base=len(vertices);first=len(indices)
        for v in model['vertices']:
            vertices.append([v[0],v[2],-v[1],v[3],v[5],-v[4]]+v[6:])
        for group in model['material_batches']:
            kind=group['material']
            # Mac HandsA shares the head material in this selected outfit.
            if character=='mac' and part=='HandsA':kind='head'
            batches.append(dict(first_index=len(indices),index_count=len(group['indices']),texture=texture_ids[kind],lightmap=-1,instance=True))
            indices.extend(base+i for i in group['indices'])
        for influences in model['skin']:
            mapped=[];source_mapped=[]
            for influence in influences:
                key=influence['file'],influence['bone']
                if key not in bone_map:raise ValueError(f'Missing skin bone {key} in {part}')
                mapped.append((bone_map[key],influence['weight']))
                source_mapped.append((bone_map[key],influence['source_weight']))
            skin.append(mapped);source_skin.append(source_mapped)
        part_info.append(dict(part=part,resource=f'{"board" if part in ("BindingsA","BoardFlexA") else prefix}_{part}.mnf',model=model['name'],morph_count=model['morph_count'],source_sha256=model['source_sha256']))
    (folder/'vertices.bin').write_bytes(b''.join(struct.pack('<10f',*v) for v in vertices))
    (folder/'indices.bin').write_bytes(struct.pack(f'<{len(indices)}I',*indices))
    (folder/'colors.bin').write_bytes(struct.pack('<4f',1,1,1,1)*len(vertices))
    (folder/'rider.json').write_text(json.dumps(dict(bones=bones,skin=skin,source_skin=source_skin,source_skin_weight_units='integer-percent',parts=part_info,units='meters',up_axis='Y',
        hairstyle=dict(name=hair['name'],item_id=hair['id'],model_item_id=hair_model['id'],
                       bolt_sha256=hashlib.sha256(bolts).hexdigest(),secondary_motion='Bind pose; original hair simulation unfinished'),
        character=character,resource_prefix=prefix,texture_archive=f'{texture_archive}.big',texture_selection_verified=False,assembly_evidence=(dict(snapshot='snow-jam-glide',ee_sha256='5dd432c88c5256c12e0aa0593a3483111bd84e0db2d39dd646444783bdf969b6',actor='0x014701A0',geometry='0x005DC600',active_lod0=list(parts)) if character=='zoe' else (dict(snapshot='snow-jam-glide',ee_sha256='5dd432c88c5256c12e0aa0593a3483111bd84e0db2d39dd646444783bdf969b6',actor=hex(preset['actor']),gameplay_character_id=preset['gameplay_character'],active_lod0=list(parts),header_fixup_bytes=[56,68]) if preset else None)),note=('Mac test outfit.' if character=='mac' else f'{character.title()} model selection verified against snow-jam-glide geometry LOD0; texture colors are original baseline resources, not yet verified against equipped variants.')+' Morph/secondary motion and career equip rules remain unfinished.'),indent=2)+'\n')
    bounds=[[min(v[k] for v in vertices) for k in range(3)],[max(v[k] for v in vertices) for k in range(3)]]
    manifest=dict(version=1,location=f'RIDER_{character.upper()}',vertex_stride=40,vertex_count=len(vertices),index_count=len(indices),
                  bounds=bounds,batches=batches,textures=textures,lighting_verified=False,
                  imported_patch_count=0,imported_instance_count=len(parts)-len(skip),missing_textures=[],source=f'GameCube {character} original rider assets')
    (folder/'world.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(dict(vertices=len(vertices),triangles=len(indices)//3,bones=len(bones),bounds=bounds)))


if __name__=='__main__':main()
