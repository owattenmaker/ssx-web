"""Execute allocation, sparse routing and initializer call ABI in Unicorn.
Original game initializer bodies are represented by recording hooks here;
real equipment population still needs PCSX2 validation.
"""
from pathlib import Path
import json,struct,sys
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN,UC_HOOK_CODE,UcError
from unicorn.mips_const import *
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'/('save-extension' if '--save-extension' in sys.argv else 'bio' if '--bio' in sys.argv else 'fx-filter')
r=json.loads((folder/'character-storage-patches.json').read_text());payload=(folder/'CHARDB-expanded-candidate.DBL').read_bytes()
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
for base,size in [(0x100000,0x20000),(0x150000,0x10000),(0x310000,0x10000),(0x410000,0x10000),(0x4a0000,0x10000),(0x530000,0x10000),(0x900000,0x10000),(0xb00000,0x30000),(0xc00000,0x10000)]:u.mem_map(base,size)
for at in (0x317e30,0x416210,0x151600,0x151a88):u.mem_write(at,struct.pack('<II',0x03e00008,0))
u.mem_write(0x900000,payload);allocations=[];calls=[]
def hook(vm,at,size,user):
 a0=vm.reg_read(UC_MIPS_REG_A0);a1=vm.reg_read(UC_MIPS_REG_A1);a2=vm.reg_read(UC_MIPS_REG_A2)
 if at==0x317e30:
  assert a0 in (0x5b0,0x2e98);ptr=0xb00000 if a0==0x5b0 else 0xb10000
  allocations.append((a0,ptr));vm.reg_write(UC_MIPS_REG_V0,ptr)
 elif at==0x416210:
  assert (a0,a1,a2) in [(0xb00000,0,0x5b0),(0xb10000,0,0x2e98)];vm.mem_write(a0,bytes(a2))
 elif at in (0x151600,0x151a88):
  index=(a0-0xb10000)//0xf88;assert index in (0,1,2) and a0==0xb10000+index*0xf88
  calls.append((hex(at),index,a1,a2 if at==0x151600 else None))
  assert a1==(3 if at==0x151600 else 30)
  if at==0x151600:assert a2==1;vm.mem_write(a0+0x288,struct.pack('<I',0xc00000+index*0x1000))
  if at==0x151a88:
   lookup=0xc00000+index*0x1000
   vm.mem_write(lookup+84*2,struct.pack('<h',2));vm.mem_write(lookup+86*2,struct.pack('<h',3))
   vm.mem_write(a0+0x290+2*4,struct.pack('<4H',84,0x36,86,0x2))
  vm.mem_write(a0+0xbc0,bytes([a1]))
u.hook_add(UC_HOOK_CODE,hook)
u.reg_write(UC_MIPS_REG_SP,0x11fff0);u.reg_write(UC_MIPS_REG_RA,0x100000);u.reg_write(UC_MIPS_REG_S0,0x900000)
u.emu_start(0x900000,0x100000,count=10000)
assert allocations==[(0x5b0,0xb00000),(0x2e98,0xb10000)]
assert bytes(u.mem_read(0x530a9a,15))==bytes([30]+[5]*7+[11]*7)
if '--bio' in sys.argv or '--save-extension' in sys.argv:
 from rider_bio import DNA,FAVES,QNA
 for cell,kind,values in [(0x530a14,'DNA',DNA),(0x530a18,'FAVES',FAVES),(0x530a1c,'QNA',QNA)]:
  table=struct.unpack('<I',u.mem_read(cell,4))[0]
  for i in range(len(values)):
   ptr=struct.unpack('<I',u.mem_read(table+4*i,4))[0]
   assert bytes(u.mem_read(ptr,len(f'kT_{kind}{i+1}Sam')+1))==f'kT_{kind}{i+1}Sam'.encode()+b'\0'
get=0x900000+r['profile_hooks']['get_record']['offset']
for profile in range(3):
 for character in list(range(30))+[30,31]:
  u.reg_write(UC_MIPS_REG_A1,profile);u.reg_write(UC_MIPS_REG_A2,character);u.reg_write(UC_MIPS_REG_RA,0x100000)
  u.emu_start(get,0x100000,count=100)
  expected=0xb10000+profile*0xf88 if character==30 else 0x4a6ca8+profile*0x9b50+character*0xf88
  assert u.reg_read(UC_MIPS_REG_V0)==expected,(profile,character)
init=0x900000+r['profile_hooks']['initialize_records']['offset']
u.reg_write(UC_MIPS_REG_RA,0x100000);u.reg_write(UC_MIPS_REG_S3,0x530000);u.reg_write(UC_MIPS_REG_SP,0x11fff0)
u.emu_start(init,0x100000,count=10000)
assert len(calls)==6 and u.reg_read(UC_MIPS_REG_SP)==0x11fff0
for i in range(3):assert bytes(u.mem_read(0xb10000+i*0xf88+0xbc0,1))==b'\x1e'
for i in range(3):assert struct.unpack('<4H',u.mem_read(0xb10000+i*0xf88+0x290+8,8))==(84,0x22,86,0x16)
assert len({struct.unpack('<I',u.mem_read(0xb10000+i*0xf88+0x288,4))[0] for i in range(3)})==3
if '--save-extension' in sys.argv:
 baseline=bytearray(u.mem_read(0xb10000,0xf88));baseline[0x288:0x28c]=bytes(4)
 template=struct.unpack('<I',u.mem_read(0x530a38,4))[0]
 assert bytes(u.mem_read(template,0xf88))==baseline
assert u.reg_read(UC_MIPS_REG_V0)==0x530600 and u.reg_read(UC_MIPS_REG_V1)==400 and u.reg_read(UC_MIPS_REG_A1)==200
report=dict(routing_cases=96,original_character_addresses_unchanged=True,independent_sam_records=3,initializer_calls=calls,displaced_setup_replayed=True,limitations='Initializer bodies mocked; no save or gameplay proof')
(folder/'hook-tests.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
