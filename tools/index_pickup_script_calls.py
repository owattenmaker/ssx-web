"""Index source-confirmed contact builtin encodings in ARA1 stage data."""
from pathlib import Path
import json,struct,sys,hashlib
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from world_assets import world_chunks,records
elf=(root/'local/disc/SLUS_207.72').read_bytes();word=lambda addr:struct.unpack_from('<I',elf,addr-0xff000)[0]
assert word(0x4797b0+0x21*4)==0x223f58
assert word(0x441f38+0x41*4)==0x301d78
programs=json.loads((root/'local/browser-pickups/ara1-scripts.json').read_text())['programs']
calls=[]
for chunk,body in enumerate(world_chunks(root/'local/assets/source/ps2/bam.ssb')):
 if chunk!=33:continue
 for kind,track,rid,data in records(body):
  if kind!=16 or track!=8:continue
  for offset in range(0,len(data)-3,4):
   op=struct.unpack_from('<I',data,offset)[0]
   if op&255!=0x21 or (op>>16)&255!=0x41:continue
   program=next((p for p in programs if p['code_start']<=offset<p['code_end']),None)
   assert program is not None, 'Contact candidate outside a LUN bytecode region'
   context=[dict(offset=p,word=struct.unpack_from('<I',data,p)[0]) for p in range(max(0,offset-16),min(len(data)-3,offset+24),4)]
   calls.append(dict(program=program['index'],program_offset=offset-program['code_start'],chunk=chunk,kind=kind,track=track,rid=rid,offset=offset,word=op,destination=(op>>8)&255,builtin=(op>>16)&255,argument_count=op>>24,context=context))
assert len(calls)==17
result=dict(elf_sha256=hashlib.sha256(elf).hexdigest(),opcode=0x21,builtin=0x41,dispatch=0x223f58,table=0x441f38,target=0x301d78,calls=calls,scope='Raw stage call candidates; bytecode block boundaries and pickup-specific association not yet verified')
(root/'local/browser-pickups/contact-script-calls.json').write_text(json.dumps(result,indent=2)+'\n')
print('Verified contact builtin dispatch and indexed17 ARA1 stage call candidates')
