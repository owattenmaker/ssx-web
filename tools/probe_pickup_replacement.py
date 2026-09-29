#!/usr/bin/env python3
"""Verify type19 replacement's update/draw/contact methods in the original ELF."""
import hashlib,json,struct
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
p=Path('local/disc/SLUS_207.72');elf=p.read_bytes()
if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected SSX3 executable')
phoff=struct.unpack_from('<I',elf,28)[0];stride,count=struct.unpack_from('<HH',elf,42)
def read(address,size):
 for i in range(count):
  typ,offset,base,_,length,_,_,_=struct.unpack_from('<8I',elf,phoff+i*stride)
  if typ==1 and base<=address and address+size<=base+length:return elf[offset+address-base:offset+address-base+size]
 raise ValueError('ELF address not file-backed')
vtable=0x491680;methods=[]
for slot,target in [(0x10,0x3608e8),(0x20,0x360790),(0x78,0x360910),(0x140,0x3609f0)]:
 raw=read(vtable+slot,8);adjust=struct.unpack_from('<h',raw)[0];actual=struct.unpack_from('<I',raw,4)[0]
 if adjust!=0 or actual!=target:raise ValueError('Unexpected type19 dispatch')
 words=list(struct.unpack('<2I',read(target,8)))
 if slot in [0x20,0x140] and words!=[0x03e00008,0]:raise ValueError('Replacement callback is not empty')
 if slot==0x78 and words!=[0x03e00008,0x24020001]:raise ValueError('Replacement update does not return1')
 methods.append(dict(slot=hex(slot),target=hex(target),adjustment=adjust,first_words=[hex(w) for w in words]))
debounce_contact=struct.unpack_from('<I',read(0x4906f0+0x144,4))[0]
if debounce_contact!=0x3614f8 or struct.unpack('<2I',read(debounce_contact,8))!=(0x03e00008,0):raise ValueError('Debounce contact is not empty')
out=Path('local/browser-pickups/replacement-dispatch.json');out.write_text(json.dumps(dict(elf_sha256=hashlib.sha256(elf).hexdigest(),vtable=hex(vtable),methods=methods,debounce_contact=hex(debounce_contact),interpretation='Type19 has no draw or contact dispatch work, and its delegated update returns1. This does not prove what later stage reset/destruction code does.'),indent=2)+'\n');print(out)
