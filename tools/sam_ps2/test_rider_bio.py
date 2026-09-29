"""Exercise new biography pointer routing for all original riders and Sam."""
import struct
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from rider_bio import table_hook
cases=0
for kind,rows,stride,cell in [('DNA',8,32,0x530a14),('FAVES',12,48,0x530a18),('QNA',4,16,0x530a1c)]:
 for character in [*range(10),30]:
  for row in range(rows):
   u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
   for base,size in [(0x100000,0x1000),(0x500000,0x10000),(0x530000,0x1000)]:u.mem_map(base,size)
   u.mem_write(0x100000,table_hook(kind));u.mem_write(0x500048,struct.pack('<I',character));u.mem_write(cell,struct.pack('<I',0x505000))
   for reg,value in [(UC_MIPS_REG_S4,0x500000),(UC_MIPS_REG_S5,48),(UC_MIPS_REG_V1,row*4),(UC_MIPS_REG_SP,0x501000),(UC_MIPS_REG_RA,0x100800)]:u.reg_write(reg,value)
   u.emu_start(0x100000,0x100800,count=100)
   assert u.reg_read(UC_MIPS_REG_V1)==(0x505000+row*4 if character==30 else 0x501000+character*stride+row*4)
   assert u.reg_read(UC_MIPS_REG_SP)==0x501000 and u.reg_read(UC_MIPS_REG_S4)==0x500000 and u.reg_read(UC_MIPS_REG_S5)==48
   cases+=1
print(cases,'biography lookups passed: original stack-table addresses unchanged, Sam uses private tables.')
