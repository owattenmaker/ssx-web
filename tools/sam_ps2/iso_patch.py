"""Append replacements to a separate ISO9660 disc, retaining all other files."""
from pathlib import Path
import struct,subprocess,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc

def build_disc(source,output,replacements):
 source,output=Path(source),Path(output)
 if source.resolve()==output.resolve() or output.exists():raise ValueError('Requires a new output disc')
 original=Disc(source)
 assert set(replacements)<=set(e['path'] for e in original.entries)
 subprocess.run(['cp','-c',str(source),str(output)],check=True)
 with output.open('r+b') as f:
  locations={}
  for name,data in replacements.items():
   f.seek(0,2);f.write(bytes((-f.tell())%2048));sector=f.tell()//2048
   f.write(data);f.write(bytes((-f.tell())%2048));locations[name]=(sector,len(data))
  f.seek(0,2);total=f.tell()//2048
  def directory(at,size,parent=''):
   f.seek(at);data=bytearray(f.read(size));p=0
   while p<size:
    n=data[p]
    if not n:p=(p//2048+1)*2048;continue
    if n<34 or p+n>size:raise ValueError('Malformed directory')
    name=data[p+33:p+33+data[p+32]]
    if name not in (b'\0',b'\1'):
     path=(parent+'/'+name.decode().split(';')[0]).lstrip('/')
     sector,length=struct.unpack_from('<I',data,p+2)[0],struct.unpack_from('<I',data,p+10)[0]
     if data[p+25]&2:directory(sector*2048,length,path)
     elif path in locations:
      sector,length=locations.pop(path)
      data[p+2:p+10]=struct.pack('<I',sector)+struct.pack('>I',sector)
      data[p+10:p+18]=struct.pack('<I',length)+struct.pack('>I',length)
    p+=n
   f.seek(at);f.write(data)
  f.seek(16*2048);pvd=bytearray(f.read(2048));r=pvd[156:]
  directory(int.from_bytes(r[2:6],'little')*2048,int.from_bytes(r[10:14],'little'))
  assert not locations
  pvd[80:88]=struct.pack('<I',total)+struct.pack('>I',total);f.seek(16*2048);f.write(pvd)
 check=Disc(output)
 for name,data in replacements.items():assert check.file(name)==data
 for old in original.entries:
  if old['path'] not in replacements:
   new=next(e for e in check.entries if e['path']==old['path'])
   assert (new['offset'],new['size'])==(old['offset'],old['size'])
 original.close();check.close()
