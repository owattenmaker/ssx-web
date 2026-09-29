"""Extract bounded original course LUN programs (--location, default ARA1); do not infer execution semantics."""
import sys,struct,json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from world_assets import world_chunks,records,locations
import argparse
from locations import location as location_info,pickup_file,pickups_dir
LOCATION=(lambda p:(p.add_argument('--location',default='ARA1'),p.parse_args().location)[1])(argparse.ArgumentParser(description=__doc__));location_info(LOCATION)
# The course stage (SSB kind 16) lives in the location's last chunk under its own track (ARA1: chunk 33, track 8).
locs=locations(root/'local/assets/source/ps2/bam.sdb');TRACK=next(i for i,l in enumerate(locs) if l['name']==LOCATION);LAST=locs[TRACK]['chunk_end']
stage=None
for chunk,body in enumerate(world_chunks(root/'local/assets/source/ps2/bam.ssb')):
 if chunk==LAST:
  stage=next(data for kind,track,rid,data in records(body) if kind==16 and track==TRACK);break
assert stage is not None
u=lambda p:struct.unpack_from('<I',stage,p)[0]
count,table=u(0x38),u(0x3c);end=u(0x40)
assert (count==238 or LOCATION!='ARA1') and table+count*4<=end<=len(stage)
offsets=list(struct.unpack_from('<'+str(count)+'I',stage,table))+[end]
# Programs start at the end of the offset table, 16-byte aligned with zero padding (BRA2 pads 4 bytes).
assert offsets==sorted(set(offsets)) and offsets[0]==(table+count*4+15)&~15 and not any(stage[table+count*4:offsets[0]])
programs=[]
for i,(start,stop) in enumerate(zip(offsets,offsets[1:])):
 magic,codeEnd,extent,other=struct.unpack_from('<4I',stage,start)
 assert magic==0x004e554c,(i,hex(start),hex(magic))
 # A program fills its slot exactly, except zero alignment padding (<16 bytes) after the last one (BRA2/BHP1).
 assert 16<=codeEnd<=extent<=stop-start and (extent==stop-start or (i==count-1 and stop-start-extent<16 and not any(stage[start+extent:stop]))) and codeEnd%4==0 and other==extent,(i,codeEnd,extent,stop-start,other)
 stop=start+extent
 code=stage[start+16:start+codeEnd]
 programs.append(dict(index=i,offset=start,extent=extent,code_start=start+16,code_end=start+codeEnd,code_words=list(struct.unpack('<'+str(len(code)//4)+'I',code)),trailer_words=list(struct.unpack('<'+str((extent-codeEnd)//4)+'I',stage[start+codeEnd:stop])),sha256=hashlib.sha256(stage[start:stop]).hexdigest()))
out=pickups_dir(LOCATION);out.mkdir(parents=True,exist_ok=True)
pickup_file(LOCATION,'scripts').write_text(json.dumps(dict(**({} if LOCATION=='ARA1' else dict(location=LOCATION,track=TRACK,chunk=LAST)),stage_sha256=hashlib.sha256(stage).hexdigest(),program_count=count,programs=programs,scope='LUN header/code/trailer boundaries checked; opcode operands and execution semantics not yet fully decoded'),indent=2)+'\n')
print('Extracted',len(programs),'bounded original LUN programs')
