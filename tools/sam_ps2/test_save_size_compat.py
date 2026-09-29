"""Only the explicit legacy/new profile-size pair gains compatibility."""
import struct,sys
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from profile_hooks import wardrobe_name_budget
cases=0
for caller in [0x2c5b00,0x2c5d9c]:
 for expected in [0xccc,0x9b61,0xab0c,0x80000]:
  for actual in [0,0xccc,0x9b61,0xab0b,0xab0c,0x80000,0xffffffff]:
   u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
   for base in [0x140000,0x2c0000,0x4a0000,0x500000,0x530000,0x900000]:u.mem_map(base,0x10000)
   u.mem_write(caller,struct.pack('<2I',0x0c052726,0));u.mem_write(0x149c98,struct.pack('<4I',0x3c190053,0x8f390a10,0x03200008,0))
   enabled='--save-extension' in sys.argv
   u.mem_write(0x530a10,struct.pack('<I',0x900000));u.mem_write(0x900000,wardrobe_name_budget(enabled))
   u.mem_write(0x530a30,struct.pack('<I',0x12345678))
   u.mem_write(0x500024,struct.pack('<I',expected));u.mem_write(0x500020,struct.pack('<I',expected))
   u.mem_write(0x500490,struct.pack('<I',actual));u.mem_write(0x4a3940,struct.pack('<I',actual))
   u.reg_write(UC_MIPS_REG_S1,0x500000);u.reg_write(UC_MIPS_REG_GP,0x4a30f0);u.reg_write(UC_MIPS_REG_SP,0x508000)
   u.emu_start(caller,caller+8,count=100)
   assert u.reg_read(UC_MIPS_REG_V1)==actual
   assert (u.reg_read(UC_MIPS_REG_V0)==actual)==(actual==expected or expected==0xab0c and actual==0x9b61)
   assert u.reg_read(UC_MIPS_REG_S1)==0x500000 and u.reg_read(UC_MIPS_REG_SP)==0x508000
   assert struct.unpack('<I',u.mem_read(0x530a30,4))[0]==(actual if enabled and caller==0x2c5d9c and expected==0xab0c else 0x12345678)
   cases+=1
print(cases,'driver size/read-count cases passed; errors and other sizes retain their original outcomes.')
