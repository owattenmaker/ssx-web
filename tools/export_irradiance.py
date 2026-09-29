#!/usr/bin/env python3
"""Export owned original irradiance coefficients and course Lighting references.

`ara1_lighting` (unchanged) holds ARA1's Lighting painter section. `lighting_by_location`
maps every tools/locations.py code to the same records for its course painter record,
plus the section's point tree (`tree`) and the payload the tree selects at the AIP start
grid (`start`, tools/course_painters.py); web/prepare-environment.py uses it.

Usage: python3 tools/export_irradiance.py [--output DIR]
"""
import argparse,hashlib,json,math,struct
from pathlib import Path
from world_assets import world_chunks,records,locations
from import_sky import chunk_range,painter_sections
ROOT=Path(__file__).resolve().parents[1]
def decode_bank(data,endian):
 if len(data)<4:raise ValueError('Truncated irradiance bank')
 count=struct.unpack_from(endian+'I',data)[0]
 if not 0<count<=4096 or len(data)!=4+count*168:raise ValueError('Irradiance bank extent mismatch')
 result={}
 for i in range(count):
  name=data[4+i*8:12+i*8].rstrip(b'\0').decode('ascii');at=4+count*8+i*160
  if not name or name in result:raise ValueError('Duplicate/empty irradiance name')
  bits=struct.unpack_from(endian+'40I',data,at);values=struct.unpack_from(endian+'40f',data,at)
  if not all(math.isfinite(x)for x in values):raise ValueError('Nonfinite irradiance coefficient')
  result[name]=dict(index=i,offset=at,coefficient_bits=list(bits),rows=[list(values[j:j+4])for j in range(0,40,4)])
 return result

def course_lighting(source,code,bank,sections=False):
 """Lighting (type 11) sections of the course location's painter record(s)."""
 _,begin,end=chunk_range(locations(source/'bam.sdb'),code);lighting=[];raw=[]
 for chunk_id,chunk in enumerate(world_chunks(source/'bam.ssb')):
  if chunk_id>end:break
  if chunk_id<begin:continue
  for kind,track,rid,data in records(chunk):
   if kind!=15 or len(data)<64:continue
   section=painter_sections(data).get(11)
   if section is None:continue
   header,count,_=struct.unpack_from('<3I',section)
   if header!=12+8*count:raise ValueError('Lighting table header changed')
   entries=[]
   for i in range(count):
    typ,at=struct.unpack_from('<2I',section,12+i*8)
    if typ!=11 or at+44>len(section):raise ValueError('Invalid Lighting payload')
    rate=struct.unpack_from('<f',section,at)[0];references=[section[at+4+j*8:at+12+j*8].rstrip(b'\0').decode('ascii')for j in range(4)];scalars=list(struct.unpack_from('<2f',section,at+36))
    if not all(math.isfinite(x)for x in [rate,*scalars]) or any(name not in bank for name in references):raise ValueError('Lighting payload references unavailable data')
    entries.append(dict(index=i,offset=at,blend_rate=rate,references=references,scalars=scalars))
   lighting.append(dict(track=track,rid=rid,chunk=chunk_id,section_sha256=hashlib.sha256(section).hexdigest(),entries=entries));raw.append(section)
 if len(lighting)!=1:raise ValueError(f'Expected one {code} Lighting section')
 return raw if sections else lighting

def main(output=None):
 ps2=(ROOT/'local/assets/source/ps2/irr.dat').read_bytes();gc=(ROOT/'local/gamecube/disc/files/data/worlds/irrngc.dat').read_bytes();bank=decode_bank(ps2,'<');other=decode_bank(gc,'>')
 if bank.keys()!=other.keys() or any(v['coefficient_bits']!=other[k]['coefficient_bits'] for k,v in bank.items()):raise ValueError('Cross-console irradiance coefficients differ')
 source=ROOT/'local/assets/source/ps2';lighting=course_lighting(source,'ARA1',bank)
 from locations import LOCATIONS
 from course_painters import point_tree,start_selection
 by_location={}
 for code in LOCATIONS:
  rows=lighting if code=='ARA1' else course_lighting(source,code,bank);extended=[]
  for row,section in zip(rows,course_lighting(source,code,bank,sections=True)):
   tree=point_tree(section,len(row['entries']));selection=start_selection(code,tree,source)
   start=None if selection is None else dict(entry=selection['entry'],position_cm=selection['position'],uniform_near_start=selection['uniform'],cell_size_cm=selection['cell_size_cm'])
   extended.append(dict(row,location=code,uniform=all((e['references'],e['scalars'])==(row['entries'][0]['references'],row['entries'][0]['scalars'])for e in row['entries']),tree=tree,start=start))
  by_location[code]=extended
 out=Path(output) if output else ROOT/'local/assets/native/IRRADIANCE';out.mkdir(parents=True,exist_ok=True)
 result=dict(version=1,source='PS2 DATA/WORLDS/IRR.DAT',ps2_sha256=hashlib.sha256(ps2).hexdigest(),gamecube_sha256=hashlib.sha256(gc).hexdigest(),all_coefficient_bits_match=True,records=bank,ara1_lighting=lighting,scope='Raw named coefficients and original Lighting payload references; normal-basis evaluation and renderer binding remain unrecovered.',lighting_by_location=by_location)
 (out/'irradiance.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(records=len(bank),coefficient_words=sum(len(x['coefficient_bits'])for x in bank.values()),lighting_payloads=len(lighting[0]['entries']),references=lighting[0]['entries'][0]['references'])))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter);p.add_argument('--output',type=Path,help='Override local/assets/native/IRRADIANCE (e.g. a scratch dir)');main(p.parse_args().output)
