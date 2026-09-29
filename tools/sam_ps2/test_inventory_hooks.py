"""Execute the full original inventory helpers with patches."""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
from inventory_hooks import SITES,hook
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
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xc00000)
u.mem_write(0x14aea8,scalar(read(0x14aea8,104)));u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
starts={}
for i,(site,cell,kind,size) in enumerate(SITES):
 start=0x900000+i*0x1000;starts[kind]=start;u.mem_write(start,scalar(hook(kind,read(site,size))))
def ee(vm,at,size,user):
 w=struct.unpack('<I',vm.mem_read(at,4))[0]
 if w&0xfc00003f==0x18:
  rs,rt,rd=(w>>21)&31,(w>>16)&31,(w>>11)&31
  value=vm.reg_read(UC_MIPS_REG_0+rs)*vm.reg_read(UC_MIPS_REG_0+rt)
  vm.reg_write(UC_MIPS_REG_LO,value&0xffffffff);vm.reg_write(UC_MIPS_REG_HI,value>>32)
  if rd:vm.reg_write(UC_MIPS_REG_0+rd,value&0xffffffff)
  vm.reg_write(UC_MIPS_REG_PC,at+4)
u.hook_add(UC_HOOK_CODE,ee)
cases=0
for profile in range(3):
 for char in range(32):
  for count in [0,3,443]:
   old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88;target=sam if char==30 else old
   record=bytearray(0xf88);struct.pack_into('<2I',record,0x288,0xb10000,count)
   for item in range(count):struct.pack_into('<2H',record,0x290+item*4,item,[0,4,16,20,0x40,0xffff][item%6])
   for kind in ['entry','list','commit']:
    u.mem_write(old,bytes(record));u.mem_write(sam,bytes(record));u.mem_write(0xb00000,bytes([0xa5])*16)
    arg=0xb00000 if kind=='list' else max(0,count-1)
    u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,char);u.reg_write(UC_MIPS_REG_A3,arg);u.reg_write(UC_MIPS_REG_RA,0x100000)
    u.emu_start(starts[kind],0x100000,count=10000)
    assert u.reg_read(UC_MIPS_REG_PC)==0x100000
    expected=record.copy()
    if kind=='entry':assert u.reg_read(UC_MIPS_REG_V0)==target+0x290+arg*4
    elif kind=='list':
     assert u.reg_read(UC_MIPS_REG_V0)==target+0x290
     assert bytes(u.mem_read(0xb00000,16))==struct.pack('<I',count)+bytes([0xa5])*12
    else:
     for item in range(count):
      at=0x292+item*4;flag=struct.unpack_from('<H',expected,at)[0]
      struct.pack_into('<H',expected,at,flag|4 if flag&16 else flag&~4)
    assert bytes(u.mem_read(old,0xf88))==(bytes(record) if char==30 else bytes(expected)),(kind,profile,char,count,'old')
    assert bytes(u.mem_read(sam,0xf88))==(bytes(expected) if char==30 else bytes(record)),(kind,profile,char,count,'sam')
    cases+=1
print(cases,'inventory entry/list/commit executions passed, including empty and full Sam inventories and isolated flag writes.')

from inventory_hooks import ROUTING_SITES,routing_hook
from snapshot_hooks import patch_words
for i,(site,cell,kind,words) in enumerate(ROUTING_SITES):
 assert read(site,16)==struct.pack('<4I',*words)
 ptr=0x904000+i*0x1000;u.mem_write(ptr,routing_hook(site,kind,words));u.mem_write(cell,struct.pack('<I',ptr));u.mem_write(site,patch_words(cell))
# Exercise the complete bulk-copy caller with a recording memcpy service.
copy_body=list(struct.unpack('<18I',read(0x14acb0,72)))
for i,w in enumerate(copy_body):
 if w>>26 in (55,63):copy_body[i]=(w&0x03ffffff)|((35 if w>>26==55 else 43)<<26)
 elif w&0xfc00003f==0x2d:copy_body[i]=w-12
u.mem_write(0x14acc0,struct.pack('<14I',*copy_body[4:]))
u.mem_write(0x3e6574,struct.pack('<2I',0x03e00008,0));copies=[]
def copy_service(vm,at,size,user):
 if at!=0x3e6574:return
 dest=vm.reg_read(UC_MIPS_REG_A0);source=vm.reg_read(UC_MIPS_REG_A1);count=vm.reg_read(UC_MIPS_REG_A2)
 copies.append((dest,source,count));vm.mem_write(dest,bytes(vm.mem_read(source,count)))
u.hook_add(UC_HOOK_CODE,copy_service)
source=bytes((i*17+3)&255 for i in range(0x834));u.mem_write(0xb20000,source)
extra=0
for profile in range(3):
 for char in range(32):
  old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88;target=sam if char==30 else old
  initial=bytes([0xa5])*0xf88;u.mem_write(old,initial);u.mem_write(sam,initial);copies.clear()
  u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,char);u.reg_write(UC_MIPS_REG_A3,0xb20000);u.reg_write(UC_MIPS_REG_SP,0xc10000);u.reg_write(UC_MIPS_REG_RA,0x100000)
  u.emu_start(0x14acb0,0x100000,count=200)
  assert copies==[(target+0x290,0xb20000,0x834)]
  expected=initial[:0x290]+source+initial[0xac4:]
  assert bytes(u.mem_read(old,0xf88))==(initial if char==30 else expected)
  assert bytes(u.mem_read(sam,0xf88))==(expected if char==30 else initial)
  assert u.reg_read(UC_MIPS_REG_SP)==0xc10000 and u.reg_read(UC_MIPS_REG_PC)==0x100000
  # Equip has more compatibility logic: here verify its pointer construction
  # and incoming item/equip arguments before allowing the original body.
  u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,char);u.reg_write(UC_MIPS_REG_A3,86);u.reg_write(UC_MIPS_REG_T0,1);u.reg_write(UC_MIPS_REG_RA,0x123456)
  u.emu_start(0x14afb0,0x14afc0,count=100)
  assert (u.reg_read(UC_MIPS_REG_A1)+u.reg_read(UC_MIPS_REG_A2)+0x4a6ca8)&0xffffffff==target
  assert u.reg_read(UC_MIPS_REG_A3)==86 and u.reg_read(UC_MIPS_REG_T0)==1 and u.reg_read(UC_MIPS_REG_RA)==0x123456
  extra+=2
print(extra,'bulk-copy/equip routing cases passed; compatibility application still needs live testing.')
