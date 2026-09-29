"""Record original gameplay group calls, without simulating unrelated frame systems."""
from pathlib import Path
import re,json,hashlib
root=Path(__file__).resolve().parents[1];path=root/'local/output/sub_002306A8_0x2306a8.cpp';source=path.read_text()
ins={int(a,16):i for a,i in re.findall(r'// 0x([0-9a-f]+): (.*)',source)}
expected=[(0x230c64,'354F98',1),(0x230c70,'354F98',5),(0x230c7c,'354F98',6),(0x230c98,'355028',1),(0x230ccc,'354F98',2),(0x230ce8,'354F98',3),(0x230d14,'354C98',1)]
rows=[]
for pc,target,group in expected:
 assert 'func_'+target in ins[pc]
 assert f'$a1, $zero, 0x{group:X}' in ins[pc+4]
 rows.append(dict(pc=pc,target=int(target,16),group=group))
out=root/'local/browser-pickups/component-frame-order.json'
out.write_text(json.dumps(dict(source_sha256=hashlib.sha256(source.encode()).hexdigest(),calls=rows,scope='Static order in the unpaused branch with debug suppression cleared; other world/frame calls occur between entries, and collection callback timing remains unverified'),indent=2)+'\n')
print('Verified original gameplay update/secondary/flush group order in230C64..230D18')
