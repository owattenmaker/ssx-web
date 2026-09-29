"""Execute relocated switch dispatches for all normal/bonus/new/sentinel IDs."""
from pathlib import Path
import json,struct,sys
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import UC_MIPS_REG_S0,UC_MIPS_REG_RA,UC_MIPS_REG_A0,UC_MIPS_REG_PC
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster/preview'
r=json.loads((folder/'character-storage-patches.json').read_text());payload=(folder/'CHARDB-expanded-candidate.DBL').read_bytes()
d=Disc(Path.home()/'Downloads/SSX 3 (USA).iso');elf=d.file('SLUS_207.72')
phoff=struct.unpack_from('<I',elf,28)[0];phsize,count=struct.unpack_from('<HH',elf,42)
def original(at,n):
 for i in range(count):
  typ,off,va,_,size,_,_,_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
 raise ValueError('Unknown code range')
uc=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN)
for base,size in [(0x100000,0x10000),(0x190000,0x10000),(0x530000,0x10000),(0x900000,0x4000)]:uc.mem_map(base,size)
uc.mem_write(0x900000,payload);uc.reg_write(UC_MIPS_REG_S0,0x900000);uc.reg_write(UC_MIPS_REG_RA,0x100000);uc.emu_start(0x900000,0x100000,count=10000)
results=[]
for i,(start,end) in enumerate([(0x19eb04,0x19eb2c),(0x19ef0c,0x19ef30)]):
 code=bytearray(original(start,end-start))
 for p in r['patches']:
  at=int(p['address'],16)
  if start<=at<end:
   old=bytes.fromhex(p['expected']);assert code[at-start:at-start+len(old)]==old
   code[at-start:at-start+len(old)]=bytes.fromhex(p['replacement'])
 uc.mem_write(start,bytes(code));table=r['dispatch_tables'][i]
 for character in list(range(32))+[0xffffffff]:
  expected=table['original_targets'][character] if character<10 else table['sam_target'] if character==30 else table['default_target']
  uc.mem_write(0x100800,struct.pack('<I',character));uc.reg_write(UC_MIPS_REG_S0,0x100800);uc.reg_write(UC_MIPS_REG_A0,character)
  uc.emu_start(start,expected,count=50);assert uc.reg_read(UC_MIPS_REG_PC)==expected,(i,character)
  results.append(dict(dispatch=i,character=character,target=hex(expected)))
(folder/'dispatch-tests.json').write_text(json.dumps(results,indent=2)+'\n');print('66 dispatch cases passed, including original ten, reserved IDs, Sam and invalid sentinel')
