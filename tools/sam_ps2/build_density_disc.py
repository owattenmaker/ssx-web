"""Derive a new playtest disc with Sam's current model (tools/sam_mesh.py -> SamPS2 --parts/--sam-textures -> extend_big.py).

Clones the previous playtest source disc (never anything in ~/Downloads) and replaces only the Sam wardrobe
(BOLTPS2.DAT, extend_wardrobe.py) and the two Sam-extended archives; the ELF patches, roster UI, locale and CHARDB
of that build are kept byte-identical.
  python3 tools/sam_ps2/build_density_disc.py [--source NAME.iso] [--output NAME.iso]
"""
from pathlib import Path
import hashlib,json,sys
from iso_patch import build_disc,Disc
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
arg=lambda k,d:sys.argv[sys.argv.index(k)+1] if k in sys.argv else d
source=folder/arg('--source','sam-uphill-binding-v2-test.iso');output=folder/arg('--output','sam-density-v6-test.iso')
assert 'Downloads' not in str(output.resolve())
assets=folder/'sam-assets'
replacements={'DATA/CHAR/'+n:(assets/n).read_bytes() for n in ['MDLPS2.BIG','MACTXP.BIG']}
replacements['DATA/CHAR/BOLTPS2.DAT']=(assets/'BOLTPS2-preserved.DAT').read_bytes()
build_disc(source,output,replacements)
d=Disc(output);old=Disc(source)
unchanged=[e['path'] for e in old.entries if e['path'] not in replacements and d.file(e['path'])==old.file(e['path'])]
report=dict(output=str(output),source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),replaced={k:hashlib.sha256(v).hexdigest() for k,v in replacements.items()},
            unchanged_files=len(unchanged),total_files=len(old.entries),sha256=hashlib.sha256(output.read_bytes()).hexdigest(),purpose='Sam Equip Gear parts (every item of his bucket: Sam parts, Mac accessories fitted to his head, per-item textures) + roster-density model (sculpted head, hair sway, LODs, five maps in the PS2 texel domain, side/three-quarter head painting from the photos), hair in the default headwear slot, gloves on the FE/NIS hands item')
assert len(unchanged)==len(old.entries)-len(replacements)
(folder/'density-disc.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
