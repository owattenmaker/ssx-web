#!/usr/bin/env python3
"""Publish owned original snow FX textures and verified authored parameters."""
import argparse,hashlib,json,os,re,struct,tempfile,time,zlib
from pathlib import Path
from inspect_disc import Disc,EXPECTED_SHA1
from rider_assets import decode_rider_texture

ORDER=('SnowTrail','LargeChunkySpray','SmallChunkySpray','RockSpray','RiderBreath','LargeImpact','SmallImpact','CloudySpray','Kicker','BodySnow')
FIELDS=('NumParticles','NumBlur','Duration','Damp','Size','Life','SizeR','LifeR','BlurStep',
        'OffX','OffY','OffZ','R0X','R0Y','R0Z','R1X','R1Y','R1Z','VelX','VelY','VelZ',
        'R0VX','R0VY','R0VZ','R1VX','R1VY','R1VZ','R2VX','R2VY','R2VZ','ForceX','ForceY','ForceZ',
        'StartColA','StartColR','StartColG','StartColB','EndColA','EndColR','EndColG','EndColB',
        'R0A','R0R','R0G','R0B','R1A','R1R','R1G','R1B','TextureId','BlendMode','SizeFinal',
        'NumFlipTextures','FlipTextureRate','VelScale','NormScale','NormSpeedScale','SideScale')

def png(path,width,height,rgba):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    rows=b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))

def member(data,name):
    endian='>' if data[:4]==b'SHPG' else '<' if data[:4]==b'SHPS' else None
    if endian is None:raise ValueError('Invalid original texture container')
    count=struct.unpack_from(endian+'I',data,8)[0]
    for i in range(count):
        if data[16+i*8:20+i*8].decode('ascii')==name:
            at=struct.unpack_from(endian+'I',data,20+i*8)[0]
            return at,data[at:]
    raise ValueError(f'Missing original texture {name}')

