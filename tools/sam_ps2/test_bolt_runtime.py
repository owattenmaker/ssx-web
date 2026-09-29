"""Execute persistent wardrobe allocation and the real cached lookup path."""
from pathlib import Path
import json,struct,sys
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_MIPS64,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE
from unicorn.mips_const import *
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster/bolts'
manifest=json.loads((folder/'character-storage-patches.json').read_text());payload=(folder/'CHARDB-expanded-candidate.DBL').read_bytes()
uc=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
for base,size in [(0x100000,0x20000),(0x310000,0x10000),(0x410000,0x10000),(0x4a0000,0x10000),(0x530000,0x10000),(0x900000,0x10000),(0xb00000,0x10000)]:uc.mem_map(base,size)
uc.mem_write(0x900000,payload);uc.mem_write(0xb00000,b'\xcc'*0x10000)
for at in (0x317e30,0x416210):uc.mem_write(at,struct.pack('<II',0x03e00008,0))
allocations=[]
def hook(u,address,size,user):
 if address==0x317e30:
  assert u.reg_read(UC_MIPS_REG_A0)==0x5b0
  allocations.append(0xb00000);u.reg_write(UC_MIPS_REG_V0,0xb00000)
 if address==0x416210:
  target=u.reg_read(UC_MIPS_REG_A0);count=u.reg_read(UC_MIPS_REG_A2);value=u.reg_read(UC_MIPS_REG_A1)
  assert target==0xb00000 and count==0x5b0 and value==0
  u.mem_write(target,bytes(count))
uc.hook_add(UC_HOOK_CODE,hook)
def initialize(base):
 uc.mem_write(base,payload);uc.reg_write(UC_MIPS_REG_S0,base);uc.reg_write(UC_MIPS_REG_SP,0x11fff0);uc.reg_write(UC_MIPS_REG_RA,0x100000)
 uc.emu_start(base,0x100000,count=10000)
 assert uc.reg_read(UC_MIPS_REG_SP)==0x11fff0 and uc.reg_read(UC_MIPS_REG_S0)==base
initialize(0x900000)
assert struct.unpack('<I',uc.mem_read(0x4a6750,4))[0]==0xb00000
assert bytes(uc.mem_read(0xb00000,0x5b0))==bytes(0x5b0)
assert bytes(uc.mem_read(0xb005b0,32))==b'\xcc'*32
uc.mem_write(0xb00010,b'keep');initialize(0x902000)
assert allocations==[0xb00000] and bytes(uc.mem_read(0xb00010,4))==b'keep'
# The original cached lookup uses DADDU, so execute it in MIPS64 mode.
vm=Uc(UC_ARCH_MIPS,UC_MODE_MIPS64|UC_MODE_LITTLE_ENDIAN)
for base,size in [(0x100000,0x10000),(0x140000,0x10000),(0xb00000,0x20000)]:vm.mem_map(base,size)
d=Disc(Path.home()/'Downloads/SSX 3 (USA).iso');elf=d.file('SLUS_207.72')
ph=struct.unpack_from('<I',elf,28)[0];stride,count=struct.unpack_from('<HH',elf,42)
start,end=0x14d4c8,0x14d554
for i in range(count):
 typ,off,va,_,size,_,_,_=struct.unpack_from('<8I',elf,ph+i*stride)
 if typ==1 and va<=start and end<=va+size:code=bytearray(elf[off+start-va:off+end-va]);break
for p in manifest['patches']:
 at=int(p['address'],16)
 if start<=at<end:
  old=bytes.fromhex(p['expected']);assert code[at-start:at-start+len(old)]==old
  code[at-start:at-start+len(old)]=bytes.fromhex(p['replacement'])
vm.mem_write(start,bytes(code));seen=[]
for character in range(32):
 table=0xb10000+character*128
 vm.mem_write(0xb00000+0x12c+character*4,struct.pack('<I',table))
 vm.mem_write(table+4,struct.pack('<H',1000+character))
for character in range(32):
 vm.reg_write(UC_MIPS_REG_A0,0xb00000);vm.reg_write(UC_MIPS_REG_A1,character);vm.reg_write(UC_MIPS_REG_A2,2);vm.reg_write(UC_MIPS_REG_RA,0x100000)
 vm.emu_start(start,0x100000,count=100)
 assert vm.reg_read(UC_MIPS_REG_V0)==1000+character,character;seen.append(character)
report=dict(allocation_reused=True,allocation_bytes=0x5b0,guard_bytes_preserved=True,original_cached_lookup_executed_for_ids=seen)
(folder/'runtime-tests.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
