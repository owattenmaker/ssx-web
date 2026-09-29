"""Run the exact patched call sites and shared trampoline for preview gating."""
import struct
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from profile_hooks import wardrobe_name_budget
cases=0
for caller in [0x182f08,0x184cb0,0x19c040]:
 for character in [*range(32),0xffffffff]:
  u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
  for base in [0x140000,0x180000,0x190000,0x500000,0x530000,0x900000]:u.mem_map(base,0x10000)
  u.mem_write(caller,struct.pack('<2I',(3<<26)|(0x149c98>>2),0))
  u.mem_write(0x149c98,struct.pack('<4I',0x3c190053,0x8f390a10,0x03200008,0))
  u.mem_write(0x530a10,struct.pack('<I',0x900000));u.mem_write(0x900000,wardrobe_name_budget())
  u.mem_write(0x500000,struct.pack('<I',character));u.reg_write(UC_MIPS_REG_S6,0x500000);u.reg_write(UC_MIPS_REG_SP,0x508000)
  s4=0x500000 if caller==0x19c040 else 0x501000
  s6=0x501000 if caller==0x19c040 else 0x500000
  u.mem_write(0x501000,struct.pack('<I',15))
  u.reg_write(UC_MIPS_REG_S4,s4);u.reg_write(UC_MIPS_REG_S6,s6)
  u.emu_start(caller,caller+8,count=100)
  assert u.reg_read(UC_MIPS_REG_V0)==int(character<10 or character==30)
  assert u.reg_read(UC_MIPS_REG_S6)==s6 and u.reg_read(UC_MIPS_REG_SP)==0x508000
  assert u.reg_read(UC_MIPS_REG_S4)==s4
  cases+=1
print(cases,'preview eligibility cases passed; original bonus IDs remain excluded and Sam is admitted.')
