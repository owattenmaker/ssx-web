"""Execute original cache initialization with candidate offsets/counts.
Only the EE-specific MULT rd variant is modeled; control flow and memory
writes execute in Unicorn MIPS64. This is not full renderer/game validation.
"""
from pathlib import Path
import json,struct,sys
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS64,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
r=json.loads((folder/'graphics-cache-patches.json').read_text())
d=Disc(Path.home()/'Downloads/SSX 3 (USA).iso');elf=d.file('SLUS_207.72');ph=struct.unpack_from('<I',elf,28)[0];stride,count=struct.unpack_from('<HH',elf,42)
def read(at,n):
 for i in range(count):
  typ,off,va,_,size,_,_,_=struct.unpack_from('<8I',elf,ph+i*stride)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError('Unmapped ELF range')
start,end=0x19ce68,0x19cf40;code=bytearray(read(start,end-start));special={}
for p in r['patches']:
 at=int(p['address'],16)
 if start<=at<end:
  old=bytes.fromhex(p['expected']);assert code[at-start:at-start+4]==old;code[at-start:at-start+4]=bytes.fromhex(p['replacement'])
for at in range(start,end,4):
 w=struct.unpack_from('<I',code,at-start)[0]
 if w>>26==0 and w&63==24 and w>>11&31:
  special[at]=w;struct.pack_into('<I',code,at-start,0)
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS64|UC_MODE_LITTLE_ENDIAN)
for base,size in [(0x100000,0x10000),(0x190000,0x10000),(0x4a0000,0x10000),(0x500000,0x241000)]:u.mem_map(base,size)
u.mem_write(start,bytes(code));u.mem_write(0x19cb60,struct.pack('<II',0x03e00008,0))
u.mem_write(0x500000,b'\xa5'*0x241000);u.mem_write(0x4a30f0-0x1788,struct.pack('<I',0x300000))
def hook(vm,pc,size,user):
 if pc not in special:return
 w=special[pc];rs=w>>21&31;rt=w>>16&31;rd=w>>11&31
 def signed(x):x&=0xffffffff;return x-0x100000000 if x&0x80000000 else x
 result=signed(vm.reg_read(UC_MIPS_REG_0+rs))*signed(vm.reg_read(UC_MIPS_REG_0+rt))
 lo=signed(result);hi=signed(result>>32)
 vm.reg_write(UC_MIPS_REG_0+rd,lo&0xffffffffffffffff);vm.reg_write(UC_MIPS_REG_LO,lo&0xffffffffffffffff);vm.reg_write(UC_MIPS_REG_HI,hi&0xffffffffffffffff)
u.hook_add(UC_HOOK_CODE,hook)
u.reg_write(UC_MIPS_REG_A0,0x500000);u.reg_write(UC_MIPS_REG_GP,0x4a30f0);u.reg_write(UC_MIPS_REG_SP,0x10fff0);u.reg_write(UC_MIPS_REG_RA,0x100100)
u.emu_start(start,0x100100,count=200000)
assert u.reg_read(UC_MIPS_REG_PC)==0x100100
for bank in range(32):
 for slot in range(256):
  at=0x500000+bank*0x12000+slot*0x120
  for field,value in [(0,0),(0x104,0),(0x108,0),(0x110,0xffffffff),(0x114,0),(0x118,0)]:assert struct.unpack('<I',u.mem_read(at+field,4))[0]==value,(bank,slot,field)
  assert bytes(u.mem_read(at+4,1))==b'\0'
assert bytes(u.mem_read(0x740000,8))==bytes(8)
assert struct.unpack('<32I',u.mem_read(0x740008,128))==(1,)*32
assert bytes(u.mem_read(0x740090,128))==b'\xa5'*128
# Execute all four external accessors. The previous constructor-only test could
# not catch callers receiving addresses inside the newly enlarged model banks.
u.mem_map(0x1d0000,0x10000);u.mem_map(0x250000,0x10000)
for entry,length,offset in [(0x1dccd8,16,0x241b10),(0x1dcce8,16,0x241b1c),(0x2591b8,24,0x241b10),(0x2591d0,24,0x241b1c)]:
 body=bytearray(read(entry,length))
 for p in r['patches']:
  at=int(p['address'],16)
  if entry<=at<entry+length:
   assert body[at-entry:at-entry+4]==bytes.fromhex(p['expected'])
   body[at-entry:at-entry+4]=bytes.fromhex(p['replacement'])
 u.mem_write(entry,bytes(body))
 u.mem_write(0x4a30f0-0x848,struct.pack('<I',0x4a0000))
 u.mem_write(0x4a007c,struct.pack('<I',0x500000))
 u.reg_write(UC_MIPS_REG_A0,0x500000);u.reg_write(UC_MIPS_REG_RA,0x100100)
 u.emu_start(entry,0x100100,count=20)
 assert u.reg_read(UC_MIPS_REG_V0)==0x500000+offset,hex(entry)
report=dict(banks=32,slots_per_bank=256,slot_initialization_verified=True,trailing_guard_untouched=True,external_accessors_verified=4,modeled_ee_mult_sites=list(map(hex,special)),scope='Constructor and external accessors; full graphics/runtime not validated')
(folder/'graphics-cache-tests.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
