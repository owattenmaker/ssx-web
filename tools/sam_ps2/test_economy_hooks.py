"""Exercise economy getter candidates against distinct original/Sam values."""
import struct,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
ROOT=Path(__file__).resolve().parents[2]
r=json.loads((ROOT/'local/sam-ps2/roster/save-extension/character-storage-patches.json').read_text())
payload=(ROOT/'local/sam-ps2/roster/save-extension/CHARDB-expanded-candidate.DBL').read_bytes()
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);u.mem_map(0x100000,0xb00000)
u.mem_write(0x900000,payload.replace(struct.pack('<I',0x0000102d),struct.pack('<I',0x00001021)));u.mem_write(0x5309fc,struct.pack('<I',0xa00000))
def ee(vm,at,size,user):
 w=struct.unpack('<I',vm.mem_read(at,4))[0]
 if w&0xfc00003f==0x18:
  rs,rt,rd=(w>>21)&31,(w>>16)&31,(w>>11)&31
  def signed(v):return v if v<0x80000000 else v-0x100000000
  product=signed(vm.reg_read(UC_MIPS_REG_0+rs))*signed(vm.reg_read(UC_MIPS_REG_0+rt))
  vm.reg_write(UC_MIPS_REG_LO,product&0xffffffff);vm.reg_write(UC_MIPS_REG_HI,(product>>32)&0xffffffff)
  if rd:vm.reg_write(UC_MIPS_REG_0+rd,product&0xffffffff)
  vm.reg_write(UC_MIPS_REG_PC,at+4)
 elif w==0x0000102d:
  vm.reg_write(UC_MIPS_REG_V0,0);vm.reg_write(UC_MIPS_REG_PC,at+4)
u.hook_add(UC_HOOK_CODE,ee)
cases=0
for site,field in [(0x150928,0xac4),(0x1509c0,0xac8)]:
 start=0x900000+r['profile_hooks'][f'economy_{site:x}']['offset']
 for profile in range(3):
  for char in range(32):
   old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88
   u.mem_write(old+field,struct.pack('<I',1234));u.mem_write(sam+field,struct.pack('<I',7890))
   u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,char);u.reg_write(UC_MIPS_REG_RA,0x100000)
   u.emu_start(start,0x100000,count=100)
   assert u.reg_read(UC_MIPS_REG_PC)==0x100000
   assert u.reg_read(UC_MIPS_REG_V0)==(0 if profile>=2 else 7890 if char==30 else 1234)
   cases+=1
start=0x900000+r['profile_hooks']['economy_150988']['offset']
for profile in range(3):
 for char in range(32):
  u.reg_write(UC_MIPS_REG_V0,char);u.reg_write(UC_MIPS_REG_S0,profile);u.reg_write(UC_MIPS_REG_RA,0x123456)
  u.emu_start(start,0x150998,count=100)
  got=(u.reg_read(UC_MIPS_REG_V0)+u.reg_read(UC_MIPS_REG_S0)+0x4a6ca8)&0xffffffff
  assert got==(0xa00000+profile*0xf88 if char==30 else 0x4a6ca8+profile*0x9b50+char*0xf88)
  assert u.reg_read(UC_MIPS_REG_RA)==0x123456
  cases+=1
print(cases,'economy read/routing cases passed; live HUD verification remains pending.')

# Execute original arithmetic/store tails as well as relocated fallback bodies.
elf=(ROOT/'local/disc/SLUS_207.72').read_bytes()
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
def read_original(at,n):
 for i in range(phnum):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError(hex(at))
def scalar(code):
 words=struct.unpack('<%dI'%(len(code)//4),code)
 return struct.pack('<%dI'%len(words),*[w-12 if w&0xfc00003f==0x2d else w for w in words])
u.mem_write(0x150a58,scalar(read_original(0x150a58,0x130)))
# Apply scalar DADDU adaptation to entire aligned hook spans, not binary data.
for site in [0x150a58,0x150a90,0x150b48]:
 meta=r['profile_hooks'][f'economy_{site:x}'];at=0x900000+meta['offset']
 u.mem_write(at,scalar(payload[meta['offset']:meta['offset']+meta['bytes']]))
mutations=0
for site,kind in [(0x150a58,'set'),(0x150a90,'earn'),(0x150b48,'spend')]:
 start=0x900000+r['profile_hooks'][f'economy_{site:x}']['offset']
 for profile in range(3):
  for char in range(32):
   for amount in [0,25,200]:
    # Distinct owned and old-formula records expose writes to the wrong target.
    old=0x4a6ca8+profile*0x9b50+char*0xf88;sam=0xa00000+profile*0xf88
    u.mem_write(old,bytes([0x5a])*0xf88);u.mem_write(sam,bytes([0xa5])*0xf88)
    u.mem_write(old+0xac4,struct.pack('<2I',100,300));u.mem_write(sam+0xac4,struct.pack('<2I',100,300))
    before_old=bytes(u.mem_read(old,0xf88));before_sam=bytes(u.mem_read(sam,0xf88))
    u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,char);u.reg_write(UC_MIPS_REG_A3,amount);u.reg_write(UC_MIPS_REG_RA,0x100000)
    u.emu_start(start,0x100000,count=100)
    assert u.reg_read(UC_MIPS_REG_PC)==0x100000
    expected=bytearray(before_sam if char==30 else before_old)
    if profile<2:
     cash=amount if kind=='set' else (100+amount if kind=='earn' else 100-amount)&0xffffffff
     struct.pack_into('<2I',expected,0xac4,cash,300+amount if kind=='earn' else 300)
     if kind!='set':assert u.reg_read(UC_MIPS_REG_V0)==cash
    else:assert u.reg_read(UC_MIPS_REG_V0)==(0xf88 if kind=='set' else 0)
    assert bytes(u.mem_read(sam,0xf88))==(bytes(expected) if char==30 else before_sam),(kind,profile,char,amount,'sam')
    assert bytes(u.mem_read(old,0xf88))==(before_old if char==30 else bytes(expected)),(kind,profile,char,amount,'original')
    mutations+=1
print(mutations,'cash set/reward/spend cases passed; exact original arithmetic retained, including unsigned underflow.')
