"""Execute the Sam-only effect predicate and original equipped-flag test."""
from pathlib import Path
import struct,json
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from profile_hooks import effect_filter
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
for a,n in [(0x100000,0x10000),(0x2e0000,0x10000),(0x900000,0x10000)]:u.mem_map(a,n)
u.mem_write(0x900000,effect_filter())
u.mem_write(0x2ece54,struct.pack('<4I',0x94620002,0x30420010,0x10400022,0x24930038))
cases=[]
for character in list(range(34))+[0xffffffff]:
 for flags in (0,16):
  u.mem_write(0x100044,struct.pack('<I',character));u.mem_write(0x100802,struct.pack('<H',flags))
  u.reg_write(UC_MIPS_REG_SP,0x100000);u.reg_write(UC_MIPS_REG_V1,0x100800);u.reg_write(UC_MIPS_REG_A1,7);u.reg_write(UC_MIPS_REG_A0,0x100900)
  normal=character<10 or character==0xffffffff or character==30
  expected=0x2ecee8 if normal and not flags else 0x2ece64
  u.emu_start(0x900000,expected,count=100)
  assert u.reg_read(UC_MIPS_REG_PC)==expected and u.reg_read(UC_MIPS_REG_S5)==8
  cases.append(dict(character=character,equipped=bool(flags),enabled=expected==0x2ece64))
Path('local/sam-ps2/roster/fx-filter/effect-tests.json').write_text(json.dumps(cases,indent=2)+'\n')
print('70 effect cases passed: originals preserved, Sam gated by equipment, bonus behavior unchanged')
