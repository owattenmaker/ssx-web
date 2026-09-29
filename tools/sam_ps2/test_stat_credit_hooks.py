"""Execute the full original cash-credit/stat-adjustment function with patches."""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
from stat_credit_hooks import SITES,hook,patch_words
root=Path(__file__).resolve().parents[2];elf=(root/'local/disc/SLUS_207.72').read_bytes()
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
def read(at,n):
 for i in range(phnum):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError(hex(at))
def scalar(code):
 words=struct.unpack('<%dI'%(len(code)//4),code)
 return struct.pack('<%dI'%len(words),*[w-12 if w&0xfc00003f==0x2d else w for w in words])
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xa00000)
u.mem_write(0x150e68,scalar(read(0x150e68,0x1d8)))
u.mem_write(0x45a750,read(0x45a750,28));u.mem_write(0x440550,read(0x440550,128));u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
for i,(site,cell,kind,words) in enumerate(SITES):
 assert read(site,16)==struct.pack('<4I',*words)
 ptr=0x900000+i*0x100;u.mem_write(ptr,hook(site,kind,words));u.mem_write(cell,struct.pack('<I',ptr));u.mem_write(site,patch_words(cell))
def ee(vm,at,size,user):
 w=struct.unpack('<I',vm.mem_read(at,4))[0]
 if w&0xfc00003f==0x18:
  rs,rt,rd=(w>>21)&31,(w>>16)&31,(w>>11)&31
  def signed(v):return v if v<0x80000000 else v-0x100000000
  result=signed(vm.reg_read(UC_MIPS_REG_0+rs))*signed(vm.reg_read(UC_MIPS_REG_0+rt))
  vm.reg_write(UC_MIPS_REG_LO,result&0xffffffff);vm.reg_write(UC_MIPS_REG_HI,(result>>32)&0xffffffff)
  if rd:vm.reg_write(UC_MIPS_REG_0+rd,result&0xffffffff)
  vm.reg_write(UC_MIPS_REG_PC,at+4)
u.hook_add(UC_HOOK_CODE,ee)
cases=0
for profile in range(3):
 for char in range(32):
  for stat in range(7):
   for level in [5,6,55]:
    old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88
    record=bytearray(0xf88);struct.pack_into('<2I',record,0xac4,1000,2000);record[0xbdf:0xbe6]=bytes([level]*7)
    u.mem_write(old,bytes(record));u.mem_write(sam,bytes(record))
    u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,char);u.reg_write(UC_MIPS_REG_A3,stat);u.reg_write(UC_MIPS_REG_RA,0x100000)
    u.emu_start(0x150e68,0x100000,count=300)
    assert u.reg_read(UC_MIPS_REG_PC)==0x100000
    expected=record.copy();changed=profile<2 and level!=5
    if changed:
     cost=struct.unpack('<I',read(0x440550+((level-1)//5)*8+4,4))[0]
     struct.pack_into('<I',expected,0xac4,(1000+cost)&0xffffffff);expected[0xbdf+stat]+=1
    assert u.reg_read(UC_MIPS_REG_V0)==int(changed)
    assert bytes(u.mem_read(old,0xf88))==(bytes(record) if char==30 else bytes(expected)),(profile,char,stat,level,'old')
    assert bytes(u.mem_read(sam,0xf88))==(bytes(expected) if char==30 else bytes(record)),(profile,char,stat,level,'sam')
    cases+=1
print(cases,'full cash-credit/stat-adjustment executions passed with original price table and unchanged return/write behavior.')
