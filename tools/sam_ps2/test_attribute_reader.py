"""Execute the full original raw-attribute getter function with patches."""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
from attribute_read_hook import SITES,WORDS,CELL,hook,patch
root=Path(__file__).resolve().parents[2];elf=(root/'local/disc/SLUS_207.72').read_bytes()
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
def read(at,n):
 for i in range(phnum):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError(hex(at))
def scalar(code):
 words=list(struct.unpack('<%dI'%(len(code)//4),code))
 for i,w in enumerate(words):
  op=w>>26
  if w&0xfc00003f==0x2d:words[i]=w-12
  elif op in (30,55):words[i]=(w&0x03ffffff)|(35<<26) # LQ/LD scalar test restores
  elif op in (31,63):words[i]=(w&0x03ffffff)|(43<<26) # SQ/SD scalar test saves
 return struct.pack('<%dI'%len(words),*words)
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xc00000)
u.mem_write(0x148158,scalar(read(0x148158,0x178)));u.mem_write(0x45a290,read(0x45a290,28))
u.mem_write(0x900000,hook());u.mem_write(CELL,struct.pack('<I',0x900000));u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
for site in SITES:
 assert read(site,16)==struct.pack('<4I',*WORDS);u.mem_write(site,patch())
u.mem_write(0x14a080,struct.pack('<2I',0x03e00008,0))
character=0
def ee(vm,at,size,user):
 if at==0x14a080:vm.reg_write(UC_MIPS_REG_V0,character)
 w=struct.unpack('<I',vm.mem_read(at,4))[0]
 if w&0xfc00003f==0x18:
  rs,rt,rd=(w>>21)&31,(w>>16)&31,(w>>11)&31
  value=vm.reg_read(UC_MIPS_REG_0+rs)*vm.reg_read(UC_MIPS_REG_0+rt)
  vm.reg_write(UC_MIPS_REG_LO,value&0xffffffff);vm.reg_write(UC_MIPS_REG_HI,value>>32)
  if rd:vm.reg_write(UC_MIPS_REG_0+rd,value&0xffffffff)
  vm.reg_write(UC_MIPS_REG_PC,at+4)
u.hook_add(UC_HOOK_CODE,ee)
cases=0
for profile in range(3):
 for character in range(32):
  for stat in range(7):
   old=0x4a6ca8+profile*0x9b50+character*0xf88;sam=0xa00000+profile*0xf88
   u.mem_write(old+0xbdf,bytes(range(5,12)));u.mem_write(sam+0xbdf,bytes(range(35,42)))
   u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,stat);u.reg_write(UC_MIPS_REG_RA,0x100000);u.reg_write(UC_MIPS_REG_SP,0xc10000)
   u.reg_write(UC_MIPS_REG_S0,0x12345);u.reg_write(UC_MIPS_REG_S1,0x56789)
   u.emu_start(0x148158,0x100000,count=150)
   assert u.reg_read(UC_MIPS_REG_PC)==0x100000
   assert u.reg_read(UC_MIPS_REG_V0)==(35 if character==30 else 5)+stat
   assert u.reg_read(UC_MIPS_REG_SP)==0xc10000 and u.reg_read(UC_MIPS_REG_S0)==0x12345 and u.reg_read(UC_MIPS_REG_S1)==0x56789
   assert bytes(u.mem_read(old+0xbdf,7))==bytes(range(5,12));assert bytes(u.mem_read(sam+0xbdf,7))==bytes(range(35,42))
   cases+=1
print(cases,'full seven-stat reader cases passed; character resolver mocked, original selection/loads/returns executed.')
