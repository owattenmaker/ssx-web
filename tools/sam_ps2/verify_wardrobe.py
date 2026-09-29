"""Verify the preservation-oriented wardrobe output without rebuilding it."""
from pathlib import Path
from extend_wardrobe import parse,strings,ROOT
old=parse((ROOT/'local/sam-ps2/original/BOLTPS2.DAT').read_bytes())
new=parse((ROOT/'local/sam-ps2/roster/sam-assets/BOLTPS2-preserved.DAT').read_bytes())
assert new[0]==old[0] and new[1][:len(old[1])]==old[1]
assert new[2][0][:len(old[2][0])]==old[2][0] and all(b[:len(a)]==a for a,b in zip(old[2][1:],new[2][1:]))
assert new[3][:len(old[3])]==old[3] and new[4]==old[4]
added=new[1][len(old[1]):];assert len(added)==443 and all(r[0]==30 for r in added)
import struct
mac={struct.unpack_from('<h',r,4)[0]:strings(r,old[3])[5] for r in old[1] if r[0]==3}
for row in added:
 model=strings(row,new[3])[5];item=struct.unpack_from('<h',row,4)[0]
 # Sam's own parts, Mac accessories moved to Sam's head, or the Mac row's own accessory model
 assert not model or b'|sam_' in model or b'|samfit_' in model or b'|sm' in model or model==mac.get(item),(item,model)
print('Original rows, offsets, compatibility order and trailer preserved; Sam namespace verified')
