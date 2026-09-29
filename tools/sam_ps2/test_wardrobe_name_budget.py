"""Run the Sam name-budget hook and check the displaced loop setup/ABI."""
import struct
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS64,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
from profile_hooks import wardrobe_name_budget
cases=0
for character in [0,3,9,10,29,30,31]:
 for flags in [0,0x80,0x100,0x200,0x400,0x800,0xf80]:
  for name in [None,b'',b'Wisconsin Chopped Unc']:
   u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS64|UC_MODE_LITTLE_ENDIAN)
   for a,n in [(0x100000,0x1000),(0x410000,0x10000),(0x500000,0x3000)]:u.mem_map(a,n)
   u.mem_write(0x100000,wardrobe_name_budget())
   u.mem_write(0x100800,struct.pack('<2I',0x24130003,0x8e240000))
   row=bytearray(56);row[0]=character
   struct.pack_into('<III',row,20,0 if name is None else 0x501000,0x123456,0)
   struct.pack_into('<I',row,52,flags);u.mem_write(0x500000,bytes(row))
   if name is not None:u.mem_write(0x501000,name+b'\0')
   calls=[]
   def strlen(vm,pc,size,user):
    if pc!=0x416810:return
    assert vm.reg_read(UC_MIPS_REG_A0)==0x501000
    calls.append(pc);vm.reg_write(UC_MIPS_REG_V0,len(name));vm.reg_write(UC_MIPS_REG_PC,vm.reg_read(UC_MIPS_REG_RA))
   u.hook_add(UC_HOOK_CODE,strlen)
   for reg,value in [(UC_MIPS_REG_S1,0x500018),(UC_MIPS_REG_S2,0x500000),(UC_MIPS_REG_S4,777),(UC_MIPS_REG_FP,12),(UC_MIPS_REG_SP,0x502ff0),(UC_MIPS_REG_RA,0x100800)]:u.reg_write(reg,value)
   u.emu_start(0x100000,0x100808,count=100)
   needed=character==30 and bool(flags&0xf00) and name is not None
   assert len(calls)==int(needed)
   assert u.reg_read(UC_MIPS_REG_S4)==777+(len(name)+1 if needed else 0)
   for reg,value in [(UC_MIPS_REG_S1,0x500018),(UC_MIPS_REG_S2,0x500000),(UC_MIPS_REG_S3,3),(UC_MIPS_REG_S5,13),(UC_MIPS_REG_FP,12),(UC_MIPS_REG_A0,0x123456),(UC_MIPS_REG_SP,0x502ff0),(UC_MIPS_REG_RA,0x100800)]:assert u.reg_read(reg)==value,(character,flags,name,reg)
   cases+=1
print(f'{cases} name-budget cases passed; Sam copied names counted, original characters unchanged, loop setup and stack preserved.')
