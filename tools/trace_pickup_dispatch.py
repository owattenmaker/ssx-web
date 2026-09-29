"""Record source/runtime evidence for indirect rider contact dispatch."""
import struct,zipfile,re,json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:m=z.read('eeMemory.bin')
rider=0x14701a0;interface=rider+0x6c0;vtable=struct.unpack_from('<I',m,interface)[0]
adjust=struct.unpack_from('<h',m,vtable+0x18)[0];target=struct.unpack_from('<I',m,vtable+0x1c)[0]
assert interface+adjust==rider and target==0x108c28
callers=[]
for p in (root/'local/output').glob('*.cpp'):
 s=p.read_text()
 if '0x6C0(' not in s:continue
 lines=re.findall(r'// 0x([0-9a-f]+): (.*)',s)
 for i,(a,ins) in enumerate(lines):
  if 'lw ' not in ins or '0x6C0(' not in ins:continue
  register=ins.split('$')[1].split(',')[0];near=lines[i+1:i+10]
  if any(f'0x1C(${register})' in text for _,text in near):callers.append(dict(source=p.name,start=int(a,16),instructions=[dict(pc=int(pc,16),instruction=text) for pc,text in [(a,ins)]+near]))
assert {c['start'] for c in callers}=={0x30c10c,0x301ee0}
output=dict(snapshot_sha256=hashlib.sha256(m).hexdigest(),rider=rider,interface=interface,vtable=vtable,this_adjustment=adjust,membership_target=target,callers=callers,scope='Contact predicate dispatch only; pickup reward actions not yet identified')
(root/'local/browser-pickups/contact-dispatch.json').write_text(json.dumps(output,indent=2)+'\n')
print('Verified interface this-adjustment and two indirect contact predicate callers')
