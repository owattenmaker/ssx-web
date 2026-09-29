"""Execute the candidate's standard-MIPS stub/access sequences in Unicorn.
This checks emitted instructions, not PS2 loader/cache or whole-game behavior.
"""
import json,struct,sys
from pathlib import Path
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import UC_MIPS_REG_0,UC_MIPS_REG_S0,UC_MIPS_REG_RA
from build_character_storage import BASE,FIELDS,DATA_OFFSET
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
if '--preview' in sys.argv:folder=folder/'preview'
elif '--attributes' in sys.argv:folder=folder/'attributes'
payload=(folder/'CHARDB-expanded-candidate.DBL').read_bytes()
report=json.loads((folder/'character-storage-patches.json').read_text())
data_offset=report['data_offset'];pointer_offsets=report.get('pointer_offsets',{hex(BASE+f):data_offset+f for f in FIELDS})
results=[]
for allocation in (0x600000,0x901230,0x1f02000):
 uc=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
 uc.mem_map(0x100000,0x10000);uc.mem_map(0x530000,0x10000)
 page=allocation&~4095;uc.mem_map(page,0x4000);uc.mem_write(allocation,payload)
 sentinel=bytes([0xa5])*0x10000;uc.mem_write(0x530000,sentinel)
 uc.reg_write(UC_MIPS_REG_S0,allocation);uc.reg_write(UC_MIPS_REG_RA,0x100000)
 uc.emu_start(allocation,0x100000,count=10000)
 expected=bytearray(sentinel)
 for cell,offset in pointer_offsets.items():
  struct.pack_into('<I',expected,int(cell,16)-0x530000,allocation+offset)
 assert bytes(uc.mem_read(0x530000,0x10000))==expected,'initializer overwrote unrelated globals'
 assert uc.reg_read(UC_MIPS_REG_S0)==allocation
 getter_count=0
 for patch in report['patches'][1:]:
  raw=bytes.fromhex(patch['replacement'])
  if len(raw)!=4 or int.from_bytes(raw,'little')>>26!=35:continue
  getter_count+=1
  word=int.from_bytes(bytes.fromhex(patch['replacement']),'little');rs=(word>>21)&31;rt=(word>>16)&31;cell=hex(0x530000+(word&65535));offset=pointer_offsets[cell];field=offset-data_offset
  code=struct.pack('<4I',(15<<26)|(rs<<16)|0x53,word,0x03e00008,0)
  uc.mem_write(0x101000,code);uc.reg_write(UC_MIPS_REG_RA,0x100000)
  # Unique addresses avoid relying on the translator to invalidate overwritten test code.
  at=0x102000+len(results)*0x200
  # Each getter uses a separate fresh entry as well.
  at+=report['patches'].index(patch)*16
  uc.mem_write(at,code);uc.emu_start(at,0x100000,count=20)
  actual=uc.reg_read(UC_MIPS_REG_0+rt)
  assert actual==allocation+offset,(patch['address'],hex(actual))
  if cell in {d['pointer_cell'] for d in report.get('dispatch_tables',[])}:
   assert bytes(uc.mem_read(actual,124))==payload[offset:offset+124]
   continue
  if cell=='0x535538':
   assert bytes(uc.mem_read(actual,210))==sentinel[0x5538:0x5538+210]
   assert bytes(uc.mem_read(actual+210,210))==bytes([5])*210
   continue
  for character in list(range(10))+[30]:
   count=136-field
   assert bytes(uc.mem_read(actual+136*character,count))==payload[data_offset+136*character+field:data_offset+136*(character+1)]
 if report.get('attribute_offset') is not None:
  old=allocation+report['attribute_offset'];uc.mem_write(old+210,bytes([37])*7)
  before=bytes(uc.mem_read(old,420));second=allocation+0x8000
  uc.mem_map(second&~4095,0x4000);uc.mem_write(second,payload)
  uc.reg_write(UC_MIPS_REG_S0,second);uc.reg_write(UC_MIPS_REG_RA,0x100000)
  uc.emu_start(second,0x100000,count=10000)
  assert bytes(uc.mem_read(second+report['attribute_offset'],420))==before,'reinitialization lost Sam stats'
 results.append(dict(allocation=hex(allocation),getter_sequences=getter_count,preserved_global_bytes=True,checked_ids=list(range(10))+[30]))
(folder/'character-storage-tests.json').write_text(json.dumps(results,indent=2)+'\n')
print(f"MIPS execution passed: 3 allocation addresses, {getter_count} getter sequences each; unrelated globals preserved; expanded cache reinitialization checked when enabled")
