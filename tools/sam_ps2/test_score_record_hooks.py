"""Execute the full original score-record paths function with patches."""
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

from score_record_hooks import SITES,hook
from snapshot_hooks import patch_words
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xc00000)
ee_mults={};pending=None
def write_code(address,code):
 words=list(struct.unpack('<%dI'%(len(code)//4),code))
 for i,w in enumerate(words):
  if w&0xfc00003f==0x18 and (w>>11)&31:
   ee_mults[address+i*4]=w;words[i]=w&~(31<<11)
 u.mem_write(address,struct.pack('<%dI'%len(words),*words))
def flush_pending():
 global pending
 if pending is not None:
  reg,value=pending;u.reg_write(UC_MIPS_REG_0+reg,value);pending=None
for at,size in [(0x158910,0xa0),(0x1558f8,0x158),(0x152398,0x50),(0x152528,0x110)]:write_code(at,scalar(read(at,size)))
u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
for i,(site,cell,profile_reg,char_reg,cd,pd,words) in enumerate(SITES):
 assert read(site,16)==struct.pack('<4I',*words)
 ptr=0x900000+i*0x1000;u.mem_write(ptr,hook(site,profile_reg,char_reg,cd,pd,words));u.mem_write(cell,struct.pack('<I',ptr));u.mem_write(site,patch_words(cell))
 for address in range(site,site+16,4):ee_mults.pop(address,None)
profile=character=score_index=0
for at in [0x14ab68,0x14a0e0,0x14a080]:u.mem_write(at,struct.pack('<2I',0x03e00008,0))
def ee(vm,at,size,user):
 global pending
 flush_pending()
 if at==0x14ab68:vm.reg_write(UC_MIPS_REG_V0,score_index)
 elif at==0x14a0e0:vm.reg_write(UC_MIPS_REG_V0,profile)
 elif at==0x14a080:vm.reg_write(UC_MIPS_REG_V0,character)
 if at in ee_mults:
  w=ee_mults[at];rs,rt,rd=(w>>21)&31,(w>>16)&31,(w>>11)&31
  value=vm.reg_read(UC_MIPS_REG_0+rs)*vm.reg_read(UC_MIPS_REG_0+rt)
  pending=(rd,value&0xffffffff)
u.hook_add(UC_HOOK_CODE,ee)
def run(at,count=500):
 u.reg_write(UC_MIPS_REG_SP,0xc10000);u.reg_write(UC_MIPS_REG_RA,0x100000)
 u.emu_start(at,0x100000,count=count)
 assert u.reg_read(UC_MIPS_REG_PC)==0x100000 and u.reg_read(UC_MIPS_REG_SP)==0xc10000
routing=0
for reference_index,(site,cell,pr,cr,cd,pd,words) in enumerate(SITES):
 reference_address=0x908000+reference_index*0x100
 for profile in range(3):
  for character in range(32):
   for reg in range(1,32):u.reg_write(UC_MIPS_REG_0+reg,0x123400+reg)
   if site==0x159204:u.reg_write(UC_MIPS_REG_V0,0x4a0000);u.reg_write(UC_MIPS_REG_V1,0xf88)
   u.reg_write(UC_MIPS_REG_0+pr,profile);u.reg_write(UC_MIPS_REG_0+cr,character)
   initial={r:u.reg_read(UC_MIPS_REG_0+r) for r in range(1,32)}
   expected={0:0,**initial}
   for w in words:
    op=w>>26;rs=(w>>21)&31;rt=(w>>16)&31;rd=(w>>11)&31;imm=w&65535
    if op==9:expected[rt]=(expected[rs]+(imm if imm<32768 else imm-65536))&0xffffffff
    elif op==13:expected[rt]=expected[rs]|imm
    elif op==15:expected[rt]=imm<<16
    elif op==0 and w&63==0x18:expected[rd]=(expected[rs]*expected[rt])&0xffffffff
    else:raise AssertionError(hex(w))
   del expected[0]
   for r,v in initial.items():u.reg_write(UC_MIPS_REG_0+r,v)
   u.emu_start(site,site+16,count=120)
   if character==30:expected[cd]=(0xa00000+profile*0xf88-expected[pd]-0x4a6ca8)&0xffffffff
   for r,v in expected.items():
    if r not in [24,25]:assert u.reg_read(UC_MIPS_REG_0+r)==v,(hex(site),profile,character,r)
   routing+=1
reads=0
for profile in range(3):
 for character in range(32):
  for score_index in [0,1,25,26]:
   old=0x4a6ca8+profile*0x9b50+character*0xf88;sam=0xa00000+profile*0xf88
   for ptr,medal,score in [(old,2,123456),(sam,1,987654)]:u.mem_write(ptr+0xad0+score_index*8,struct.pack('<BbHI',1,medal,3,score))
   for start,default in [(0x158910,0xffffffff),(0x158960,0),(0x1558f8,0xffffffff)]:
    if start==0x1558f8:u.reg_write(UC_MIPS_REG_A1,0);u.reg_write(UC_MIPS_REG_A2,0);u.reg_write(UC_MIPS_REG_A3,0)
    else:u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,character);u.reg_write(UC_MIPS_REG_A3,0);u.reg_write(UC_MIPS_REG_T0,0)
    run(start)
    expected=default if score_index==26 else (987654 if character==30 else 123456) if start==0x158960 else (1 if character==30 else 2)
    assert u.reg_read(UC_MIPS_REG_V0)==expected,(hex(start),profile,character,score_index)
    reads+=1
writes=0
for profile in range(3):
 for character in [0,3,9,30,31]:
  for score_index in [0,25,26]:
   for event_mode in [0,1,3]:
    for before_medal in [-1,0,2]:
     for medal in [-1,0,1,3]:
      record=bytearray(0xf88);struct.pack_into('<BbHI',record,0xad0+min(score_index,25)*8,0,before_medal,4,123456)
      ref=0xb10000;old=0x4a6ca8+profile*0x9b50+character*0xf88;sam=0xa00000+profile*0xf88
      for ptr in [ref,old,sam]:u.mem_write(ptr,bytes(record))
      u.reg_write(UC_MIPS_REG_A0,ref);u.reg_write(UC_MIPS_REG_A1,0);u.reg_write(UC_MIPS_REG_A2,event_mode);u.reg_write(UC_MIPS_REG_A3,medal&0xffffffff);run(0x152528)
      expected=bytes(u.mem_read(ref,0xf88))
      u.reg_write(UC_MIPS_REG_A1,0);u.reg_write(UC_MIPS_REG_A2,0);u.reg_write(UC_MIPS_REG_A3,event_mode);u.reg_write(UC_MIPS_REG_T0,medal&0xffffffff);run(0x1559a8)
      assert bytes(u.mem_read(old,0xf88))==(bytes(record) if character==30 else expected)
      assert bytes(u.mem_read(sam,0xf88))==(expected if character==30 else bytes(record))
      writes+=1
print(routing,'pointer/register cases,',reads,'full medal/score reads and',writes,'medal updates matched original helper behavior with isolated records.')
