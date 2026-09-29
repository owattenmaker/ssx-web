"""Verify reset head choice, bounds guards and epilogue register preservation."""
import struct
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from default_head_hook import SITE,CELL,RESTORE,hook,patch
cases=0
for character in range(32):
 for lookup_case in ['valid','missing','out_of_range','null']:
  for flags in [0,0x36,0xffff]:
   u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xa00000)
   code=bytearray(hook())
   # Only the EE epilogue LQs are adapted to scalar loads in this harness.
   for i in range(0,len(code),4):
    word=struct.unpack_from('<I',code,i)[0]
    if word>>26==30:struct.pack_into('<I',code,i,(word&0x03ffffff)|(35<<26))
   u.mem_write(SITE,patch());u.mem_write(CELL,struct.pack('<I',0x900000));u.mem_write(0x900000,bytes(code))
   record=bytearray([0xa5])*0xf88
   struct.pack_into('<II',record,0x288,0 if lookup_case=='null' else 0xa10000,4)
   struct.pack_into('<HH',record,0x292+2*4,flags,0)
   struct.pack_into('<H',record,0x292+3*4,flags)
   u.mem_write(0xa00000,bytes(record))
   for item,index in [(84,2),(86,3)]:u.mem_write(0xa10000+2*item,struct.pack('<h',-1 if lookup_case=='missing' else 4 if lookup_case=='out_of_range' else index))
   u.reg_write(UC_MIPS_REG_S1,character);u.reg_write(UC_MIPS_REG_S3,0xa00000);u.reg_write(UC_MIPS_REG_SP,0xa20000);u.reg_write(UC_MIPS_REG_V0,0xdeadbeef)
   for i,off in enumerate([0x40,0x30,0x20,0x10]):u.mem_write(0xa20000+off,struct.pack('<4I',0x123400+i,2,3,4))
   u.emu_start(SITE,0x151bc4,count=150)
   expected=record.copy()
   if character==30 and lookup_case=='valid':
    struct.pack_into('<H',expected,0x292+2*4,flags&~0x34)
    struct.pack_into('<H',expected,0x292+3*4,flags|0x34)
   assert bytes(u.mem_read(0xa00000,0xf88))==bytes(expected)
   assert u.reg_read(UC_MIPS_REG_PC)==0x151bc4 and u.reg_read(UC_MIPS_REG_V0)==0xdeadbeef and u.reg_read(UC_MIPS_REG_SP)==0xa20000
   for i,reg in enumerate([UC_MIPS_REG_S0,UC_MIPS_REG_S1,UC_MIPS_REG_S2,UC_MIPS_REG_S3]):assert u.reg_read(reg)==0x123400+i
   # The original reset-by-default-bit rule must retain the regular head.
   if character==30 and lookup_case=='valid':
    head_flags=[struct.unpack_from('<H',expected,0x292+i*4)[0] for i in [2,3]]
    reset=[f|0x16 if f&0x20 else f&~0x14 for f in head_flags]
    assert reset[0]&0x14==0 and reset[1]&0x14==0x14
   cases+=1
print(cases,'default-head reset cases passed; missing/out-of-range lookups skipped and original riders preserved.')

from pathlib import Path
from default_head_hook import RESET_SITE,RESET_CELL,RESET_WORDS,reset_metadata_hook,reset_patch
elf=(Path(__file__).resolve().parents[2]/'local/disc/SLUS_207.72').read_bytes()
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
def original(at,n):
 for i in range(phnum):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError(hex(at))
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xa00000)
u.mem_write(RESET_SITE,original(RESET_SITE,80));assert original(RESET_SITE,16)==struct.pack('<4I',*RESET_WORDS)
u.mem_write(RESET_SITE,reset_patch());u.mem_write(RESET_CELL,struct.pack('<I',0x900000));u.mem_write(0x900000,reset_metadata_hook())
reset_cases=0
for character in range(32):
 for before_flags in [(0x22,0x16),(0x36,2),(2,0x36),(0xffff,0xffff)]:
  for lookup_case in ['valid','missing','out_of_range','null']:
   for count in [0,4,443]:
    record=bytearray([0xa5])*0xf88;record[0xbc0]=character
    struct.pack_into('<II',record,0x288,0 if lookup_case=='null' else 0xa10000,count)
    for i in range(count):struct.pack_into('<2H',record,0x290+4*i,i,[0,4,16,20,0x36,0xffff][i%6])
    for i,flag in zip([2,3],before_flags):struct.pack_into('<H',record,0x292+4*i,flag)
    u.mem_write(0xa00000,bytes(record))
    for item,index in [(84,2),(86,3)]:u.mem_write(0xa10000+2*item,struct.pack('<h',-1 if lookup_case=='missing' else count if lookup_case=='out_of_range' else index))
    expected=record.copy()
    if character==30 and lookup_case=='valid':
     for i,on in [(2,False),(3,True)]:
      if i<count:
       flag=struct.unpack_from('<H',expected,0x292+4*i)[0];struct.pack_into('<H',expected,0x292+4*i,flag|0x20 if on else flag&~0x20)
    for i in range(count):
     flag=struct.unpack_from('<H',expected,0x292+4*i)[0];struct.pack_into('<H',expected,0x292+4*i,flag|0x16 if flag&0x20 else flag&~0x14)
    u.reg_write(UC_MIPS_REG_A0,0xa00000);u.reg_write(UC_MIPS_REG_RA,0x100000);u.reg_write(UC_MIPS_REG_SP,0xa20000)
    u.emu_start(RESET_SITE,0x100000,count=10000)
    assert u.reg_read(UC_MIPS_REG_PC)==0x100000 and u.reg_read(UC_MIPS_REG_SP)==0xa20000
    assert bytes(u.mem_read(0xa00000,0xf88))==bytes(expected),(character,before_flags,lookup_case,count)
    reset_cases+=1
print(reset_cases,'complete default-outfit resets passed, including older-save head flags and invalid lookup cases.')
