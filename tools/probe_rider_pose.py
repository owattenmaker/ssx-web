#!/usr/bin/env python3
"""Read original animation state and bone arrays for native-pose conformance.

This is offline reference instrumentation. Expected bone arrays are test outputs,
never native gameplay inputs. The native pose is generated from original assets.
"""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path
from reference_ground_profile import extract_ground

def extract(path,rider=0x14701a0,*,rig_path=None):
    with zipfile.ZipFile(path) as z:m=z.read('eeMemory.bin')
    def u(at):return struct.unpack_from('<I',m,at)[0]
    def bits(at,n):return list(struct.unpack_from('<'+'I'*n,m,at))
    def f(at,n=1):return list(struct.unpack_from('<'+'f'*n,m,at))
    geometry=u(rider+0x780);animation=u(rider+0x784);parts={}
    for i in range(u(geometry+8)):
        p=u(geometry+12)+i*0x58;parts[u(p)]=(u(p+4),u(p+0x44))
    layers=[]
    for channel in range(6):
        seq=u(u(animation+0x50)+channel*8+4);seen=set()
        while seq:
            if seq in seen:raise ValueError('Cyclic animation sequence list')
            seen.add(seq)
            for slot in range(3):
                s=seq+slot*0x1c
                if not u(s+0x18):continue
                time=f(s+8)[0];weight=f(seq+0x94)[0]*f(s+0x14)[0];duration=f(s+0x10)[0]
                fade_in=f(seq+0xa8)[0];fade_out=f(seq+0xa4)[0]
                if time<fade_in:weight=min(weight,time/fade_in)
                if duration-time<fade_out:weight=min(weight,(duration-time)/fade_out)
                layers.append(dict(seek_pending=bool(u(seq+0xc4)),raised_flags=struct.unpack_from('<Q',m,seq+0xb8)[0],completed=bool(u(seq+0xc0)),mirror=bool(u(seq+0x80)),root_position=f(seq+0x60,3),root_rotation=f(seq+0x70,4),sequence_flags=struct.unpack_from('<Q',m,seq+0xb0)[0],channel=channel,sequence=seq,slot=slot,semantic=u(seq),clip=u(s+4),time=time,weight=weight,priority=u(seq+0x84),mask=struct.unpack_from('<Q',m,seq+0x88)[0],speed=f(s+12)[0],duration=duration,slot_weight=f(s+0x14)[0],sequence_weight=f(seq+0x94)[0],fade_target=f(seq+0x98)[0],fade_remaining=f(seq+0x9c)[0],stop_on_fade=bool(u(seq+0xa0)),sequence_speed=f(seq+0x90)[0],loop=bool(u(s+0x1c)),fade_in=fade_in,fade_out=fade_out,base_weight=f(seq+0x94)[0]*f(s+0x14)[0]))
            seq=u(seq+0xc8)
    rig_file=Path(rig_path) if rig_path is not None else Path('local/assets/native/RIDER_ZOE/rider.json')
    if rig_file.is_dir():rig_file=rig_file/'rider.json'
    rig=json.loads(rig_file.read_text());bones=[]
    for b in rig['bones']:
        base,count=parts[b['file']];index=base+b['index']
        if b['index']>=count:raise ValueError('Compiled bone mapping outside original part')
        bones.append(dict(name=b['name'],file=b['file'],source_index=index,local_position_bits=bits(u(geometry+0x24)+16*index,3),local_rotation_bits=bits(u(geometry+0x28)+16*index,4),world_position_bits=bits(u(geometry+0x2c)+32*index,3),world_rotation_bits=bits(u(geometry+0x2c)+32*index+16,4),local_position=f(u(geometry+0x24)+16*index,3),local_rotation=f(u(geometry+0x28)+16*index,4),world_position=f(u(geometry+0x2c)+32*index,3),world_rotation=f(u(geometry+0x2c)+32*index+16,4)))
    source_mask=struct.unpack_from('<Q',m,geometry+0x150)[0]
    limited_bones=bool(u(rider+0xb1c))
    if limited_bones:source_mask &= struct.unpack_from('<Q',m,geometry+0x158)[0]
    bone_mask=sum(1<<i for i,b in enumerate(bones) if source_mask & (1<<b['source_index']))
    legs=[]
    for offset in (0x8e0,0x910):
        legs.append(dict(bones=[u(rider+offset+k*4) for k in range(3)],position=f(rider+offset+16,3),rotation=f(rider+offset+32,4)))
    contact=dict(board_direction=f(rider+0x390,3),normal=f(rider+0x370,3),board_alignment=f(rider+0x2bc)[0],board_lift_cm=f(rider+0x31c)[0],leg_weight=f(rider+0x318)[0],legs=legs)
    presentation=dict(turn=f(rider+0x1f0)[0],extra_lean=f(rider+0x208)[0],brake=f(rider+0x214)[0],roll=f(rider+0x250)[0],lift_cm=f(rider+0x2c8)[0],lateral=f(rider+0x3b0,3),control_state=u(u(rider+0x77c)+0xde4))
    air_state=None
    if presentation['control_state']==5:
        from reference_air_control import extract_air_control
        air_state=extract_air_control(m,rider)['state']
    ground=extract_ground(m,rider)
    from reference_animation_variants import extract_animation_variant_flags
    return dict(random_state=list(struct.unpack_from('<6I',m,0x4ff030)),next_rate=f(animation+0x1c)[0],variant_flags=extract_animation_variant_flags(m,rider),grab_end_semantic=436 if u(animation+0x64) else 287,current_semantics=[u(animation+i*4) for i in range(6)],character=rig['character'],bone_mask=bone_mask,limited_bones=limited_bones,default_root_position=f(animation+0x30,3),default_root_rotation=f(animation+0x40,4),default_mirror=bool(u(animation+0x18)),contact_cache=ground['cache'],body_cache=ground['body_cache'],air_state=air_state,pivot_bone=u(rider+0x89c),pose_state=ground['state'],presentation=presentation,contact=contact,source=str(path),ee_sha256=hashlib.sha256(m).hexdigest(),rider=rider,geometry=geometry,animation=animation,root_position_cm=f(rider+0x110,3),root_rotation=f(rider+0x120,4),scale=f(geometry+0x140,3),layers=layers,bones=bones,
        note='Expected bones are audit-only; native pose generation consumes original rig/AFB assets and sequence state.')

def animation_inputs(path,rider=0x14701a0,*,collision_enabled=True,rig_path=None):
    """Native initialization only: deliberately excludes expected bone arrays."""
    data=extract(path,rider,rig_path=rig_path)
    return dict(character=data.get('character','zoe'),collision_enabled=collision_enabled,
                **{key:data[key] for key in ('layers','scale','contact','presentation','pose_state','pivot_bone','air_state','contact_cache','body_cache','default_root_position','default_root_rotation','default_mirror','bone_mask','limited_bones','current_semantics','grab_end_semantic','variant_flags','random_state','next_rate') if key in data},
                provenance=dict(source=data['source'],ee_sha256=data['ee_sha256']))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--rider',type=lambda x:int(x,0),default=0x14701a0);p.add_argument('--rig',type=Path);a=p.parse_args();result=extract(a.snapshot,a.rider,rig_path=a.rig);a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k!='bones'},indent=2))
if __name__=='__main__':main()
