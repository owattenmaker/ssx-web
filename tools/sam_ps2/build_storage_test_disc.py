from pathlib import Path
import hashlib,json,struct,sys
from iso_patch import build_disc,Disc
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
source=Path.home()/'Downloads/SSX 3 (USA).iso'
menu='--menu' in sys.argv
output=folder/(sys.argv[sys.argv.index('--output')+1] if '--output' in sys.argv else ('menu-test.iso' if menu else 'storage-test.iso'))
storage=folder/'fx-filter' if '--fx-filter' in sys.argv else folder/'profiles' if '--profiles' in sys.argv else folder/'bolts' if '--bolt-runtime' in sys.argv else folder/'preview' if '--preview' in sys.argv else folder/'attributes' if '--attributes' in sys.argv else folder
if '--allocation-probe' in sys.argv:storage=folder/'allocation-probe'
if '--mesh-probe' in sys.argv:storage=folder/'mesh-probe'
if '--bio' in sys.argv:storage=folder/'bio'
if '--save-extension' in sys.argv:
 assert '--bio' in sys.argv
 storage=folder/'save-extension'
d=Disc(source);elf=bytearray(d.file('SLUS_207.72'));report=json.loads((storage/'character-storage-patches.json').read_text())
assert hashlib.sha1(elf).hexdigest()==report['original_elf_sha1']
if menu:
 report['patches']+= [
  dict(address='0x181350',expected='0a00022a',replacement='0b00022a',purpose='Search all eleven display-order entries'),
  dict(address='0x1817c0',expected='0a00022a',replacement='0b00022a',purpose='Recognize the new colon-named widget as display index10'),
  dict(address='0x440f72',expected='00',replacement='1e',purpose='Append Sam ID30 in verified table padding; original ten bytes unchanged')]

if '--assets' in sys.argv:
 report['patches'].append(dict(address='0x181f3c',expected='0a00422c',replacement='1f00422c',purpose='Allow primary Sam ID30 through preview update eligibility; normal IDs0-9 unchanged'))
if '--graphics-cache' in sys.argv:
 report['patches']+=json.loads((folder/'graphics-cache-patches.json').read_text())['patches']
if '--patches-file' in sys.argv:
 report['patches']+=json.loads(Path(sys.argv[sys.argv.index('--patches-file')+1]).read_text())['patches']
phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
for patch in report['patches']:
 at=int(patch['address'],16);old=bytes.fromhex(patch['expected']);new=bytes.fromhex(patch['replacement']);matched=False
 for i in range(phnum):
  typ,off,va,_,size,_,_,_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1 and va<=at and at+len(old)<=va+size:
   p=off+at-va;assert elf[p:p+len(old)]==old;elf[p:p+len(old)]=new;matched=True;break
 assert matched
payload=(storage/'CHARDB-expanded-candidate.DBL').read_bytes();assert hashlib.sha256(payload).hexdigest()==report['payload_sha256']
replacements={'SLUS_207.72':elf,'DATA/BE/CHARDB.DBL':payload}
if menu:replacements['DATA/UI/FE.LUI']=(folder/('FE-roster-icons-candidate.LUI' if '--roster-icons' in sys.argv else 'FE-roster-candidate.LUI')).read_bytes()
if '--roster-icons' in sys.argv:
 assert menu
 replacements['DATA/UI/FE_1.SSH']=(folder/'FE_1-roster.SSH').read_bytes()
if menu:replacements['DATA/LOCALE/FEAMER.LOC']=(folder/('FEAMER-roster-bio-candidate.LOC' if '--bio' in sys.argv else 'FEAMER-roster-candidate.LOC')).read_bytes()
if menu:replacements['DATA/LOCALE/CMNAMER.LOC']=(folder/'CMNAMER-roster-candidate.LOC').read_bytes()
if '--assets' in sys.argv:
 assets=folder/'sam-assets'
 if '--stock-wardrobe' not in sys.argv:replacements['DATA/CHAR/BOLTPS2.DAT']=(assets/'BOLTPS2-preserved.DAT').read_bytes()
 for name in ['MDLPS2.BIG','MACTXP.BIG']:replacements['DATA/CHAR/'+name]=(assets/name).read_bytes()
if '--wardrobe-file' in sys.argv:
 replacements['DATA/CHAR/BOLTPS2.DAT']=Path(sys.argv[sys.argv.index('--wardrobe-file')+1]).read_bytes()
build_disc(source,output,replacements)
(folder/('menu-disc.json' if menu else 'storage-disc.json')).write_text(json.dumps(dict(output=str(output),elf_sha256=hashlib.sha256(elf).hexdigest(),changes=list(replacements),patches=report['patches'],purpose='Menu wiring experiment; Sam equipment/progression not integrated' if menu else 'Original roster regression of new character-storage mechanism; no Sam slot exposed yet'),indent=2)+'\n')
print(output)
