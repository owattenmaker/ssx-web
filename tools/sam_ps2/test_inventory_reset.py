"""Execute the full original all-rider equipment reset with patches."""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
from inventory_reset_hook import SITE,CELL,WORDS,hook,patch
root=Path(__file__).resolve().parents[2];elf=(root/'local/disc/SLUS_207.72').read_bytes()
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
def read(at,n):
 for i in range(phnum):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError(hex(at))
def scalar(code):
 words=struct.unpack('<%dI'%(len(code)//4),code)
 return struct.pack('<%dI'%len(words),*[w-12 if w&0xfc00003f==0x2d else w for w in words])
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xb00000)
body=bytearray(scalar(read(0x14af10,0xa0)));mults={}
for offset in range(0,len(body),4):
 w=struct.unpack_from('<I',body,offset)[0]
 if w&0xfc00003f==0x18:
  mults[0x14af10+offset]=w
  struct.pack_into('<I',body,offset,w&~(31<<11))
u.mem_write(0x14af10,bytes(body));assert read(SITE,16)==struct.pack('<4I',*WORDS)
u.mem_write(SITE,patch());u.mem_write(0x14af88,struct.pack('<I',0x2862000b))
u.mem_write(CELL,struct.pack('<I',0x900000));u.mem_write(0x900000,hook());u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
pending=None
# EE MULT in original branch delay slots cannot be emulated by skipping PC:
# execute ordinary MULT natively, then apply its extra rd result at next fetch.
def ee(vm,at,size,user):
 global pending
 if pending is not None:
  reg,value=pending;vm.reg_write(UC_MIPS_REG_0+reg,value);pending=None
 if at in mults:
  w=mults[at];rs,rt,rd=(w>>21)&31,(w>>16)&31,(w>>11)&31
  value=vm.reg_read(UC_MIPS_REG_0+rs)*vm.reg_read(UC_MIPS_REG_0+rt)
  pending=(rd,value&0xffffffff)
u.hook_add(UC_HOOK_CODE,ee)
records=0
for fixture in range(6):
 expected={}
 for profile in range(3):
  for character in [*range(10),30]:
   ptr=0xa00000+profile*0xf88 if character==30 else 0x4a6ca8+profile*0x9b50+character*0xf88
   count=0 if fixture==4 else 443 if fixture==5 else [0,1,3,443][(character+profile+fixture)%4]
   data=bytearray([0xa5])*0xf88;struct.pack_into('<I',data,0x28c,count)
   for item in range(count):struct.pack_into('<2H',data,0x290+item*4,item,[0,4,16,20,0x40,0xffff][item%6])
   u.mem_write(ptr,bytes(data))
   for item in range(count):
    at=0x292+item*4;flags=struct.unpack_from('<H',data,at)[0]
    struct.pack_into('<H',data,at,flags|16 if flags&4 else flags&~16)
   expected[ptr]=bytes(data)
 guards=[0x4a6c98,0x4c3e98,0x9ffff0,0xa02e98]
 for guard in guards:u.mem_write(guard,bytes([0x6c])*16)
 pending=None;u.reg_write(UC_MIPS_REG_RA,0x100000);u.reg_write(UC_MIPS_REG_SP,0xb10000)
 u.emu_start(0x14af10,0x100000,count=250000)
 assert u.reg_read(UC_MIPS_REG_PC)==0x100000
 assert u.reg_read(UC_MIPS_REG_SP)==0xb10000
 for ptr,data in expected.items():assert bytes(u.mem_read(ptr,0xf88))==data,(fixture,hex(ptr))
 for guard in guards:assert bytes(u.mem_read(guard,16))==bytes([0x6c])*16
 records+=len(expected)
print('6 complete reset passes verified',records,'original/Sam records, including empty/full inventories, byte preservation and boundary guards.')
