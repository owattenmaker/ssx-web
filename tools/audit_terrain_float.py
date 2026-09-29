#!/usr/bin/env python3
"""Compare source-operation terrain refinement with recorded original contacts."""
import json,struct,subprocess,zipfile,sys
from pathlib import Path
from reference_probes import riders
def main():
    root=Path(__file__).resolve().parents[1];folder=root/'local/reference/terrain'
    patches={x['resource_id']:x for x in json.loads((root/'local/assets/native/ARA1/terrain.json').read_text())['patches']}
    states=[Path(x) for x in sys.argv[1:]] if len(sys.argv)>1 else [root/'local/reference/pcsx2'/('snow-jam-'+x+'.p2s') for x in ('glide','glide-1','glide-30-a','glide-120','brake-30','charge-15','jump-30','turn-left-30')]
    reports=[];rows=[]
    for state in states:
     with zipfile.ZipFile(state) as z:m=z.read('eeMemory.bin')
     r=next(x for x in riders(m) if x['kind']=='human');b=int(r['address'],16)
     def vector(o):return struct.unpack_from('<3f',m,b+o)
     rid=struct.unpack_from('<I',m,b+0x430)[0]
     if rid not in patches:continue
     p=patches[rid];coefficients=[[x*100,-z*100,y*100] for x,y,z in p['coefficients']]
     geometry=struct.unpack_from('<I',m,b+0x780)[0];scale=struct.unpack_from('<f',m,geometry+0x140)[0]
     uv=struct.unpack_from('<2f',m,b+0xaac);turn=r['controls']['turn_1f0']
     previous_lateral=vector(0x3b0);previous_state=None
     if state.name=='snow-jam-turn-left-30.p2s':
      previous_state=state.with_name('snow-jam-turn-left-29.p2s')
      if previous_state.exists():
       with zipfile.ZipFile(previous_state) as z:prior=z.read('eeMemory.bin')
       prior_base=int(next(x for x in riders(prior) if x['kind']=='human')['address'],16)
       assert vector(0x380)==struct.unpack_from('<3f',prior,prior_base+0x370),'Previous normal mismatch'
       previous_lateral=struct.unpack_from('<3f',prior,prior_base+0x3b0)
      else:previous_state=None
     row=[*(x for c in coefficients for x in c),*vector(0x110),*vector(0x380),*previous_lateral,turn,scale,*uv]
     rows.append(row);reports.append(dict(state=str(state),resource=rid,uv=uv,point=vector(0x460),normal=vector(0x370),turn=turn,previous_tangent_state=str(previous_state) if previous_state else None))
    fixture=folder/'live-contact-fixture.txt';fixture.write_text(str(len(rows))+'\n'+'\n'.join(' '.join(map(str,row)) for row in rows)+'\n')
    binary=folder/'terrain_live_contact'
    subprocess.run(['clang++','-std=c++20','-O2','-ffp-contract=off',str(root/'tests/terrain_live_contact.cpp'),'-o',str(binary)],check=True)
    output=subprocess.check_output([str(binary),str(fixture)],text=True)
    for line,r in zip(output.splitlines(),reports):
     values=list(map(float,line.split()));r['native_uv']=values[2:4];r['native_point']=values[4:7];r['native_normal']=values[7:10]
     for field in ('uv','point','normal'):r[field+'_max_error']=max(abs(a-b) for a,b in zip(r[field],r['native_'+field]))
     print(Path(r['state']).name,'uv',r['uv_max_error'],'pointcm',r['point_max_error'],'normal',r['normal_max_error'])
    (folder/'float-live-audit.json').write_text(json.dumps(dict(note='Uses original UV to identify the coarse seed cell; previous lateral tangent for turn30 comes from independently captured turn29.',cases=reports),indent=2)+'\n')


if __name__=='__main__':main()
