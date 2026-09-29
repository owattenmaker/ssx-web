"""Execute all nine upgrade routing sites without changing purchase semantics."""
import struct
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from upgrade_hooks import SITES,hook,patch_words
cases=0
for site,cell,kind,words in SITES:
 for profile in range(3):
  for char in range(32):
   u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
   for base in [0x150000,0x530000,0x900000]:u.mem_map(base,0x10000)
   u.mem_write(site,patch_words(cell));u.mem_write(cell,struct.pack('<I',0x900000));u.mem_write(0x5309fc,struct.pack('<I',0xa00000));u.mem_write(0x900000,hook(kind))
   u.reg_write(UC_MIPS_REG_S1,profile);u.reg_write(UC_MIPS_REG_S2,char);u.reg_write(UC_MIPS_REG_S3,0x4a0000);u.reg_write(UC_MIPS_REG_A0,0xf88)
   u.reg_write(UC_MIPS_REG_A1,777);u.reg_write(UC_MIPS_REG_A3,6);u.reg_write(UC_MIPS_REG_S0,0x1234);u.reg_write(UC_MIPS_REG_S4,0x5678)
   u.emu_start(site,site+16,count=100)
   v0=u.reg_read(UC_MIPS_REG_V0);v1=u.reg_read(UC_MIPS_REG_V1);a0=u.reg_read(UC_MIPS_REG_A0)
   got=(v0+(v1 if kind=='stat' else a0)+(v1 if kind=='debit' else 0x4a6ca8))&0xffffffff
   expected=0xa00000+profile*0xf88 if char==30 else 0x4a6ca8+profile*0x9b50+char*0xf88
   assert got==expected,(hex(site),profile,char,hex(got),hex(expected))
   assert u.reg_read(UC_MIPS_REG_LO)==(char*0xf88 if kind=='debit' else profile*0x9b50)
   assert u.reg_read(UC_MIPS_REG_HI)==0
   for reg,val in [(UC_MIPS_REG_S1,profile),(UC_MIPS_REG_S2,char),(UC_MIPS_REG_S3,0x4a0000),(UC_MIPS_REG_A1,777),(UC_MIPS_REG_A3,6),(UC_MIPS_REG_S0,0x1234),(UC_MIPS_REG_S4,0x5678)]:assert u.reg_read(reg)==val
   cases+=1
print(cases,'upgrade balance/stat/debit routing cases passed; actual purchases still require live validation.')
