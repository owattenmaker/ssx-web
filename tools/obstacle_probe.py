#!/usr/bin/env python3
"""Export original rider collision spheres and verify authored instance bindings."""
import argparse,hashlib,json,math,struct,zipfile
from pathlib import Path
from reference_probes import riders


def probe(state,package):
    with zipfile.ZipFile(state) as z:memory=z.read('eeMemory.bin')
    def read(fmt,at):
        size=struct.calcsize('<'+fmt)
        if not 0<=at<=len(memory)-size:raise ValueError('Obstacle pointer outside EE RAM')
        return struct.unpack_from('<'+fmt,memory,at)
    def u(at):return read('I',at)[0]
    results=[];checks=[]
    for rider in riders(memory):
        base=int(rider['address'],16);shape=u(base+0xaa0)
        if not shape:continue
        count=u(shape+0x2c);mask=u(shape+0x28)
        if count>20:raise ValueError('Original body sphere count outside verified storage')
        spheres=[]
        for i in range(count):
            at=shape+0x30+32*i;x,y,z=read('3f',at);radius=read('f',at+16)[0];bone=u(at+20)
            if not all(math.isfinite(v) for v in (x,y,z,radius)) or radius<0:raise ValueError('Invalid original body sphere')
            spheres.append(dict(index=i,bone=bone,enabled=bool(mask&(1<<i)),source_center_cm=[x,y,z],native_center_m=[x/100,z/100,-y/100],radius_cm=radius))
        results.append(dict(rider=rider['address'],kind=rider['kind'],volume_address=hex(shape),mask=mask,spheres=spheres,
                            broad_center_cm=read('3f',shape+0x10),broad_radius_cm=read('f',shape+0x20)[0],query_flag874=u(base+0x874),
                            source_ground_normal=rider['source_contact_normal'],source_position_cm=rider['position_cm']))
        world=u(base+0x860)
        if not world:continue
        count=u(world+8)
        if count>64:raise ValueError('Original nearby instance count outside storage')
        for i in range(count):
            obj=u(world+12+i*4);resource=u(obj+0x78);track,rid=resource&255,resource>>8;descriptor=u(obj+0x88)
            if not descriptor:continue
            table=package['bindings'].get(str(track))
            if not table or rid>=table['instance_count']:continue
            authored=table['descriptors'][table['instance_descriptor_indices'][rid]]
            actual=read('3I',descriptor)
            expected=(authored['type'],authored['flags'],authored['resource08'])
            if actual!=expected:raise ValueError(f'Authored binding differs from original instance {resource:08x}: {actual} vs {expected}')
            checks.append(dict(rider=rider['address'],instance=hex(obj),resource=resource,descriptor=hex(descriptor),
                               source_offset=authored['source_offset'],type=actual[0],flags=actual[1],resource08=actual[2],collision_resource=authored['collision_resource'],
                               runtime_flags=u(obj+8)))
    return dict(state=str(state),ee_sha256=hashlib.sha256(memory).hexdigest(),rider_volumes=results,verified_instance_bindings=checks)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('states',type=Path,nargs='+')
    parser.add_argument('--package',type=Path,default=Path('local/assets/collision-reference/ARA1.json'));parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();package=json.loads(args.package.read_text());results=[probe(s,package) for s in args.states]
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(dict(cases=results),indent=2)+'\n')
    print(f"Verified {sum(len(r['verified_instance_bindings']) for r in results)} live authored bindings; exported {sum(len(r['rider_volumes']) for r in results)} rider volumes")

if __name__=='__main__':main()
