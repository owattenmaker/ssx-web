"""Execute the full original stat-purchase function with patches."""
import struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
from upgrade_hooks import SITES,hook,patch_words
from upgrade_hooks import refresh_after_purchase
from profile_hooks import wardrobe_name_budget
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
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xc00000)
u.mem_write(0x150c20,scalar(read(0x150c20,0x248)))
u.mem_write(0x45a730,read(0x45a730,28));u.mem_write(0x440550,read(0x440550,128));u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
for i,(site,cell,kind,words) in enumerate(SITES):
 assert read(site,16)==struct.pack('<4I',*words)
 ptr=0x900000+i*0x100;u.mem_write(ptr,hook(kind));u.mem_write(cell,struct.pack('<I',ptr));u.mem_write(site,patch_words(cell))
u.mem_write(0x150e10,struct.pack('<I',(3<<26)|(0x149c98>>2)))
u.mem_write(0x149c98,struct.pack('<4I',0x3c190053,0x8f390a10,0x03200008,0))
u.mem_write(0x530a10,struct.pack('<I',0x902000));u.mem_write(0x902000,wardrobe_name_budget(True))
u.mem_write(0x530b24,struct.pack('<I',0x903000));u.mem_write(0x903000,refresh_after_purchase())
u.mem_write(0x535538,struct.pack('<I',0xa10000))
# Notification services are represented by a recording stub; all price,
# affordability, cap and cash/stat mutations execute original instructions.
u.mem_write(0x147f78,struct.pack('<4I',0x3c0200b0,0x34420000,0x03e00008,0))
u.mem_write(0xb0000c,struct.pack('<I',0xb00100));u.mem_write(0xb00110,struct.pack('<h',0));u.mem_write(0xb00114,struct.pack('<I',0xb00200));u.mem_write(0xb00200,struct.pack('<2I',0x03e00008,0))
notifications=[]
def ee(vm,at,size,user):
 if at==0xb00200:notifications.append(vm.reg_read(UC_MIPS_REG_A0))
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
 for char in [0,3,9,30,31]:
  for stat in range(7):
   for level in [5,6,54,55]:
    price=struct.unpack('<I',read(0x440550+(level//5-1)*8+4,4))[0]
    for cash in sorted({0,max(0,price-1),price,price+100}):
     old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88
     record=bytearray(0xf88);struct.pack_into('<2I',record,0xac4,cash,99999);record[0xbdf:0xbe6]=bytes([level]*7)
     u.mem_write(old,bytes(record));u.mem_write(sam,bytes(record));notifications.clear()
     u.mem_write(0xa10000,bytes([0xa5])*420)
     for reg,value in [(UC_MIPS_REG_A0,0xb00300),(UC_MIPS_REG_A1,profile),(UC_MIPS_REG_A2,char),(UC_MIPS_REG_A3,stat),(UC_MIPS_REG_RA,0x100000),(UC_MIPS_REG_SP,0xc10000)]:u.reg_write(reg,value)
     saved={reg:0x123400+reg for reg in [UC_MIPS_REG_S0,UC_MIPS_REG_S1,UC_MIPS_REG_S2,UC_MIPS_REG_S3,UC_MIPS_REG_S4]}
     for reg,value in saved.items():u.reg_write(reg,value)
     u.emu_start(0x150c20,0x100000,count=500)
     assert u.reg_read(UC_MIPS_REG_PC)==0x100000
     succeeds=profile<2 and cash>=price and level!=55
     expected=record.copy()
     if succeeds:struct.pack_into('<I',expected,0xac4,cash-price);expected[0xbdf+stat]+=1
     assert u.reg_read(UC_MIPS_REG_V0)==int(succeeds),(profile,char,stat,level,cash)
     assert notifications==([0xb00000] if succeeds else [])
     assert bytes(u.mem_read(old,0xf88))==(bytes(record) if char==30 else bytes(expected)),(profile,char,stat,level,cash,'old')
     assert bytes(u.mem_read(sam,0xf88))==(bytes(expected) if char==30 else bytes(record)),(profile,char,stat,level,cash,'sam')
     cache=bytearray([0xa5]*420)
     if char==30 and succeeds:cache[210+profile*70:217+profile*70]=expected[0xbdf:0xbe6]
     assert bytes(u.mem_read(0xa10000,420))==bytes(cache),(profile,char,stat,level,cash,'cache')
     assert u.reg_read(UC_MIPS_REG_SP)==0xc10000
     for reg,value in saved.items():assert u.reg_read(reg)==value
     cases+=1
print(cases,'full purchase cases passed: prices, guards, isolated writes, immediate Sam cache refresh and notification.')
