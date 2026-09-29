"""Conservative linear provenance audit for front-end cache size/field constants.
Produces reviewable candidates, never patches the executable itself.
"""
import struct,json
import audit_roster as a
from pathlib import Path
# Owner ranges established from cache/view constructors and allocation callers.
RANGES=[('view',0x1956f8,0x195958),('cache',0x19cdf0,0x19e538),('view',0x1a1ce8,0x1a2b00),('view',0x228238,0x2282d0)]
# External accessors return addresses of the two trailing containers. Leaving
# these at the old offsets hands callers model-cache bytes as container headers.
RANGES += [('view',0x1dccd8,0x1dccf8),('view',0x2591b8,0x2591e8)]
refs=[];bounds=[]
def new_offset(owner,value):
 if owner=='cache':
  if 0xb4000<=value<0xb4030:return value+0x18c000
  if 0xb4030<=value<=0xb4040:return value+0x18c060
 else:
  if 0xb5a70<=value<0xb5aa0:return value+0x18c000
  if 0xb5aa0<=value<=0xb5b00:return value+0x18c060
def instruction(pc,tags,origin,owner):
 w=int.from_bytes(a.read(pc,4),'little');op=w>>26;rs=w>>21&31;rt=w>>16&31;rd=w>>11&31;imm=w&65535;si=imm-65536 if imm&32768 else imm
 tags=set(tags)
 if op==15:tags.discard(rt)
 elif op in (9,13):
  if rs in tags:
   old=0xb0000+(imm if op==13 else si);new=new_offset(owner,old)
   if new is not None:refs.append(dict(owner=owner,lui=hex(origin),use=hex(pc),old=hex(old),new=hex(new),opcode=hex(w)))
  tags.discard(rt)
 elif op in (32,33,35,36,37,40,41,43,49,57,55,63,30,31):
  if rs in tags:
   old=0xb0000+si;new=new_offset(owner,old)
   if new is not None:refs.append(dict(owner=owner,lui=hex(origin),use=hex(pc),old=hex(old),new=hex(new),opcode=hex(w)))
  if op in (32,33,35,36,37,55,30):tags.discard(rt)
 elif op==0:
  if w&63 in (0x21,0x2d,0x25):
   inherited=(rs in tags) != (rt in tags);tags.discard(rd)
   if inherited:tags.add(rd)
  elif w&63 not in (8,9):tags.discard(rd)
 elif op in (10,11,12,14):tags.discard(rt)
 return frozenset(tags)
for owner,start,end in RANGES:
 for seed in range(start,end,4):
  w=int.from_bytes(a.read(seed,4),'little')
  if owner=='cache' and w>>26==10 and w&65535==10:bounds.append(dict(address=hex(seed),opcode=hex(w)))
  if w>>26!=15 or w&65535!=11:continue
  initial=frozenset([w>>16&31]);work=[(seed+4,initial)];seen=set()
  if seed>=start+4:
   prev=int.from_bytes(a.read(seed-4,4),'little');pop=prev>>26
   if pop in (1,4,5,6,7,20,21,22,23) or pop==17 and (prev>>21&31)==8:
    imm=prev&65535;imm=imm-65536 if imm&32768 else imm
    work=[(seed+imm*4,initial)]
    if pop not in (20,21,22,23):work.append((seed+4,initial))
  while work:
   pc,tags=work.pop()
   if not tags or not start<=pc<end or (pc,tags) in seen:continue
   seen.add((pc,tags));assert len(seen)<20000
   w=int.from_bytes(a.read(pc,4),'little');op=w>>26;rs=w>>21&31;rt=w>>16&31;fn=w&63
   if op in (2,3) or op==0 and fn in (8,9):
    delay=instruction(pc+4,tags,seed,owner)
    if op==3 or op==0 and fn==9:
     work.append((pc+8,frozenset(t for t in delay if 16<=t<=23 or t==28)))
    elif op==2:work.append((((pc+4)&0xf0000000)|((w&0x3ffffff)<<2),delay))
    continue
   if op in (1,4,5,6,7,20,21,22,23) or op==17 and rs==8:
    imm=w&65535;imm=imm-65536 if imm&32768 else imm;target=pc+4+imm*4
    delay=instruction(pc+4,tags,seed,owner)
    if not (op in (5,21) and rs==rt):work.append((target,delay))
    if not (op in (4,20) and rs==rt):work.append((pc+8,tags if op in (20,21,22,23) else delay))
   else:work.append((pc+4,instruction(pc,tags,seed,owner)))
refs=list({(r['lui'],r['use']):r for r in refs}.values())
report=dict(old_banks=10,new_banks=32,bank_stride=0x12000,old_view_size=0xb5ae0,new_view_size=0x241b40,metadata_growth_with_padding=96,constant_candidates=refs,loop_candidates=bounds,note='Control-flow provenance; review owner ranges and unresolved uses before applying')
(a.out/'graphics-cache-candidates.json').write_text(json.dumps(report,indent=2)+'\n')
print('field/size uses',len(refs),'LUI definitions',len({r['lui'] for r in refs}),'loop candidates',bounds)
for r in refs:print(r['lui'],r['use'],r['old'],'->',r['new'])
