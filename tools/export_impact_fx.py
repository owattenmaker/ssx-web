#!/usr/bin/env python3
"""Export the original rider impact/contact FX textures (git-ignored outputs).

Texture ids come from the PS2 FX texture table 0x4891B0 (stride 12, packed four-byte tags):
22 `sprk` (rider sparks component RFX+0x470, 2DABC8/2DA390) and 24 `ospk` (attack fist
sparkle RFX+0xC70, 2F1150/2F1510). Both live in DATA/TEXTURES/PARTICLE.SSH. Decoding is the
one used for the snow FX textures (tools/export_snow_assets.py): `.gs.rgba` keeps the raw GS
alpha 0..128, `.rgba`/`.png` double it for viewing.
"""
import argparse,hashlib,importlib.util,json,os,shutil,struct,tempfile,time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
TEXTURES=((22,'sprk','PARTICLE'),(24,'ospk','PARTICLE'))

def load_snow_exporter():
    spec=importlib.util.spec_from_file_location('export_snow_assets',ROOT/'tools/export_snow_assets.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def decode(data,name):
    width,height=struct.unpack_from('<HH',data,4);size=int.from_bytes(data[1:4],'little')
    if data[0]==5 and size==16+width*height*4:return width,height,bytearray(data[16:size])
    if data[0]==2 and size==16+width*height and data[size]==33:
        palette_size=int.from_bytes(data[size+1:size+4],'little');palette=data[size+16:size+palette_size]
        if palette_size<16 or len(palette)!=palette_size-16 or len(palette)%4:raise ValueError('Truncated PS2 FX palette')
        raw=bytearray()
        for index in data[16:size]:
            entry=(index&0xe7)|((index&8)<<1)|((index&16)>>1)   # PSMT8 CLUT swizzle
            if entry*4+4>len(palette):raise ValueError(f'PS2 palette index outside stored CLUT: {name}/{index}')
            raw.extend(palette[entry*4:entry*4+4])
        return width,height,raw
    raise ValueError(f'Unsupported original compact SHPS texture {name}')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    from disc_paths import ps2_iso;p.add_argument('--ps2-iso',type=Path,default=ps2_iso())
    p.add_argument('--output',type=Path,default=ROOT/'local/assets/native/IMPACT_FX')
    p.add_argument('--web',type=Path,default=ROOT/'web/public/assets/IMPACT_FX')
    a=p.parse_args();snow=load_snow_exporter()
    elf=(ROOT/'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest()!=snow.EXPECTED_SHA1:raise ValueError('Unexpected original executable')
    phoff=struct.unpack_from('<I',elf,28)[0];entry_size,count=struct.unpack_from('<HH',elf,42)
    def tag(address):
        for n in range(count):
            kind,offset,vaddr,_,size,_,_,_=struct.unpack_from('<8I',elf,phoff+n*entry_size)
            if kind==1 and vaddr<=address and address+4<=vaddr+size:return elf[offset+address-vaddr:offset+address-vaddr+4]
    a.output.parent.mkdir(parents=True,exist_ok=True);stage=Path(tempfile.mkdtemp(prefix='.impact-fx-',dir=a.output.parent));textures=[]
    disc=snow.Disc(a.ps2_iso)
    try:
        for texture_id,name,archive in TEXTURES:
            if tag(0x4891b0+12*texture_id)!=name.encode('ascii'):raise ValueError(f'Original texture table mismatch for {texture_id}')
            container=disc.file('DATA/TEXTURES/'+archive+'.SSH');offset,data=snow.member(container,name);width,height,raw=decode(data,name)
            if max(raw[3::4])>128:raise ValueError('Unexpected PS2 FX alpha range')
            (stage/(name+'.gs.rgba')).write_bytes(raw)
            view=bytearray(raw)
            for i in range(3,len(view),4):view[i]=min(255,view[i]*2)
            (stage/(name+'.rgba')).write_bytes(view);snow.png(stage/(name+'.png'),width,height,bytes(view))
            textures.append(dict(id=texture_id,name=name,width=width,height=height,file=name+'.gs.rgba',gs_alpha_scale=255/128,
                                 sha256=hashlib.sha256(raw).hexdigest(),source='PS2 DATA/TEXTURES/'+archive+'.SSH',
                                 source_sha256=hashlib.sha256(container).hexdigest(),source_offset=offset,encoding=data[0]))
    finally:disc.close()
    manifest=dict(schema_version=1,texture_table='0x4891B0 stride 12',textures=textures)
    (stage/'impact-fx.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if a.output.exists():shutil.rmtree(a.output)
    os.rename(stage,a.output)
    if a.web:
        a.web.mkdir(parents=True,exist_ok=True)
        for f in ['impact-fx.json']+[t['file'] for t in textures]:shutil.copy2(a.output/f,a.web/f)
    print(json.dumps(dict(output=str(a.output),web=str(a.web),textures=[t['name'] for t in textures])))

if __name__=='__main__':main()