def main():
    from disc_paths import ps2_iso;p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ps2-iso',type=Path,default=ps2_iso());p.add_argument('--output',type=Path,default=Path('local/assets/native/SNOW_FX'));p.add_argument('--original-defaults',type=Path,default=Path('build/snow-emission-oracle/original-dynamic-spray-profiles.bin'));a=p.parse_args()
    elf=Path('local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original executable')
    original=a.original_defaults.read_bytes()
    if len(original)!=10*232:raise ValueError('Run test_snow_emission_native.py to recover original defaults first')
    header=Path('local/gamecube/disc/files/data/confman/cmrender.h');config={}
    for name,kind,value in re.findall(r'//__CFM__: Render_DynamicSpray_(\S+) ([123]) ([^\r\n]+)',header.read_text()):
        group,key=name.split('_',1);kind=int(kind)
        config.setdefault(group,{})[key]=(kind,int(value) if kind==1 else struct.unpack('<f',struct.pack('<f',float(value)))[0])
    profiles=[]
    for index,name in enumerate(ORDER):
        values={}
        for n,key in enumerate(FIELDS):
            kind,value=config[name][key];packed=struct.pack('<i' if kind==1 else '<f',value)
            if original[index*232+n*4:index*232+n*4+4]!=packed:raise ValueError(f'Original PS2/GC configuration differs: {name}.{key}')
            values[key]=value
        profiles.append(dict(name=name,emitter_index=index,parameters=values))
    output=a.output;output.parent.mkdir(parents=True,exist_ok=True);stage=Path(tempfile.mkdtemp(prefix='.snow-fx-',dir=output.parent));textures=[]
    disc=Disc(a.ps2_iso)
    try:
        sources={name:disc.file('DATA/TEXTURES/'+name+'.SSH')for name in ('PARTICLE','EFFECTS')}
        for texture_id,name,archive in ((5,'spry','PARTICLE'),(6,'impt','PARTICLE'),(25,'brth','PARTICLE'),(55,'btrl','EFFECTS'),(56,'wake','EFFECTS'),(57,'yrbn','EFFECTS'),(58,'orbn','EFFECTS'),(59,'rrbn','EFFECTS'),(60,'brbn','EFFECTS'),(61,'prbn','EFFECTS')):
            #4891B0 uses packed four-byte tags, not string pointers.
            address=0x4891b0+12*texture_id;tag=None
            phoff=struct.unpack_from('<I',elf,28)[0];entry_size,count=struct.unpack_from('<HH',elf,42)
            for n in range(count):
                kind,offset,vaddr,_,size,_,_,_=struct.unpack_from('<8I',elf,phoff+n*entry_size)
                if kind==1 and vaddr<=address and address+4<=vaddr+size:tag=elf[offset+address-vaddr:offset+address-vaddr+4]
            if tag!=name.encode('ascii'):raise ValueError(f'Original texture table mismatch for{texture_id}: {tag!r}')
            container=sources[archive];offset,data=member(container,name);width,height=struct.unpack_from('<HH',data,4);size=int.from_bytes(data[1:4],'little')
            if data[0]==5 and size==16+width*height*4:raw=bytearray(data[16:size])
            elif data[0]==2 and size==16+width*height and data[size]==33:
                palette_size=int.from_bytes(data[size+1:size+4],'little')
                palette=data[size+16:size+palette_size]
                if palette_size<16 or len(palette)!=palette_size-16 or len(palette)%4:raise ValueError('Truncated PS2 FX palette')
                raw=bytearray()
                for index in data[16:size]:
                    entry=(index&0xe7)|((index&8)<<1)|((index&16)>>1)
                    if entry*4+4>len(palette):raise ValueError(f"PS2 palette index outside stored CLUT: {name}/{index}")
                    raw.extend(palette[entry*4:entry*4+4])
            else:raise ValueError('Unsupported original compact SHPS texture')
            (stage/(name+'.gs.rgba')).write_bytes(raw);maximum=max(raw[3::4])
            if maximum>128:raise ValueError('Unexpected PS2 FX alpha range')
            for i in range(3,len(raw),4):raw[i]=min(255,raw[i]*2)
            rgba=bytes(raw);(stage/(name+'.rgba')).write_bytes(rgba);png(stage/(name+'.png'),width,height,rgba)
            textures.append(dict(id=texture_id,name=name,width=width,height=height,file=name+'.rgba',sha256=hashlib.sha256(rgba).hexdigest(),source='PS2 DATA/TEXTURES/'+archive+'.SSH',source_sha256=hashlib.sha256(container).hexdigest(),source_offset=offset,encoding=data[0],gs_alpha_file=name+'.gs.rgba',gs_alpha_scale=255/128,alpha_conversion='GS0..128 toRGBA0..255 (double,saturate)'))
    finally:disc.close()
    gc=Path('local/gamecube/disc/files/data/textures/particle.gsh');container=gc.read_bytes();offset,data=member(container,'tmb1');width,height,rgba=decode_rider_texture(data)
    (stage/'tmb1.rgba').write_bytes(rgba);png(stage/'tmb1.png',width,height,rgba);textures.append(dict(id=14,name='tmb1',width=width,height=height,file='tmb1.rgba',sha256=hashlib.sha256(rgba).hexdigest(),source=str(gc),source_sha256=hashlib.sha256(container).hexdigest(),source_offset=offset,encoding=data[0],alpha_conversion='Original GX RGB5A3 toRGBA'))
    manifest=dict(schema_version=1,coordinate_system='Original source centimeters, Z-up; convert only at native renderer boundary',textures=textures,profiles=profiles,
        verified=dict(executable_sha1=EXPECTED_SHA1,parameter_header=str(header),parameter_header_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),ps2_defaults_sha256=hashlib.sha256(original).hexdigest(),matching_parameter_words=580,constructor='2DE4A8..2DF178',texture_id_table='4891B0, stride12'),
        runtime=dict(emission='snow_emission matches2DF920 prefix,2E0EE8+2DE398 SnowTrail,2E02B8 chunks,2E1598 impact,2E1A80 cloudy spray',effects_lcg='gp+A0C',particle_random='3177F0 uses6-word4FF018, separate from gameplay4FF030',particle_evaluation='snow_particles plain-native A00 equations; CPUcoefficients and20,000births/2,000VUcases verified',remaining='Other emitter categories, fullscene FXscheduling, viewpixelclamp/fog require joinedvalidation'))
    (stage/'snow-fx.json').write_text(json.dumps(manifest,indent=2)+'\n');(stage/'original-profiles.bin').write_bytes(original)
    if output.exists():os.rename(output,output.with_name(output.name+'.previous-'+str(time.time_ns())))
    os.rename(stage,output);print(json.dumps(dict(output=str(output),textures=len(textures),profiles=len(profiles),verified_parameter_words=580)))

if __name__=='__main__':main()
