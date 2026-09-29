"""Execute peak flag getter/setters with exact low-bit equivalent scalar ops.

Flags use bits12/13 of a 64-bit field. Scalar adaptations preserve the untouched
upper word in RAM; tests compare the entire record, including that upper word.
"""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
from peak_flag_hooks import READ_SITE,READ_CELL,WRITES,reader,writer
from snapshot_hooks import patch_words
elf=(Path(__file__).resolve().parents[2]/'local/disc/SLUS_207.72').read_bytes()
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
def read(at,n):
 for i in range(phnum):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError(hex(at))
def scalar(b):
 out=list(struct.unpack('<%dI'%(len(b)//4),b))
 for i,w in enumerate(out):
  op=w>>26;fn=w&63;shift=(w>>6)&31
  if op==0 and fn==0x2d:out[i]=w-12
  elif op in (30,55):out[i]=(w&0x03ffffff)|(35<<26)
  elif op in (31,63):out[i]=(w&0x03ffffff)|(43<<26)
  elif op==0 and fn==0x38:
   if shift in (19,20):out[i]=(w&~0x7ff)|((32-shift)<<6)|2
   else:out[i]=w&~63
  elif op==0 and fn==0x3f:out[i]=0
 return struct.pack('<%dI'%len(out),*out)
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xc00000)
u.mem_write(READ_SITE,patch_words(READ_CELL));u.mem_write(READ_CELL,struct.pack('<I',0x900000));u.mem_write(0x900000,scalar(reader(read(READ_SITE,0x78))))
u.mem_write(0x146070,scalar(read(0x146070,0xe0)));u.mem_write(0x150b88,scalar(read(0x150b88,0x98)))
u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
for i,(site,cell,p,c,words) in enumerate(WRITES):
 assert read(site,16)==struct.pack('<4I',*words)
 ptr=0x901000+i*0x1000;u.mem_write(ptr,writer(site,p,c,words));u.mem_write(cell,struct.pack('<I',ptr));u.mem_write(site,patch_words(cell))
u.mem_write(0x14a0e0,struct.pack('<2I',0x03e00008,0));u.mem_write(0x147798,struct.pack('<2I',0x03e00008,0x00001021))
profile=0
def ee(vm,at,size,user):
 if at==0x14a0e0:vm.reg_write(UC_MIPS_REG_V0,profile)
 w=struct.unpack('<I',vm.mem_read(at,4))[0]
 if w&0xfc00003f==0x18:
  rs,rt,rd=(w>>21)&31,(w>>16)&31,(w>>11)&31
  v=vm.reg_read(UC_MIPS_REG_0+rs)*vm.reg_read(UC_MIPS_REG_0+rt)
  vm.reg_write(UC_MIPS_REG_LO,v&0xffffffff);vm.reg_write(UC_MIPS_REG_HI,(v>>32)&0xffffffff)
  if rd:vm.reg_write(UC_MIPS_REG_0+rd,v&0xffffffff)
  vm.reg_write(UC_MIPS_REG_PC,at+4)
u.hook_add(UC_HOOK_CODE,ee)
patterns=[0,1<<12,1<<13,3<<12,0xffffffffffffffff,0x8123456789abcdef]
getters=mutations=0
for profile in range(3):
 for char in range(32):
  old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88
  for mode in [0,1,2,3,0xffffffff]:
   for flags in patterns:
    u.mem_write(old+0x278,struct.pack('<Q',flags));u.mem_write(sam+0x278,struct.pack('<Q',flags^0x3000))
    u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,char);u.reg_write(UC_MIPS_REG_A3,mode);u.reg_write(UC_MIPS_REG_RA,0x100000)
    u.emu_start(READ_SITE,0x100000,count=150)
    actual=flags^0x3000 if char==30 else flags;expected=0 if mode==0 else (actual>>(12 if mode==1 else 13))&1
    assert u.reg_read(UC_MIPS_REG_V0)==expected,(profile,char,mode,hex(flags))
    getters+=1
for profile in range(3):
 for char in [0,3,9,30,31]:
  old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88
  for start in [0x146070,0x150b88]:
   for mode in [0,1,2,3]:
    for value in [0,1,2,0xffffffff]:
     for flags in patterns:
      record=bytearray([0xa5])*0xf88;struct.pack_into('<Q',record,0x278,flags)
      u.mem_write(old,bytes(record));u.mem_write(sam,bytes(record));u.mem_write(0x534ff1,bytes([char]))
      u.reg_write(UC_MIPS_REG_RA,0x100000);u.reg_write(UC_MIPS_REG_SP,0xc10000)
      u.reg_write(UC_MIPS_REG_A1,0 if start==0x146070 else profile);u.reg_write(UC_MIPS_REG_A2,mode if start==0x146070 else char);u.reg_write(UC_MIPS_REG_A3,value if start==0x146070 else mode);u.reg_write(UC_MIPS_REG_T0,value)
      u.emu_start(start,0x100000,count=300)
      assert u.reg_read(UC_MIPS_REG_PC)==0x100000 and u.reg_read(UC_MIPS_REG_SP)==0xc10000
      expected=record.copy()
      if start==0x146070 or profile<2 and mode in (1,2):
       bit=12 if mode==1 else 13;enabled=(value if start==0x146070 else value^1)&1
       struct.pack_into('<Q',expected,0x278,(flags&~(1<<bit))|(enabled<<bit))
      assert bytes(u.mem_read(old,0xf88))==(bytes(record) if char==30 else bytes(expected)),(hex(start),profile,char,mode,value,'old')
      assert bytes(u.mem_read(sam,0xf88))==(bytes(expected) if char==30 else bytes(record)),(hex(start),profile,char,mode,value,'sam')
      mutations+=1
print(getters,'peak flag reads and',mutations,'full setter executions passed; other bits, upper words and rider records preserved.')
