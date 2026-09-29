#!/usr/bin/env python3
"""Export original116950 trick-name tables from the owned reference state."""
import hashlib,json,struct,zipfile
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
ROOT=Path(__file__).resolve().parents[1]
TABLES=[(0x43d118,6),(0x43d130,12),(0x43cf40,8),(0x43cf60,4),(0x43cf70,15),(0x43cfb0,6),(0x43cfc8,5),(0x43cfe0,56),(0x43d0c0,16),(0x43d160,86),(0x4a0ff0,2),(0x43d160,86),(0x43d2b8,6),(0x43d2d0,4),(0x43d2e0,16),(0x43d100,6),(0x43d320,26)]
def extract():
 elf=(ROOT/'local/disc/SLUS_207.72').read_bytes()
 if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected executable')
 with zipfile.ZipFile(ROOT/'local/reference/pcsx2/snow-jam-glide.p2s') as z:ram=z.read('eeMemory.bin')
 tables=[]
 for address,count in TABLES:
  table=[]
  for i in range(count):
   pointer=struct.unpack_from('<I',ram,address+4*i)[0]
   if not pointer:table.append(None);continue
   if not 0x430000<=pointer<0x4b0000:raise ValueError('Unexpected trick-name pointer')
   end=ram.find(b'\0',pointer,pointer+256)
   if end<0:raise ValueError('Unterminated trick name')
   table.append(ram[pointer:end].decode('ascii'))
  tables.append(table)
 return dict(version=1,tables=tables,source_tables=[hex(a) for a,_ in TABLES],elf_sha256=hashlib.sha256(elf).hexdigest(),ram_sha256=hashlib.sha256(ram).hexdigest())
if __name__=='__main__':
 p=ROOT/'local/browser-ui/trick-names.json';p.write_text(json.dumps(extract(),indent=2)+'\n');print(p)
