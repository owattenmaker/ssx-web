"""Locate absolute attribute-cache bases in the verified original ELF."""
from pathlib import Path
import json,struct
import audit_roster as a
refs={};direct=[]
for va,off,size,_ in a.segments:
 for i in range(0,size-4,4):
  word=int.from_bytes(a.elf[off+i:off+i+4],'little')
  if word>>26!=15 or word&65535!=0x53:continue
  reg=(word>>16)&31
  for step in range(1,17):
   if i+step*4+4>size:break
   w=int.from_bytes(a.elf[off+i+step*4:off+i+step*4+4],'little');op=w>>26;rs=(w>>21)&31
   if rs!=reg or w&65535!=0x5538:continue
   at=va+i+step*4
   if op==9:refs[at]=dict(lui=hex(va+i),use=hex(at),opcode=hex(w),pointer_cell='0x535538')
   elif op in (32,33,35,36,37,40,41,43):direct.append(hex(at))
assert not direct,('Unconverted direct accesses',direct)
assert refs
(a.out/'attribute-storage-reference-candidates.json').write_text(json.dumps(list(refs.values()),indent=2)+'\n')
print('Attribute cache base references:',len(refs),'(candidate scan; not an exhaustive alias audit)')
