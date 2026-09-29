#!/usr/bin/env python3
"""Publish verified original snow sprite view constraints."""
import hashlib,json,struct,zipfile
from pathlib import Path
from extract_fx_microcode import extract_programs
ROOT=Path(__file__).resolve().parents[1]
def export():
 code,_=extract_programs((ROOT/'local/disc/SLUS_207.72').read_bytes())[4]
 if struct.unpack_from('<2I',code,0xd98)!=(0x43000000,0x81fdce69) or struct.unpack_from('<I',code,0xda4)[0]!=0x0180e71f:raise ValueError('Original particle pixel clamp differs')
 sizes=[]
 for name in ['glide','jump-31','jump-90']:
  with zipfile.ZipFile(ROOT/f'local/reference/pcsx2/snow-jam-{name}.p2s')as archive:m=archive.read('eeMemory.bin')
  renderer=struct.unpack_from('<I',m,0x4a30f0-0x854)[0];sizes.append(struct.unpack_from('<2I',m,renderer+0x5a30))
 if len(set(sizes))!=1:raise ValueError('Reference source viewport varies')
 view=dict(source_viewport=list(sizes[0]),max_projected_half_extent=128,program_sha256=hashlib.sha256(code).hexdigest(),source='Program4 D98 I=128; DA0 MINIi.xy after projected-size ABS; Snow Jam viewport in three baseline states')
 for base in ['local/assets/native/SNOW_FX','web/public/assets/SNOW_FX']:
  path=ROOT/base/'snow-fx.json';j=json.loads(path.read_text());j['view']=view;path.write_text(json.dumps(j,indent=2)+'\n')
 print(json.dumps(view))
if __name__=='__main__':export()
