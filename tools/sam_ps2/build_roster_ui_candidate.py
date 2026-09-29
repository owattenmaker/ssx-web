"""Stage an eleventh menu entry; NOT installed until runtime mapping is ready."""
from pathlib import Path
import copy,json,struct,sys
from lui_screen import Screen,State
from loc_file import name_hash
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
icons='--roster-icons' in sys.argv
b=(folder/'character-select-screen.bin').read_bytes();screen=Screen.decode(b)
assert screen.encode()==b
# IDs '0'..'9' are confirmed by onWidgetCreate's ASCII loop.
old_id=ord('7');new_id=ord(':')
def identity(r):return struct.unpack_from('<I',r,4)[0] if len(r)>=8 else None
def kind(r):return struct.unpack_from('<H',r)[0]
node=next(r for r in screen.definitions if kind(r)==0x17 and identity(r)==old_id)
new=bytearray(node);struct.pack_into('<I',new,4,new_id)
assert struct.unpack_from('<I',new,36)[0]==0x07c86713
struct.pack_into('<I',new,36,name_hash('SAM_NAME'))
assert struct.unpack_from('<h',new,12)[0]==75
struct.pack_into('<h',new,12,90) # selection-frame binding must match Sam's new frame
screen.definitions.insert(next(i for i,r in enumerate(screen.definitions) if kind(r)==0x13 and identity(r)==0x53c55),bytes(new))
menu=next(i for i,r in enumerate(screen.definitions) if kind(r)==0x13 and identity(r)==0x53c55)
r=bytearray(screen.definitions[menu]);assert struct.unpack_from('<I',r,32)[0]==10
assert list(struct.unpack_from('<10I',r,36))==list(range(48,58))
struct.pack_into('<H',r,2,len(r)+4);struct.pack_into('<I',r,32,11);r+=struct.pack('<I',new_id);screen.definitions[menu]=bytes(r)
# Keep original states and unknown payloads; give Sam an independent state.
mac_hash=0x5373;sam_hash=0x597d
mac=next(s for s in screen.states if s.name==mac_hash)
sam=copy.deepcopy(mac);sam.name=sam_hash
# The low 16 bits are a timeline frame number, not flags. Reusing Mac's75
# makes getFrameByLabel(Sam) select the Mac keyframe.90 is unused before100.
assert all(s.flags!=90 for s in screen.states)
sam.flags=90;screen.states.insert(next(i for i,s in enumerate(screen.states) if s.flags>90),sam)
# A distinct description widget lets Sam have his own text without changing Mac.
from loc_file import name_hash
description_id=0x69027
sam_description_id=name_hash('SamDescription')
sam_description_key=name_hash('SAM_TAGLINE')
assert all(identity(r)!=sam_description_id for r in screen.definitions)
description=bytearray(next(r for r in screen.definitions if identity(r)==description_id))
assert kind(description)==0x17 and struct.unpack_from('<I',description,36)[0]==0x01aa2673
struct.pack_into('<I',description,4,sam_description_id)
struct.pack_into('<I',description,36,sam_description_key)
parent_index=next(i for i,r in enumerate(screen.definitions) if kind(r)==0x10 and struct.pack('<I',description_id) in r[36:])
parent=bytearray(screen.definitions[parent_index]);child_count=struct.unpack_from('<I',parent,32)[0]
assert len(parent)==36+4*child_count and child_count==10
struct.pack_into('<H',parent,2,len(parent)+4);struct.pack_into('<I',parent,32,child_count+1)
parent+=struct.pack('<I',sam_description_id)
screen.definitions[parent_index]=bytes(parent)
screen.definitions.insert(parent_index,bytes(description))
for state in screen.states:
 for i,r in enumerate(list(state.records)):
  if kind(r)==0x21 and identity(r)==description_id:
   own=bytearray(r);struct.pack_into('<I',own,4,sam_description_id)
   old=bytearray(r)
   for at in range(12,len(r),4):
    if struct.unpack_from('<H',r,at)[0]==13:
     struct.pack_into('<h',own,at+2,255 if state is sam else 0)
     if state is sam:struct.pack_into('<h',old,at+2,0)
   state.records[i]=bytes(old);state.records.append(bytes(own))
