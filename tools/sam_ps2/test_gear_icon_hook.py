"""Compare original icon archive getter and Sam extension for every rider ID."""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from gear_icon_hook import SITE,CELL,PATH_CELL,PATH,hook
elf=(Path(__file__).resolve().parents[2]/'local/disc/SLUS_207.72').read_bytes()
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
def read(at,n):
 for i in range(phnum):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError(hex(at))
def scalar(b):return b.replace(struct.pack('<I',0x0000102d),struct.pack('<I',0x00001021))
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0x900000)
original=read(SITE,0xa8);u.mem_write(SITE,scalar(original));u.mem_write(0x45a5c0,read(0x45a5c0,40));u.mem_write(0x900000,scalar(hook(original)));u.mem_write(PATH_CELL,struct.pack('<I',0x901000));u.mem_write(0x901000,PATH)
for char in [*range(32),0xffffffff]:
 results=[]
 for start in [SITE,0x900000]:
  u.reg_write(UC_MIPS_REG_A1,char);u.reg_write(UC_MIPS_REG_RA,0x100000)
  u.emu_start(start,0x100000,count=100)
  assert u.reg_read(UC_MIPS_REG_PC)==0x100000 and u.reg_read(UC_MIPS_REG_A1)==char
  results.append(u.reg_read(UC_MIPS_REG_V0))
 assert results[1]==(0x901000 if char==30 else results[0])
 if char==30:assert bytes(u.mem_read(results[1],len(PATH)))==PATH
print('33 icon archive getter cases passed; original archive pointers and bonus-ID behavior preserved.')
