"""Execute the full original per-mission flag functions function with patches."""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
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

from mission_flag_hooks import CELL,WORDS,READERS,SETTERS,SITES,hook
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xc00000)
for at,_ in READERS:u.mem_write(at,scalar(read(at,0x58)))
for at,_ in SETTERS:u.mem_write(at,scalar(read(at,0x70)))
u.mem_write(0x900000,hook());u.mem_write(CELL,struct.pack('<I',0x900000));u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
for site in SITES:
 assert read(site,16)==struct.pack('<4I',*WORDS)
 u.mem_write(site,struct.pack('<4I',0x3c190053,0x8f390b78,0x0320f809,0))
u.mem_write(0x154240,scalar(read(0x154240,0x38)))
u.mem_write(0x43ee10,read(0x43ee10,88*36))
mission_ids=[struct.unpack('<I',read(0x43ee10+i*36,4))[0] for i in range(88)]
u.mem_write(0x14a080,struct.pack('<2I',0x03e00008,0))
character=slot=0
def resolve(vm,at,size,user):
 if at==0x14a080:vm.reg_write(UC_MIPS_REG_V0,character)
u.hook_add(UC_HOOK_CODE,resolve)
def run(at,value=0):
 u.reg_write(UC_MIPS_REG_A1,mission_ids[slot]);u.reg_write(UC_MIPS_REG_A2,value);u.reg_write(UC_MIPS_REG_RA,0x100000);u.reg_write(UC_MIPS_REG_SP,0xc10000)
 u.reg_write(UC_MIPS_REG_S0,0x123456);u.reg_write(UC_MIPS_REG_S1,0xabcdef)
 u.emu_start(at,0x100000,count=2000)
 assert u.reg_read(UC_MIPS_REG_PC)==0x100000 and u.reg_read(UC_MIPS_REG_SP)==0xc10000
 assert u.reg_read(UC_MIPS_REG_S0)==0x123456 and u.reg_read(UC_MIPS_REG_S1)==0xabcdef
 return u.reg_read(UC_MIPS_REG_V0)
reads=writes=0
for character in range(32):
 old=0x4a6ca8+character*0xf88;sam=0xa00000
 for slot in [0,1,7,25,87]:
  assert mission_ids.index(mission_ids[slot])==slot
  offset=0x118+slot*4
  for flags in [0,1,2,4,8,16,0xffffffff,0xa5a5a5a5]:
   a=bytearray([0x5a])*0xf88;b=bytearray([0xa5])*0xf88
   struct.pack_into('<I',a,offset,flags);struct.pack_into('<I',b,offset,flags^31)
   u.mem_write(old,bytes(a));u.mem_write(sam,bytes(b))
   for at,bit in READERS:
    assert run(at)==(((flags^31) if character==30 else flags)>>bit)&1
    assert bytes(u.mem_read(old,0xf88))==bytes(a) and bytes(u.mem_read(sam,0xf88))==bytes(b)
    reads+=1
   for at,bit in SETTERS:
    for value in [0,1,2,0xffffffff]:
     u.mem_write(old,bytes(a));u.mem_write(sam,bytes(b))
     expected=bytearray(b if character==30 else a);base_flags=flags^31 if character==30 else flags
     new_flags=(base_flags&~(1<<bit))|((value&1)<<bit);struct.pack_into('<I',expected,offset,new_flags)
     assert run(at,value)==new_flags
     assert bytes(u.mem_read(old,0xf88))==(bytes(a) if character==30 else bytes(expected))
     assert bytes(u.mem_read(sam,0xf88))==(bytes(expected) if character==30 else bytes(b))
     writes+=1
print(reads,'full mission-status reads and',writes,'full setter executions passed; other bits, slots, fields and riders preserved.')