for state in screen.states:
 for i,r in enumerate(list(state.records)):
  if kind(r)==0x21 and identity(r)==old_id:
   new=bytearray(r);struct.pack_into('<I',new,4,new_id);old=bytearray(r)
   for at in range(12,len(r),4):
    if struct.unpack_from('<H',r,at)[0]==13:
     struct.pack_into('<h',new,at+2,255 if state is sam else 0)
     if state is sam:struct.pack_into('<h',old,at+2,0)
   state.records[i]=bytes(old);state.records.append(bytes(new))
if icons:
 mac_icon=0x3757854;icon_group=0x648bf23
 white_id=name_hash('SamRosterWhite');orange_id=name_hash('SamRosterOrange')
 template=next(d for d in screen.definitions if identity(d)==mac_icon)
 group_index=next(i for i,d in enumerate(screen.definitions) if identity(d)==icon_group)
 group=bytearray(screen.definitions[group_index]);n=struct.unpack_from('<I',group,32)[0]
 assert kind(group)==0x10 and n==11 and len(group)==36+4*n
 struct.pack_into('<H',group,2,len(group)+8);struct.pack_into('<I',group,32,n+2)
 group+=struct.pack('<II',white_id,orange_id);screen.definitions[group_index]=bytes(group)
 for ident in [orange_id,white_id]:
  d=bytearray(template);struct.pack_into('<I',d,4,ident);struct.pack_into('<I',d,36,ident)
  screen.definitions.insert(group_index,bytes(d))
 for state in screen.states:
  for i,r in enumerate(list(state.records)):
   if kind(r)!=0x21:continue
   if identity(r)==mac_icon:
    for ident in [white_id,orange_id]:
     added=bytearray(r);struct.pack_into('<I',added,4,ident)
     for at in range(12,len(added),4):
      prop=struct.unpack_from('<H',added,at)[0]
      value={0:514,1:270,6:26,7:74,13:255 if ident==white_id or state is sam else 0}.get(prop)
      if value is not None:struct.pack_into('<h',added,at+2,value)
     state.records.append(bytes(added))
    if state is sam:
     old=bytearray(r)
     for at in range(12,len(old),4):
      if struct.unpack_from('<H',old,at)[0]==13:struct.pack_into('<h',old,at+2,0)
     state.records[i]=bytes(old)
   if identity(r)==0x6e5eed4:
    moved=bytearray(r)
    for at in range(12,len(moved),4):
     if struct.unpack_from('<Hh',moved,at)==(0,512):struct.pack_into('<h',moved,at+2,556)
    state.records[i]=bytes(moved)
range_updates=0
for state in screen.states:
 for i,r in enumerate(state.records):
  if kind(r)==0x21 and identity(r)==0x53c55:
   r=bytearray(r);count=struct.unpack_from('<I',r,8)[0];assert len(r)==12+4*count
   for at in range(12,len(r),4):
    prop,value=struct.unpack_from('<Hh',r,at)
    if prop==12:
     assert value==9;struct.pack_into('<h',r,at+2,10);range_updates+=1
   state.records[i]=bytes(r)
assert range_updates==1
assert next(i for i,r in enumerate(screen.definitions) if identity(r)==new_id)<menu
encoded=screen.encode();assert Screen.decode(encoded).encode()==encoded
(folder/('sam-roster-icons-screen-candidate.bin' if icons else 'sam-roster-screen-candidate.bin')).write_bytes(encoded)
(folder/'menu-candidate.json').write_text(json.dumps(dict(original_definition_count=229,candidate_definition_count=len(screen.definitions),original_state_count=18,candidate_state_count=len(screen.states),new_widget_id=new_id,installed=False,remaining=['New character ID and database allocation','Locale key for Sam label','Selection events and silhouettes','Wardrobe/portrait mapping','Separate save progression','Runtime verification']),indent=2)+'\n')
print('Staged 11-entry menu; original byte-exact round trip and candidate reparse passed. Not installed.')
