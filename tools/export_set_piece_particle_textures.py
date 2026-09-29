#!/usr/bin/env python3
"""Export the textures of the set-piece particle systems (builtin16 / builtin26).

Every TextureId used by the stage programs of the given locations (LUN builtin 0x10
MakeParticleData and 0x1A DynamicParticle, keyed arguments in export_snow_assets.FIELDS
order) is resolved through the executable's texture table 0x4891B0 (12-byte entries,
packed 4-char tag) to its member of PS2 DATA/TEXTURES/PARTICLE.SSH (or EFFECTS.SSH).
Flipbooks: the draw uses TextureId + trunc(phase) (3708C0 at 370A3C, 371380 at 3713A8),
so NumFlipTextures > 1 exports the whole sequence id..id+N-1 (no current profile sets it;
the live systems hold NumFlipTextures 1, rate 20).

The PS2 records are 8-bit CLUT (encoding 2, CSM1 palette index swizzle) or 32-bit
(encoding 5); the raw GS alpha is kept (`gs_alpha_scale` 255/128 turns a normalised sample
into the GS blend factor). Most records use 0..128 (128 = 1.0); `star` (13) has a 0..255
CLUT alpha, uploaded unchanged (EE copy at its descriptor +0x20 equals the SSH CLUT), so its
MODULATE alpha (At*Av>>7, clamped 255) reaches ~2x in the GS blend: keep max_gs_alpha. All use TEX0 TFX MODULATE / TCC 1 and TEX1 0x61
(bilinear) in the live texture descriptors.

Output: web/public/assets/PARTICLES/textures/<tag>.gs.rgba and textures.json.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from export_snow_assets import FIELDS,member
from export_startfire import decode_program
from inspect_disc import Disc,EXPECTED_SHA1

TABLE=0x4891b0
def elf_bytes(elf,address,n):
    phoff=struct.unpack_from('<I',elf,28)[0];size,count=struct.unpack_from('<HH',elf,42)
    for i in range(count):
        kind,offset,vaddr,_,length,_,_,_=struct.unpack_from('<8I',elf,phoff+i*size)
        if kind==1 and vaddr<=address and address+n<=vaddr+length:return elf[offset+address-vaddr:offset+address-vaddr+n]
    raise ValueError(f'{address:#x} outside the executable')
def tag_of(elf,texture_id):return elf_bytes(elf,TABLE+12*texture_id,4).decode('ascii')

def programs(location):
    path=root/('local/browser-pickups/ara1-disassembly.json' if location=='ARA1' else f'local/browser-pickups/{location}/disassembly.json')
    return json.loads(path.read_text())['programs']

def used_textures(locations):
    """{texture id: {frames, rate, uses:[{location, program, builtin}]}} from builtin16/26 calls."""
    ti,fi,ri=FIELDS.index('TextureId'),FIELDS.index('NumFlipTextures'),FIELDS.index('FlipTextureRate')
    used={}
    for location in locations:
        for program in programs(location):
            calls=[i for i in program['instructions'] if i['opcode']==0x21 and i.get('builtin') in (16,26)]
            if not calls:continue
            for builtin,_,keys in decode_program(program):
                if builtin not in (16,26):continue
                if ti not in keys:raise ValueError(f'{location} program {program["index"]}: TextureId missing')
                texture=int(keys[ti][1]);frames=int(keys[fi][1]) if fi in keys else 1
                rate=float(keys[ri][1]) if ri in keys else None
                entry=used.setdefault(texture,dict(frames=set(),rates=set(),uses=[]))
                entry['frames'].add(frames);entry['rates'].add(rate)
                entry['uses'].append(dict(location=location,program=program['index'],builtin=builtin))
    # Every parameter block of the location's stage programs (builtin16 / builtin25 AddParticleData / builtin26,
    # defaults included: TextureId 16) and the emitters alive at race tick 0 (export_set_piece_particles.py).
    for location in locations:
        path=root/'web/public/assets'/location/'PARTICLES/particles.json'
        if not path.exists():raise ValueError(f'{path}: run tools/export_set_piece_particles.py --location {location} first')
        doc=json.loads(path.read_text())
        for b in doc['blocks']:
            w=b['words'];texture=w[49];frames=max(1,w[52]);rate=struct.unpack('<f',struct.pack('<I',w[53]))[0]
            entry=used.setdefault(texture,dict(frames=set(),rates=set(),uses=[]))
            entry['frames'].add(frames);entry['rates'].add(rate);entry['uses'].append(dict(location=location,program=b['program'],builtin=b['builtin']))
        for e in doc['initial']['effects']:
            if e['kind']!='Particle':continue
            texture=e['emitter'][1];frames=max(1,e['emitter'][0x180//4])
            entry=used.setdefault(texture,dict(frames=set(),rates=set(),uses=[]))
            entry['frames'].add(frames);entry['uses'].append(dict(location=location,initial=e['resource']))
    return used

def decode(container,name):
    offset,data=member(container,name);width,height=struct.unpack_from('<HH',data,4);size=int.from_bytes(data[1:4],'little')
    if data[0]==5 and size==16+width*height*4:raw=bytearray(data[16:size]);encoding='PSMCT32'
    elif data[0]==2 and size>=16+width*height and data[size]==33:   # base level; a record may carry mip levels up to its size (EFFECTS gltr)
        palette_size=int.from_bytes(data[size+1:size+4],'little');palette=data[size+16:size+palette_size]
        if palette_size<16 or len(palette)%4:raise ValueError(f'{name}: truncated CLUT')
        raw=bytearray()
        for index in data[16:16+width*height]:
            entry=(index&0xe7)|((index&8)<<1)|((index&16)>>1)       # CSM1 CLUT swizzle
            if entry*4+4>len(palette):raise ValueError(f'{name}: palette index {index} outside CLUT')
            raw.extend(palette[entry*4:entry*4+4])
        encoding='PSMT8+CLUT32'
    else:raise ValueError(f'{name}: unsupported SSH record encoding {data[0]}')
    return dict(width=width,height=height,encoding=encoding,source_offset=offset),bytes(raw)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--locations',nargs='+',default=['ARA1','BRA2','BHP1','ASS1','ABA1'])
    from disc_paths import ps2_iso;p.add_argument('--ps2-iso',type=Path,default=ps2_iso())
    p.add_argument('--output',type=Path,default=root/'web/public/assets/PARTICLES/textures')
    a=p.parse_args()
    elf=(root/'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original executable')
    # The manifest is shared by every location: keep the locations it already covers (and every exported particles.json),
    # so exporting one course never drops another course's textures.
    previous=a.output/'textures.json'
    if previous.exists():a.locations=list(a.locations)+[l for l in json.loads(previous.read_text()).get('locations',[]) if l not in a.locations]
    a.locations+=[q.parent.parent.name for q in sorted((root/'web/public/assets').glob('*/PARTICLES/particles.json')) if q.parent.parent.name not in a.locations]
    used=used_textures(a.locations)
    disc=Disc(a.ps2_iso)
    try:archives={n:disc.file(f'DATA/TEXTURES/{n}.SSH') for n in ('PARTICLE','EFFECTS')}
    finally:disc.close()
    out=a.output;out.mkdir(parents=True,exist_ok=True);textures=[];sequences=[]
    wanted={}
    for texture,entry in sorted(used.items()):
        frames=max(entry['frames'])
        for i in range(frames):wanted.setdefault(texture+i,[]).append(texture)
        sequences.append(dict(base_id=texture,frames=[texture+i for i in range(frames)],rates=sorted(r for r in entry['rates'] if r is not None),
            rate_default='constructor default (live systems: 20 frames/s, NumFlipTextures 1)' if None in entry['rates'] else None,
            uses=entry['uses']))
    for texture in sorted(wanted):
        tag=tag_of(elf,texture);source=None
        for name,container in archives.items():
            try:info,raw=decode(container,tag)
            except ValueError as e:
                if 'Missing original texture' in str(e):continue
                raise
            source=name;break
        if source is None:raise ValueError(f'texture {texture} {tag} not found in PARTICLE/EFFECTS.SSH')
        (out/f'{tag}.gs.rgba').write_bytes(raw)
        container=archives[source]
        textures.append(dict(id=texture,tag=tag,file=f'{tag}.gs.rgba',width=info['width'],height=info['height'],gs_alpha_scale=255/128,
            max_gs_alpha=max(raw[3::4]),encoding=info['encoding'],source=f'PS2 DATA/TEXTURES/{source}.SSH',source_offset=info['source_offset'],
            source_sha256=hashlib.sha256(container).hexdigest(),sha256=hashlib.sha256(raw).hexdigest(),table_entry=hex(TABLE+12*texture)))
    manifest=dict(schema_version=1,locations=a.locations,
        texture_table='0x4891B0 (12-byte entries, packed 4-char tag)',executable_sha1=EXPECTED_SHA1,
        sampling='TEX0 TFX MODULATE, TCC 1; TEX1 0x61 (bilinear mag/min, LCM 1); sprite ST 0..1 over the whole texture',
        pixel_format='RGBA8 rows top to bottom; alpha is the raw GS byte (128 = 1.0; star reaches 255): multiply a normalised sample by gs_alpha_scale',
        textures=textures,sequences=sequences)
    (out/'textures.json').write_text(json.dumps(manifest,indent=1)+'\n')
    print(json.dumps(dict(output=str(out),textures=[(t['id'],t['tag'],t['width'],t['height']) for t in textures])))

if __name__=='__main__':main()
