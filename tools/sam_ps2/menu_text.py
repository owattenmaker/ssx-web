from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
folder=ROOT/'local/sam-ps2/original';output=ROOT/'local/sam-ps2/replacements'
for name in ['CMNAMER.LOC','FEAMER.LOC','OVAMER.LOC']:
 data=bytearray((folder/name).read_bytes())
 def replace(old,new):
  a=old.encode('utf-16le');b=new.ljust(len(old)).encode('utf-16le');assert len(a)==len(b),(old,new)
  assert a in data,old
  data[:]=data.replace(a,b)
 if name=='FEAMER.LOC':
  replace("MacKenzie 'Mac' Fraser","Sam 'Chopped Unc'")
  replace("Mac 'Smack' Fraser","Rope Tow Regular")
  text=bytes(data).decode('utf-16le',errors='surrogatepass');start=text.index('Mac has always');end=text.index('\0',start)
  replace(text[start:end],"Wisconsin raised Sam on the flat mountains of the Midwest. At 5'11 and 160 lbs, this regular-stance chopped unc prefers going uphill to going down. His riding is questionable, his fear of getting inverted is not. He brings an olive vest, a strong moustache and absolutely no intention of landing upside down. The rope tow is his home mountain, and making it back to the lodge counts as a podium finish.")
  # Each profile column stores ten roster entries; Mac is entry five.
  def profile(label,new):
   text=bytes(data).decode('utf-16le',errors='surrogatepass');at=text.index(label+'\0\0')+len(label)+2
   for _ in range(4):at=text.index('\0\0',at)+2
   end=text.index('\0',at);old=text[at:end];assert len(new)<=len(old),(label,old,new)
   a=at*2;data[a:end*2]=new.ljust(len(old)).encode('utf-16le')
  profile('Height:',"5'11")
  profile('Weight:','160lbs')
  profile('Age:','--')
 if 'Mac Fraser'.encode('utf-16le') in data:replace('Mac Fraser','Sam')
 # Whole-word names only, so unrelated internal words and credits stay intact.
 text=bytes(data).decode('utf-16le',errors='surrogatepass')
 text=re.sub(r'\bMac\b','Sam',text);text=re.sub(r'\bMAC\b','SAM',text)
 result=text.encode('utf-16le',errors='surrogatepass');assert len(result)==len(data)
 (output/name).write_bytes(result)
 print(name,'updated without relocating strings')
