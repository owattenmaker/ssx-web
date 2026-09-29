"""Verify alternate material namespace and targeted wardrobe bindings."""
from pathlib import Path
import json,struct
from extend_wardrobe import parse,strings,ROOT
folder=ROOT/'local/sam-ps2/roster/sam-assets'
def blocks(data):
 count=struct.unpack_from('<I',data,8)[0]
 rows=[(data[16+i*8:20+i*8].decode(),struct.unpack_from('<I',data,20+i*8)[0]) for i in range(count)]
 return {name:data[start:(rows[i+1][1] if i+1<count else len(data))] for i,(name,start) in enumerate(rows)}
base=blocks((folder/'sam_textures.ssh').read_bytes());alt=blocks((folder/'sam_uphill.ssh').read_bytes())
assert set(base)=={f's{i:03}' for i in range(15)} and set(alt)=={f'u{i:03}' for i in range(15)}
changed=[i for i in range(15) if base[f's{i:03}']!=alt[f'u{i:03}']];assert changed==[1]
assert set(alt['u001'][19:16+128*128*4:4])=={128}
model=json.loads((folder/'uphill-model-verification.json').read_text());assert model['geometry_and_skinning_unchanged']
assert set(model['textures'])<=set(alt) and not set(model['textures'])&set(base)
wardrobe=parse((folder/'BOLTPS2-preserved.DAT').read_bytes());bindings=[]
for row in wardrobe[1][4829:]:
 item=struct.unpack_from('<h',row,4)[0];values=strings(row,wardrobe[3])
 if values[6] and b'sam_uphill' in values[6]:bindings.append(item)
 if item==52:assert values[5]==b'data/char/mdlps2.big|sam_top_uphill.mpf'
 if item==55:assert values[7]==b'su02'
assert bindings==[55,52]
report={'changed_image_payload_indices':changed,'unchanged_image_payloads':14,'disjoint_texture_names':True,'variant_model':model,'binding_items':bindings}
(folder/'uphill-variant-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print('Uphill binding contract verified: unique material names, preserved geometry, one changed atlas, two targeted wardrobe rows.')
