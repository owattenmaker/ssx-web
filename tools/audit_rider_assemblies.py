#!/usr/bin/env python3
"""Match live original rider assemblies to owned PS2/GC model resources.

Expected rig/bone values are validation outputs only. This tool never publishes
captured pose arrays as assets or gameplay inputs.
"""
import argparse,hashlib,json,struct,zipfile
from collections import defaultdict
from pathlib import Path
from compare_character_assets import big_members
from rider_assets import decode_high_model,NPC_PRESETS

ROOT=Path(__file__).resolve().parents[1]
_ROSTER=json.loads((ROOT/'web/public/assets/riders.json').read_text())
ROSTER_BY_CHARACTER={e['character']:e['id'] for e in _ROSTER if e['kind']=='rider'}
PREFIX_OF={e['id']:(e.get('resource_prefix') or e['id']) for e in _ROSTER if e['kind'] in ('rider','cheat')}
def rig_file(name):
    """Native rig of a rider: the Snow Jam opponents and Zoe in local/assets/native, the other characters in
    local/assets/native/CHARACTERS (tools/export_npc_riders.py rig_folder; CHARACTERS/RIDER_MAC is the live Mac assembly)."""
    if name not in NPC_PRESETS and name!='zoe':
        character=ROOT/f'local/assets/native/CHARACTERS/RIDER_{name.upper()}/rider.json'
        if character.exists():return character
    return ROOT/f'local/assets/native/RIDER_{name.upper()}/rider.json'
def discover_participants(memory):
    """Race roster in slot order for any event savestate (tools/reference_race_event.py): the human is
    Zoe (the capture rider); computer riders are named by their gameplay character id (NPC_PRESETS)."""
    from reference_race_event import extract_race_event
    u=lambda at:struct.unpack_from('<I',memory,at)[0]
    game=int(extract_race_event(memory)['provenance']['game_address'],16);by_character={p['gameplay_character']:n for n,p in NPC_PRESETS.items()};rows=[]
    for n in range(u(game+0x78)):
        actor=u(game+0x28+n*4);entry=u(0x5305b0+u(actor+0x86c)*4);character=struct.unpack_from('<b',memory,0x535b20+entry*28+17)[0]
        human=(u(actor+0x6c0),u(actor+0x6e8))==(0x4583a8,0x458360)
        if not human and character not in by_character:
            # other roster characters (Peak 2/3 grids: Nate, Mac, Kaori, Elise, Viggo...): named by web/public/assets/riders.json
            name=ROSTER_BY_CHARACTER.get(character)
            if name is None:raise ValueError(f'Computer rider slot {n} has unexported gameplay character {character}')
            rows.append((name,actor));continue
        rows.append(('zoe' if human else by_character[character],actor))
    return rows

