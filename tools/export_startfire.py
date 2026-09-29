#!/usr/bin/env python3
"""Export the ARA1 start-gate spark fountains (startfirePop) from original data.

Sources, all verified here rather than typed in:
- Stage kind-16 handler rows (bam.ssb chunk 33) link the startfireTrig and
  startfireTimer instances to LUN programs 116 and 118.
- Program 118 (ara1-disassembly.json, tools/disassemble_stage_scripts.py) calls
  builtin 0x10 (0x2FD420, MakeParticleData) six times; its keyed arguments
  follow the snow profile FIELDS order. Builtin 0x37 arguments gate the pairs.
- Texture 28 (`spx2`, table 0x4891B0) from PS2 DATA/TEXTURES/PARTICLE.SSH,
  raw GS alpha 0..128 retained.
- Instance matrices/bounds from the packaged world_collision.json.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from export_snow_assets import FIELDS,member
from world_assets import world_chunks,records

INT_FIELDS={'NumParticles','NumBlur','TextureId','BlendMode','NumFlipTextures'}
TEXTURE_ID,TEXTURE_NAME=28,'spx2'

def decode_program(program):
    """Return [(builtin,args,{key:value})] for program calls, with keyed arguments."""
    keys={};registers={};calls=[]
    for ins in program['instructions']:
        op,w,inline=ins['opcode'],ins['word'],ins['inline_words']
        key=(w>>8)&255
        if op==0x28:keys[key]=('int',(w>>16)&0xffff)
        elif op==0x29:keys[key]=('float',float(struct.unpack('<h',struct.pack('<H',(w>>16)&0xffff))[0]))
        elif op==0x25:keys[key]=('int',inline[0])
        elif op==0x26:keys[key]=('float',struct.unpack('<f',struct.pack('<I',inline[0]))[0])
        elif op==0x27:keys[key]=('resource',inline[0])
        elif op==0x17:registers[key]=struct.unpack('<f',struct.pack('<I',inline[0]))[0]
        elif op==0x16:registers[key]=inline[0]
        elif op==0x23:registers[key]=-registers[(w>>16)&255]
        elif op==0x20:keys[key]=('float',registers[(w>>16)&255])
        elif op==0x21:calls.append((ins['builtin'],ins['argument_count'],dict(keys)));keys={}
    return calls

def texture(iso,elf):
    from inspect_disc import Disc
    address=0x4891b0+12*TEXTURE_ID;phoff=struct.unpack_from('<I',elf,28)[0];size,count=struct.unpack_from('<HH',elf,42);tag=None
    for n in range(count):
        kind,offset,vaddr,_,length,_,_,_=struct.unpack_from('<8I',elf,phoff+n*size)
        if kind==1 and vaddr<=address<address+4<=vaddr+length:tag=elf[offset+address-vaddr:offset+address-vaddr+4]
    if tag!=TEXTURE_NAME.encode():raise ValueError(f'Texture table 28 is {tag!r}')
    disc=Disc(iso)
    try:container=disc.file('DATA/TEXTURES/PARTICLE.SSH')
    finally:disc.close()
    offset,data=member(container,TEXTURE_NAME);width,height=struct.unpack_from('<HH',data,4);end=int.from_bytes(data[1:4],'little')
    if not(data[0]==2 and end==16+width*height and data[end]==33):raise ValueError('Unexpected spx2 encoding')
    palette=data[end+16:end+int.from_bytes(data[end+1:end+4],'little')];raw=bytearray()
    for index in data[16:end]:
        entry=(index&0xe7)|((index&8)<<1)|((index&16)>>1);raw.extend(palette[entry*4:entry*4+4])
    if max(raw[3::4])>128:raise ValueError('Unexpected GS alpha range')
    return dict(id=TEXTURE_ID,name=TEXTURE_NAME,width=width,height=height,gs_alpha_file='spx2.gs.rgba',gs_alpha_scale=255/128,source='PS2 DATA/TEXTURES/PARTICLE.SSH',source_offset=offset,source_sha256=hashlib.sha256(container).hexdigest(),sha256=hashlib.sha256(raw).hexdigest()),bytes(raw)

def main():
    from disc_paths import ps2_iso;p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ps2-iso',type=Path,default=ps2_iso());p.add_argument('--output',type=Path,default=root/'web/public/assets/STARTFIRE');a=p.parse_args()
    elf=(root/'local/disc/SLUS_207.72').read_bytes()
    world=json.loads((root/'web/public/assets/ARA1/world_collision.json').read_text())
    programs=json.loads((root/'local/browser-pickups/ara1-disassembly.json').read_text())['programs']
    for ci,chunk in enumerate(world_chunks(root/'local/assets/source/ps2/bam.ssb')):
        if ci==33:stage=next(d for k,t,r,d in records(chunk) if k==16);break
    rows,base=struct.unpack_from('<2I',stage,0x18);assert (rows,base)==(231,0x70)
    descriptors=world['bindings']['8']['descriptors'];named={i['name']:i for i in world['instances'] if i['track']==8}
    def handler(name,slot):
        resource=descriptors[named[name]['collision_descriptor']]['resource08'];assert resource&255==8
        script=struct.unpack_from('<6I',stage,base+(resource>>8)*24)[slot];assert script&255==8;return script>>8
    trigger,timer=named['mdl_ARA1_startfireTrig_1000'],named['mdl_ARA1_startfireTimer']
    trigger_program,timer_program=handler(trigger['name'],2),handler(timer['name'],5)
    assert (trigger_program,timer_program)==(116,118),(trigger_program,timer_program)
    start=decode_program(programs[trigger_program])
    assert [c[0] for c in start]==[0x01,0x2c,0x03],start
    debounce=start[0][2][2][1];timer_key05=start[2][2][5][1]
    assert start[1][2][0]==('resource',(timer['rid']<<8)|8),'trigger does not target the timer'
    calls=decode_program(programs[timer_program]);by_rid={i['rid']:i for i in named.values()}
    profile=None;schedule=[];gate=None;pending=None;sound=None
    for builtin,count,keys in calls:
        if builtin==0x37:gate=keys[1][1]
        elif builtin==0x2c:pending=keys[0][1]
        elif builtin==0x1e:sound=keys[1][1]
        elif builtin==0x10:
            assert count==52 and pending&255==8;instance=by_rid[pending>>8];assert instance['name'].startswith('mdl_ARA1_startfirePop_')
            values={name:(int(keys[k][1]) if name in INT_FIELDS else float(keys[k][1])) for k,name in enumerate(FIELDS[:52]) if k in keys}
            if profile is None:profile=values
            elif values!=profile:raise ValueError('startfire emitters differ')
            if not schedule or schedule[-1]['gate']!=gate:schedule.append(dict(gate=gate,instances=[]))
            schedule[-1]['instances'].append(dict(rid=instance['rid'],name=instance['name'],matrix=instance['matrix']))
    assert profile['TextureId']==TEXTURE_ID and profile['BlendMode']==0 and len(schedule)==3 and all(len(s['instances'])==2 for s in schedule)
    info,raw=texture(a.ps2_iso,elf)
    out=a.output;out.mkdir(parents=True,exist_ok=True);(out/'spx2.gs.rgba').write_bytes(raw)
    manifest=dict(version=1,coordinate_system='Original source centimeters, Z-up (native = (x,z,-y)/100)',
        profile=profile,profile_source=f'LUN program {timer_program} builtin 0x10 (0x2FD420) keyed arguments; keys follow export_snow_assets.FIELDS',
        missing_keys_use_constructor_defaults=[f for f in FIELDS[:52] if f not in profile],
        blend=dict(mode=0,gs_alpha='0x48 (Cs*As+Cd), table 0x44B420[0]=7 -> setter 0x3624F4'),
        texture=info,
        trigger=dict(name=trigger['name'],rid=trigger['rid'],bounds_min_cm=trigger['bounds_min_cm'],bounds_max_cm=trigger['bounds_max_cm'],program=trigger_program,debounce=debounce,timer_key05=timer_key05),
        timer=dict(name=timer['name'],rid=timer['rid'],program=timer_program),
        schedule=schedule,sound=sound,
        inferred=dict(gate_unit_seconds=1/30,gate_note='builtin 0x37 argument scales entity vtable+0x100 by 1/30; interpreted as timer time since trigger',
            trigger_volume='instance world AABB approximates the authored trigger slab',
            burst='Duration>=0: 0x370788 advances kernel age only (0x36D3E8); particle i is emitted at i*Duration/NumParticles (remaining time Duration+Life+LifeR/2 set by 0x3705E0)'))
    (out/'startfire.json').write_text(json.dumps(manifest,indent=1)+'\n')
    print(json.dumps(dict(output=str(out),profile_fields=len(profile),pairs=[[i['rid'] for i in s['instances']] for s in schedule],gates=[s['gate'] for s in schedule])))

if __name__=='__main__':main()
