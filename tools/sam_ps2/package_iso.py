"""Build an independent Sam PS2 disc; append archives and update ISO extents."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc,inspect_big,SECTOR
ROOT=Path(__file__).resolve().parents[2]
source=Path.home()/'Downloads/SSX 3 (USA).iso'
output=Path.home()/'Downloads/SSX 3 - Sam Edition.iso'
replacements=ROOT/'local/sam-ps2/replacements'
d=Disc(source)
assert hashlib.sha1(d.file('SLUS_207.72')).hexdigest()=='77114dfd1205eaccf1ccc18c5f9650097fa78bd8'
changes={}
for name in ['DATA/CHAR/MDLPS2.BIG','DATA/CHAR/MACTXP.BIG','DATA/UI/CHARPIC.BIG']:
 entry=next(e for e in d.entries if e['path']==name);archive=inspect_big(d,entry)
 files=[]
 for e in archive['files']:
  replace=replacements/e['path']
  if name.endswith('MACTXP.BIG') and e['path'].startswith('mac_suit_a01_'):replace=replacements/'sam.ssh'
  payload=replace.read_bytes() if replace.is_file() else d.read(e['disc_offset'],e['size'])
  files.append((e['path'],payload))
  if replace.is_file():changes[e['path']]={'size':len(payload),'sha256':hashlib.sha256(payload).hexdigest()}
 end=16+sum(9+len(n.encode('ascii')) for n,p in files)
 data=bytearray((end+2047)//2048*2048);directory=bytearray()
 for n,p in files:
  directory+=struct.pack('>II',len(data),len(p))+n.encode('ascii')+b'\0'
  data+=p;data+=bytes((-len(data))%2048)
 data[:16]=b'BIGF'+struct.pack('<I',len(data))+struct.pack('>II',len(files),end)
 data[16:16+len(directory)]=directory
 target=ROOT/'local/sam-ps2'/Path(name).name;target.write_bytes(data)
 if name.endswith('MDLPS2.BIG'):assert 'mac_TopA.mpf' in changes
assert 'mac_suit_a01_a01.ssh' in changes
if output.exists() and '--refresh' not in sys.argv:raise SystemExit('Output exists; use --refresh only while emulator is stopped')
if not output.exists():subprocess.run(['cp','-c',str(source),str(output)],check=True)
with output.open('r+b') as out:
 def update_dir(at,size,parent=''):
  out.seek(at);buf=bytearray(out.read(size));pos=0
  while pos<size:
   length=buf[pos]
   if not length:pos=(pos//2048+1)*2048;continue
   raw=buf[pos+33:pos+33+buf[pos+32]]
   if raw not in (b'\0',b'\1'):
    name=raw.decode('ascii').split(';')[0];path=(parent+'/'+name).lstrip('/')
    extent=int.from_bytes(buf[pos+2:pos+6],'little');lengthbytes=int.from_bytes(buf[pos+10:pos+14],'little')
    if buf[pos+25]&2:update_dir(extent*2048,lengthbytes,path)
    elif path in locations:
     sector,lengthbytes=locations[path];buf[pos+2:pos+10]=struct.pack('<I',sector)+struct.pack('>I',sector);buf[pos+10:pos+18]=struct.pack('<I',lengthbytes)+struct.pack('>I',lengthbytes)
   pos+=length
  out.seek(at);out.write(buf)
 for name in ['CMNAMER.LOC','FEAMER.LOC','OVAMER.LOC']:
  e=next(e for e in d.entries if e['path']=='DATA/LOCALE/'+name);data=(replacements/name).read_bytes();assert len(data)==e['size'];out.seek(e['offset']);out.write(data)
 locations={}
 for path in ['DATA/CHAR/MDLPS2.BIG','DATA/CHAR/MACTXP.BIG','DATA/UI/CHARPIC.BIG']:
  out.seek(0,2);out.write(bytes((-out.tell())%2048));sector=out.tell()//2048
  data=(ROOT/'local/sam-ps2'/Path(path).name).read_bytes();out.write(data);locations[path]=(sector,len(data))
 out.seek(0,2);sectors=out.tell()//2048
 out.seek(16*2048);pvd=bytearray(out.read(2048));record=pvd[156:]
 update_dir(int.from_bytes(record[2:6],'little')*2048,int.from_bytes(record[10:14],'little'))
 pvd[80:88]=struct.pack('<I',sectors)+struct.pack('>I',sectors);out.seek(16*2048);out.write(pvd)
verify=Disc(output)
for name in ['CMNAMER.LOC','FEAMER.LOC','OVAMER.LOC']:
 assert verify.file('DATA/LOCALE/'+name)==(replacements/name).read_bytes()
for name in locations:
 archive=inspect_big(verify,next(e for e in verify.entries if e['path']==name))
 for e in archive['files']:
  if e['path'] in changes:assert hashlib.sha256(verify.read(e['disc_offset'],e['size'])).hexdigest()==changes[e['path']]['sha256']
(ROOT/'local/sam-ps2/install-manifest.json').write_text(json.dumps({'source':str(source),'output':str(output),'changes':changes,'verified_archive_payloads':True},indent=2)+'\n')
print(output)
