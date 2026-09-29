"""Guarded cache-bank/allocation expansion from reviewed address provenance."""
from pathlib import Path
import json,struct
import audit_roster as a
folder=a.out
r=json.loads((folder/'graphics-cache-candidates.json').read_text());patches={}
def patch(at,new,purpose):
 old=a.read(at,4);entry=dict(address=hex(at),expected=old.hex(),replacement=struct.pack('<I',new).hex(),purpose=purpose)
 if at in patches:assert patches[at]['replacement']==entry['replacement']
 patches[at]=entry
for e in r['constant_candidates']:
 lui,use=int(e['lui'],16),int(e['use'],16);new=int(e['new'],16);word=int(e['opcode'],16)
 assert a.read(use,4)==struct.pack('<I',word)
 upper=int.from_bytes(a.read(lui,4),'little');assert upper>>26==15 and upper&65535==11
 # All enlarged fields remain below a signed 16-bit offset of highword24.
 assert new>>16==0x24 and new&65535<32768
 patch(lui,(upper&0xffff0000)|(new>>16),'Relocate expanded cache/view metadata highword')
 patch(use,(word&0xffff0000)|(new&65535),'Relocate cache/view field or allocation size')
for e in r['loop_candidates']:
 pc=int(e['address'],16);old=int(e['opcode'],16);assert old>>26==10 and old&65535==10
 patch(pc,(old&0xffff0000)|32,'Iterate all 32 model cache banks')
# Verify offsets and padding do not collide for any bank, slot, or counter.
assert 32*256*0x120==0x240000
assert 0x240008+32*4<=0x240090
assert 0x1a70+0x240090==0x241b00
assert 0x241b3c+4<=r['new_view_size']
result=dict(bank_count=32,bank_stride=0x12000,view_size=r['new_view_size'],patches=list(patches.values()),limitations='Instruction/layout checks only; all allocation callers and runtime behavior still require validation')
(folder/'graphics-cache-patches.json').write_text(json.dumps(result,indent=2)+'\n');print(len(patches),'guarded code changes; view allocation',hex(r['new_view_size']))
