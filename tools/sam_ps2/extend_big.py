"""Append Sam resources without moving any original BIG member payload."""
from pathlib import Path
import struct,json,hashlib,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
from compare_character_assets import big_members
ROOT=Path(__file__).resolve().parents[2]
def extend(original,added):
 assert original[:4]==b'BIGF'
 count,end=struct.unpack_from('>II',original,8);at=16;first=len(original)
 old=dict(big_members(original));assert not ({n.lower() for n in old}&{n.lower() for n in added})
 for _ in range(count):
  offset,size=struct.unpack_from('>II',original,at)
  if size:first=min(first,offset)
  at=original.index(0,at+8,end)+1
 footer=original[at:end];directory=bytearray(original[16:at]);data=bytearray(original)
 for name,payload in sorted(added.items()):
  data+=bytes((-len(data))%2048);offset=len(data);data+=payload
  directory+=struct.pack('>II',offset,len(payload))+name.encode('ascii')+b'\0'
 new_end=16+len(directory)+len(footer)
 if new_end>first:raise ValueError('New directory exceeds original padding; refusing to overwrite a member')
 data[:16]=b'BIGF'+struct.pack('<I',len(data))+struct.pack('>II',count+len(added),new_end)
 data[16:new_end]=directory+footer
 decoded=dict(big_members(data))
 assert all(decoded[n]==b for n,b in old.items()) and all(decoded[n]==b for n,b in added.items())
 return bytes(data),dict(original_members_preserved=len(old),new_members=len(added),original_payload_offsets_unchanged=True)
if __name__=='__main__':
 folder=ROOT/'local/sam-ps2/roster/sam-assets';d=Disc(Path.home()/'Downloads/SSX 3 (USA).iso');report={}
 for name,added in [('MDLPS2.BIG',{p.name:p.read_bytes() for p in folder.glob('sam_*.mpf')}),('MACTXP.BIG',{n:(folder/n).read_bytes() for n in ['sam_textures.ssh','sam_icons.ssh','sam_uphill.ssh']})]:
  assert added
  result,checks=extend(d.file('DATA/CHAR/'+name),added);(folder/name).write_bytes(result)
  checks['sha256']=hashlib.sha256(result).hexdigest();report[name]=checks
 (folder/'archive-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
