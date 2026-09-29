"""Read-only PS2 roster audit, anchored to the verified original executable."""
from pathlib import Path
import sys,struct,json,hashlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
ROOT=Path(__file__).resolve().parents[2]
from capstone import Cs,CS_ARCH_MIPS,CS_MODE_MIPS64,CS_MODE_LITTLE_ENDIAN
out=ROOT/'local/sam-ps2/roster';out.mkdir(parents=True,exist_ok=True)
d=Disc(Path.home()/'Downloads/SSX 3 (USA).iso');elf=d.file('SLUS_207.72')
assert hashlib.sha1(elf).hexdigest()=='77114dfd1205eaccf1ccc18c5f9650097fa78bd8'
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
segments=[]
for i in range(phnum):
 kind,offset,address,phys,size,mem,flags,align=struct.unpack_from('<8I',elf,phoff+i*phsize)
 if kind==1:segments.append((address,offset,size,mem))
def read(address,size):
 for va,offset,length,mem in segments:
  if va<=address and address+size<=va+length:return elf[offset+address-va:offset+address-va+size]
 raise ValueError(f'Not file-backed: {address:x}')
cs=Cs(CS_ARCH_MIPS,CS_MODE_MIPS64|CS_MODE_LITTLE_ENDIAN)
for name,start,end in [('character-select',0x181148,0x182488),('character-profile-map',0x14a080,0x14a160)]:
 lines=[]
 for at in range(start,end,4):
  word=read(at,4);ins=list(cs.disasm(word,at))
  text=f'{ins[0].mnemonic} {ins[0].op_str}' if ins else '.word '+hex(int.from_bytes(word,'little'))
  raw=int.from_bytes(word,'little');op=raw>>26;rs=(raw>>21)&31;rt=(raw>>16)&31;rd=(raw>>11)&31;imm=raw&65535;imm=imm-65536 if imm&32768 else imm
  # Capstone's generic MIPS64 decoder mistakes EE LQ/SQ for DSP/MSA.
  if op in (0x1e,0x1f):text=f'{"lq" if op==0x1e else "sq"} r{rt}, {imm}(r{rs})'
  elif op==0 and raw&63 in (0x18,0x19):text=f'{"mult" if raw&63==0x18 else "multu"} r{rd}, r{rs}, r{rt} ; EE destination register'
  elif op in (0x12,0x1c):text=f'.word 0x{raw:08x} ; EE COP2/MMI: not decoded here'
  lines.append(f'{at:08x}: {word.hex()}  {text}')
 (out/(name+'.asm')).write_text('\n'.join(lines)+'\n')
b=d.file('DATA/BE/CHARDB.DBL');assert len(b)%136==0
rows=[]
for i in range(len(b)//136):
 row=b[i*136:(i+1)*136]
 def string(at,n):return row[at:at+n].split(b'\0')[0].decode('ascii')
 rows.append(dict(index=i,long_name=string(0,32),first_name=string(32,16),nickname=string(48,16),weight=struct.unpack_from('<I',row,64)[0],stance=struct.unpack_from('<I',row,68)[0],model_size=struct.unpack_from('<i',row,72)[0],age=struct.unpack_from('<I',row,96)[0],height=string(100,16),nationality=string(116,16),position=struct.unpack_from('<I',row,132)[0]))
(out/'chardb.json').write_text(json.dumps(rows,indent=2)+'\n')
for start,size in [(0x440f68,48)]:
 try:(out/f'table-{start:08x}.bin').write_bytes(read(start,size))
 except ValueError as e:print(e)
print('Character DB:',len(rows),'records');print([(r['index'],r['first_name'],r['position']) for r in rows])
