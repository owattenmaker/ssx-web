"""Decode original LUN instruction boundaries, preserving unknown semantics."""
import json,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
import argparse
from locations import location as location_info,pickup_file,pickups_dir
LOCATION=(lambda p:(p.add_argument('--location',default='ARA1'),p.parse_args().location)[1])(argparse.ArgumentParser(description=__doc__));location_info(LOCATION)
source=json.loads(pickup_file(LOCATION,'scripts').read_text());elf=(root/'local/disc/SLUS_207.72').read_bytes()
# Handlers read one inline word through stack+74 and advance s6 once.
wide={0x14:0x22418c,0x15:0x2241e8,0x16:0x222a3c,0x17:0x2229d8,0x1d:0x224228,0x24:0x2242b8,0x25:0x2248b8,0x26:0x224920,0x27:0x224998}
for pc in wide.values():assert struct.unpack_from('<I',elf,pc-0xff000)[0]==0x26d60001
programs=[];contacts=[]
for program in source['programs']:
 words=program['code_words'];instructions=[];index=0
 while index<len(words):
  word=words[index];opcode=word&255
  if opcode>42:raise ValueError(f'Unknown opcode {opcode:x} at program{program["index"]} word{index}')
  width=2 if opcode in wide else 1
  if index+width>len(words):raise ValueError('Truncated inline operand')
  entry=dict(word_index=index,offset=program['code_start']+index*4,word=word,opcode=opcode,inline_words=words[index+1:index+width])
  if opcode==0x21:
   builtin=(word>>16)&255
   if builtin>=111:raise ValueError('Builtin outside observed table')
   entry.update(builtin=builtin,argument_count=word>>24,destination=(word>>8)&255,target=struct.unpack_from('<I',elf,0x441f38+builtin*4-0xff000)[0])
   if builtin==0x41:contacts.append(dict(program=program['index'],**entry))
  instructions.append(entry);index+=width
 programs.append(dict(index=program['index'],offset=program['offset'],instructions=instructions))
assert len(contacts)==17 or LOCATION!='ARA1'
pickup_file(LOCATION,'disassembly').write_text(json.dumps(dict(programs=programs,contact_calls=contacts,scope='Instruction boundaries and builtin dispatch decoded; execution/control-flow and operand meanings incomplete'),indent=2)+'\n')
print('Decoded',sum(len(p['instructions']) for p in programs),'instructions in',len(programs),'programs;',len(contacts),'contact calls on instruction boundaries')
