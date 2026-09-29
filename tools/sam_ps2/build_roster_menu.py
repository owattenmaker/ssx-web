"""Rebuild FE.LUI with the staged selection screen; preserve other screens."""
import base64,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
from world_assets import refpack
from lui_screen import Screen
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
def literal(data):
 if len(data)>=1<<24:raise ValueError('Refpack short-size limit')
 out=bytearray(b'\x10\xfb'+len(data).to_bytes(3,'big'));at=0
 while len(data)-at>=4:
  n=min(112,(len(data)-at)//4*4);out.append(0xe0+n//4-1);out+=data[at:at+n];at+=n
 out.append(0xfc+len(data)-at);out+=data[at:];assert refpack(bytes(out))==data
 return bytes(out)
def replace_screen(original,hash_value,replacement):
 _,_,_,table,objects,fonts=struct.unpack_from('<6I',original)
 count=struct.unpack_from('<I',original,table)[0]
 entries=[struct.unpack_from('<II',original,table+4+i*8) for i in range(count)]
 header=bytearray(struct.pack('<I',count)+bytes(count*8));payload=bytearray();hits=0
 for i,(key,offset) in enumerate(entries):
  end=table+entries[i+1][1] if i+1<count else objects
  old=original[table+offset:end]
  new=literal(replacement) if key==hash_value else old
  if key==hash_value:hits+=1
  struct.pack_into('<II',header,4+i*8,key,len(header)+len(payload));payload+=new
 assert hits==1
 rebuilt=bytearray(original[:table])+header+payload+original[objects:]
 shift=table+len(header)+len(payload)-objects
 struct.pack_into('<II',rebuilt,16,objects+shift,fonts+shift)
 # Every decoded screen and the object/font suffix must retain exact bytes.
 for i,(key,offset) in enumerate(entries):
  newkey,start=struct.unpack_from('<II',rebuilt,table+4+i*8);assert newkey==key
  end=struct.unpack_from('<I',rebuilt,table+4+(i+1)*8+4)[0]+table if i+1<count else objects+shift
  decoded=refpack(bytes(rebuilt[table+start:end]))
  oldend=table+entries[i+1][1] if i+1<count else objects
  expected=replacement if key==hash_value else refpack(original[table+offset:oldend])
  assert decoded==expected
 assert rebuilt[objects+shift:]==original[objects:]
 return bytes(rebuilt)
if __name__=='__main__':
 icons='--roster-icons' in sys.argv
 d=Disc(Path.home()/'Downloads/SSX 3 (USA).iso');original=d.file('DATA/UI/FE.LUI');candidate=(folder/('sam-roster-icons-screen-candidate.bin' if icons else 'sam-roster-screen-candidate.bin')).read_bytes()
 Screen.decode(candidate)
 result=replace_screen(original,0x0c23e1a2,candidate)
 if icons:
  from loc_file import name_hash
  result=bytearray(result);objects,fonts=struct.unpack_from('<II',result,16);n=struct.unpack_from('<I',result,objects)[0]
  assert objects+4+n*24==fonts
  rows=[struct.unpack_from('<II4f',result,objects+4+i*24) for i in range(n)]
  flags=next(r[1] for r in rows if r[0]==0x3757854);assert (flags>>8)&255==20
  flags=(flags&0xffff00ff)|(22<<8)
  extra=b''
  for name,x in [('SamRosterWhite',1),('SamRosterOrange',33)]:
   ident=name_hash(name);assert all(r[0]!=ident for r in rows)
   extra+=struct.pack('<II4f',ident,flags,(x+.5)/64,1.5/128,(x+27.5)/64,74.5/128)
  result[fonts:fonts]=extra;struct.pack_into('<I',result,objects,n+2);struct.pack_into('<I',result,20,fonts+len(extra))
 (folder/('FE-roster-icons-candidate.LUI' if icons else 'FE-roster-candidate.LUI')).write_bytes(result)
 from loc_file import append,name_hash
 locale=append(d.file('DATA/LOCALE/FEAMER.LOC'),name_hash('SAM_TAGLINE'),
  'A chopped unc from Wisconsin. Prefers uphill and keeps his head above his board.')
 if '--bio' in sys.argv:
  from rider_bio import locale_entries
  for key,value in locale_entries().items():locale=append(locale,name_hash(key),value)
 (folder/('FEAMER-roster-bio-candidate.LOC' if '--bio' in sys.argv else 'FEAMER-roster-candidate.LOC')).write_bytes(locale)
 (folder/'CMNAMER-roster-candidate.LOC').write_bytes(append(d.file('DATA/LOCALE/CMNAMER.LOC'),name_hash('SAM_NAME'),'Sam'))
 print(f'Rebuilt FE.LUI: {len(result)} bytes; 75 other screens and object/font data preserved')
