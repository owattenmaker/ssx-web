"""Candidate member-offset audit for the fixed-size PS2 wardrobe database."""
import json,struct
from pathlib import Path
import audit_roster as a
OLD_BASES=[0x2c+120*i for i in range(11)]
NEW_BASES=[0x2c+128*i for i in range(11)]
changes=dict(zip(OLD_BASES,NEW_BASES));changes[0x554]=0x5ac
members=[];counts=[]
for at in range(0x14be70,0x14dc80,4):
 word=int.from_bytes(a.read(at,4),'little');op=word>>26;rs=(word>>21)&31;imm=word&65535
 if op in (9,32,33,35,36,37,40,41,43) and rs not in (0,28,29) and imm in changes and changes[imm]!=imm:
  members.append(dict(address=hex(at),expected=struct.pack('<I',word).hex(),replacement=struct.pack('<I',(word&0xffff0000)|changes[imm]).hex(),old_offset=hex(imm),new_offset=hex(changes[imm]),base_register=rs))
 if op==9 and rs==0 and imm in (29,30,90,120):counts.append(dict(address=hex(at),word=hex(word),value=imm))
globals=[]
for va,off,size,_ in a.segments:
 for i in range(0,size-4,4):
  w=int.from_bytes(a.elf[off+i:off+i+4],'little')
  if w>>26!=15 or w&65535!=0x4a:continue
  reg=(w>>16)&31
  for n in range(1,17):
   if i+n*4+4>size:break
   use=int.from_bytes(a.elf[off+i+n*4:off+i+n*4+4],'little')
   if use>>26==9 and (use>>21)&31==reg and use&65535==0x6750:
    globals.append(dict(lui=hex(va+i),address=hex(va+i+n*4),expected=struct.pack('<I',use).hex(),replacement=struct.pack('<I',(use&0x03ffffff)|(35<<26)).hex()))
result=dict(original_base='0x4a6750',old_array_bases=list(map(hex,OLD_BASES)),new_array_bases=list(map(hex,NEW_BASES)),new_allocation_bytes=0x5b0,member_candidates=members,count_candidates=counts,global_candidates=globals,note='Requires reference/lifetime review before application; not a proven complete patch list')
(a.out/'bolt-layout-candidates.json').write_text(json.dumps(result,indent=2)+'\n');print(len(members),'member candidates;',len(globals),'global candidates; counters:',counts)
