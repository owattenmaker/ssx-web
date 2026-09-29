"""Execute the original getFrameByLabel against the rebuilt screen timeline."""
from pathlib import Path
import struct,sys
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS64,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from lui_screen import Screen
from loc_file import name_hash
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
original=Screen.decode((folder/'character-select-screen.bin').read_bytes())
candidate=Screen.decode((folder/'sam-roster-screen-candidate.bin').read_bytes())
assert [(s.name,s.flags) for s in candidate.states if s.name!=0x597d]==[(s.name,s.flags) for s in original.states]
assert [s.flags for s in candidate.states]==sorted(set(s.flags for s in candidate.states))
elf=Disc(Path.home()/'Downloads/SSX 3 (USA).iso').file('SLUS_207.72')
ph=struct.unpack_from('<I',elf,28)[0];stride,count=struct.unpack_from('<HH',elf,42)
for i in range(count):
 typ,off,va,_,size,_,_,_=struct.unpack_from('<8I',elf,ph+i*stride)
 if typ==1 and va<=0x39c778<va+size:code=elf[off+0x39c778-va:off+0x39c7bc-va];break
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS64|UC_MODE_LITTLE_ENDIAN)
for base,size in [(0x390000,0x10000),(0x500000,0x20000)]:u.mem_map(base,size)
u.mem_write(0x39c778,code);data=candidate.encode();u.mem_write(0x500000,data)
u.mem_write(0x510034,struct.pack('<I',0x500000+struct.unpack_from('<I',data,8)[0]))
for state in candidate.states:
 if not state.name:continue
 u.reg_write(UC_MIPS_REG_A0,0x510000);u.reg_write(UC_MIPS_REG_A1,state.name);u.reg_write(UC_MIPS_REG_RA,0x390000)
 u.emu_start(0x39c778,0x390000,count=400)
 assert u.reg_read(UC_MIPS_REG_V0)==state.flags
for widget,frame in [(55,75),(58,90)]:
 d=next(d for d in candidate.definitions if struct.unpack_from('<I',d,4)[0]==widget)
 assert struct.unpack_from('<h',d,12)[0]==frame
assert next(s.flags for s in candidate.states if s.name==0x597d)==90
description=name_hash('SamDescription')
parent_index=next(i for i,d in enumerate(candidate.definitions) if struct.unpack_from('<I',d,4)[0]==0x3277174)
parent=candidate.definitions[parent_index]
assert struct.unpack_from('<11I',parent,36)==(*range(0x69020,0x6902a),description)
assert next(i for i,d in enumerate(candidate.definitions) if struct.unpack_from('<I',d,4)[0]==description)<parent_index
print('Original game frame lookup verified: Sam90, Mac75, all original labels unchanged; selection bindings match.')
