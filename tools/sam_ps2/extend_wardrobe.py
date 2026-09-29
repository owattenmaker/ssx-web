"""Append Sam rows while retaining all original rows/string offsets/trailer."""
from pathlib import Path
import struct,json
from collections import Counter
ROOT=Path(__file__).resolve().parents[2]
def parse(data):
 count=struct.unpack_from('<I',data,4)[0];rows=[data[8+i*56:8+(i+1)*56] for i in range(count)];at=8+count*56;tables=[]
 for stride in (12,8,8):
  n=struct.unpack_from('<I',data,at)[0];at+=4;tables.append([data[at+i*stride:at+(i+1)*stride] for i in range(n)]);at+=n*stride
 n=struct.unpack_from('<I',data,at)[0];at+=4
 if at+n>len(data):raise ValueError('Truncated string pool')
 return data[:4],rows,tables,data[at:at+n],data[at+n:]
def strings(row,pool):
 out=[]
 for off in struct.unpack_from('<8I',row,20):
  if not off:out.append(None);continue
  if off>len(pool):raise ValueError('String outside declared pool')
  out.append(pool[off-1:pool.index(b'\0',off-1)])
 return out
if __name__=='__main__':
 old=parse((ROOT/'local/sam-ps2/original/BOLTPS2.DAT').read_bytes());folder=ROOT/'local/sam-ps2/roster/sam-assets'
 candidate=parse((folder/'BOLTPS2.DAT').read_bytes())
 assert len(old[1])==4829 and len(candidate[1])==4829+443
 assert Counter(candidate[2][0][:len(old[2][0])])==Counter(old[2][0])
 for a,b in zip(old[2][1:],candidate[2][1:]):assert b[:len(a)]==a
 # Original C# candidate must agree semantically before any new rows are accepted.
 for a,b in zip(old[1],candidate[1]):
  assert a[:20]+a[52:]==b[:20]+b[52:]
  assert strings(a,old[3])==strings(b,candidate[3])
 pool=bytearray(old[3]);lookup={}
 for row in old[1]:
  for off,text in zip(struct.unpack_from('<8I',row,20),strings(row,old[3])):
   if text is not None:lookup.setdefault(text,off)
 added=[]
 for source in candidate[1][len(old[1]):]:
  row=bytearray(source);assert row[0]==30
  for i,text in enumerate(strings(source,candidate[3])):
   if text is None:off=0
   else:
    if text not in lookup:lookup[text]=len(pool)+1;pool+=text+b'\0'
    off=lookup[text]
   struct.pack_into('<I',row,20+4*i,off)
  added.append(bytes(row))
 # Sam's rules: Mac's (bytes kept) as edited by the Equip Gear spec (tools/sam_wardrobe.py rules: no hood rows,
 # Sam's hair under hats) in the spec's order; without a spec they are exactly Mac's
 sam_rules=candidate[2][0][len(old[2][0]):]
 assert sam_rules and all(r[0]==30 for r in sam_rules)
 mac_rules=[bytes([30])+r[1:] for r in old[2][0] if r[0]==3]
 if not (ROOT/'local/sam-model/wardrobe-spec.json').exists():assert Counter(sam_rules)==Counter(mac_rules)
 extra_tables=[]
 for table,newtable in zip(old[2][1:],candidate[2][1:]):
  sam=[struct.pack('<I',30)+r[4:] for r in table if struct.unpack_from('<I',r)[0]==3]
  assert newtable[len(table):]==sam
  extra_tables.append(table+sam)
 tables=[old[2][0]+sam_rules,*extra_tables]
 data=old[0]+struct.pack('<I',len(old[1])+len(added))+b''.join(old[1]+added)
 for table in tables:data+=struct.pack('<I',len(table))+b''.join(table)
 data+=struct.pack('<I',len(pool))+pool+old[4]
 check=parse(data)
 assert check[1][:4829]==old[1] and check[2][0][:len(old[2][0])]==old[2][0]
 assert all(b[:len(a)]==a for a,b in zip(old[2][1:],check[2][1:])) and check[3][:len(old[3])]==old[3] and check[4]==old[4]
 mac={struct.unpack_from('<h',r,4)[0]:strings(r,old[3])[5] for r in old[1] if r[0]==3}
 for row in check[1][4829:]:
  values=strings(row,check[3]);item=struct.unpack_from('<h',row,4)[0]
  assert not values[5] or b'|sam_' in values[5] or b'|samfit_' in values[5] or b'|sm' in values[5] or values[5]==mac.get(item)
 (folder/'BOLTPS2-preserved.DAT').write_bytes(data)
 report=dict(sam_rules=len(sam_rules),original_item_rows_byte_identical=4829,original_compatibility_rows_byte_identical=len(old[2][0]),original_string_pool_prefix_identical=len(old[3]),original_trailer_identical=len(old[4]),sam_item_rows=len(added),other_original_table_prefixes_identical=True,sam_extra_table_rows=[len(b)-len(a) for a,b in zip(old[2][1:],check[2][1:])])
 (folder/'wardrobe-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