def audit(snapshot,ps2_archive,gamecube_archive,discover=False):
    with zipfile.ZipFile(snapshot) as archive:memory=archive.read('eeMemory.bin')
    def read(fmt,at):
        if at<0 or at+struct.calcsize(fmt)>len(memory):raise ValueError('Rider pointer outside EE memory')
        return struct.unpack_from(fmt,memory,at)
    def u(at):return read('<I',at)[0]
    ps2=dict(big_members(ps2_archive.read_bytes()));gc=dict(big_members(gamecube_archive.read_bytes()));headers=defaultdict(list)
    def key(raw):
        if len(raw)!=96:raise ValueError('Invalid original model header length')
        # The original loader writes three runtime pointers into reserved56..63;
        # every other model-header byte must match the owned source verbatim.
        return raw[:56]+bytes(12)+raw[68:]
    for name,data in ps2.items():
        count,at,_=struct.unpack_from('<HHI',data,4)
        for index in range(count):headers[key(data[at+index*96:at+(index+1)*96])].append((name,index))
    # Snow Jam fixtures: the verified fixed roster; --discover: any event savestate's roster (slot order).
    participants=discover_participants(memory) if discover else [('zoe',0x14701a0)]+[(name,p['actor'])for name,p in NPC_PRESETS.items()]
    rows=[]
    for name,actor in participants:
        geometry=u(actor+0x780);slot=u(actor+0x86c);entry=u(0x5305b0+slot*4);character=read('<b',0x535b20+entry*28+17)[0]
        expected_prefix=NPC_PRESETS[name]['prefix'] if name in NPC_PRESETS else 'zoe' if name=='zoe' else PREFIX_OF[name]
        count=u(geometry+8);base=u(geometry+12);models=[];bone_count=0;native_rig=json.loads(rig_file(name).read_text())
        native_resources={p.get('resource',f"{'board' if p['part'] in ('BindingsA','BoardFlexA') else expected_prefix}_{p['part']}.mnf")for p in native_rig['parts']}
        for index in range(count):
            part=base+index*0x58
            if not u(part+0x18):continue
            pointer=u(u(part+0x1c));raw=memory[pointer:pointer+96];candidates=headers[key(raw)]
            candidates=[x for x in candidates if x[0].split('_')[0] in (expected_prefix,'board')]
            if len(candidates)!=1:raise ValueError(f'Ambiguous/missing exact source model {name}/{index}: {candidates}')
            resource,lod=candidates[0]
            source_at=struct.unpack_from('<H',ps2[resource],6)[0]+lod*96
            if ps2[resource][source_at+56:source_at+68]!=bytes(12):raise ValueError('Selected model has nonzero source fixup fields')
            if any(value and not 0<value<len(memory) for value in read('<3I',pointer+56)):raise ValueError('Model fixup is not an EE pointer')
            if lod!=0:raise ValueError('Reference high-LOD assembly changed')
            gc_resource=str(Path(resource).with_suffix('.mnf'));model=decode_high_model(gc[gc_resource]);bone_pointer=u(part+0x38);live_bones=u(part+0x44)
            if live_bones!=len(model['bones']):raise ValueError(f'Bone count differs {resource}')
            differences=[]
            for bone_index,bone in enumerate(model['bones']):
                bits=read('<8I',bone_pointer+bone_index*80+32)
                if tuple(bone['source_translation_bits']+bone['source_rotation_bits'])!=bits:differences.append(bone_index)
            if differences:raise ValueError(f'Cross-version bind floats differ {resource}: {differences}')
            if gc_resource not in native_resources:raise ValueError(f'Active source model missing from native package: {gc_resource}')
            bone_count+=live_bones
            models.append(dict(part_id=u(part),resource_ps2=resource,resource_gamecube=gc_resource,lod=lod,
                header_sha256=hashlib.sha256(key(raw)).hexdigest(),ps2_sha256=hashlib.sha256(ps2[resource]).hexdigest(),gamecube_sha256=hashlib.sha256(gc[gc_resource]).hexdigest(),
                source_bind_bones=live_bones,all_source_bind_components_exact=True))
        if len(models)!=len(native_resources):raise ValueError(f'Native package includes unselected models: {name}')
        if bone_count!=len(native_rig['bones']):raise ValueError(f'Native compiled bone count differs: {name}')
        rows.append(dict(character=name,gameplay_character_id=character,visual_resource_prefix=expected_prefix,
            rider_address=hex(actor),slot=slot,geometry=hex(geometry),body_scale=list(read('<3f',geometry+0x140)),
            active_models=len(models),rig_bones=bone_count,models=models,
            texture_selection_verified=False,texture_note='Original texture resources are present; exact equipped color variants still require texture-binding recovery.'))
    return dict(snapshot=str(snapshot),ee_sha256=hashlib.sha256(memory).hexdigest(),
        ps2_archive_sha256=hashlib.sha256(ps2_archive.read_bytes()).hexdigest(),gamecube_archive_sha256=hashlib.sha256(gamecube_archive.read_bytes()).hexdigest(),
        header_runtime_fixups_excluded=[56,68],participants=rows)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot',type=Path,default=ROOT/'local/reference/pcsx2/snow-jam-glide.p2s')
    parser.add_argument('--ps2-models',type=Path,default=ROOT/'local/assets/source/ps2/mdlps2.big')
    parser.add_argument('--gc-models',type=Path,default=ROOT/'local/gamecube/disc/files/data/char/mdlngc.big')
    parser.add_argument('--output',type=Path,default=ROOT/'local/native-qa/opponent-assemblies.json')
    parser.add_argument('--discover',action='store_true',help='Read the roster from the savestate (non-Snow-Jam events)')
    args=parser.parse_args();result=audit(args.snapshot,args.ps2_models,args.gc_models,args.discover);args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    for rider in result['participants']:print(rider['character'],rider['active_models'],'original models,',rider['rig_bones'],'bind bones exactly match')
if __name__=='__main__':main()
