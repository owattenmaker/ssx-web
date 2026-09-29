#!/usr/bin/env python3
"""Compare native-generated neutral Zoe collision bodies with captured reference bodies.

The native factory receives rig/animation packet assets and sequence/contact/root
inputs only. Captured bone arrays are removed before writing the input fixture.
"""
import json,subprocess,zipfile
from pathlib import Path
from probe_rider_pose import extract
from obstacle_probe import probe
from reference_ground_profile import extract_ground

def main():
    root=Path(__file__).resolve().parents[1];folder=root/'local/reference/terrain/generated-body';folder.mkdir(parents=True,exist_ok=True)
    package=json.loads((root/'local/assets/native/ARA1/world_collision.json').read_text());cases=[]
    for name in ['glide','glide-120','turn-left-30','brake-30','jump-30']:
        state=root/f'local/reference/pcsx2/snow-jam-{name}.p2s';pose=extract(state);pose.pop('bones');pose['collision_enabled']=True
        with zipfile.ZipFile(state) as z:ground=extract_ground(z.read('eeMemory.bin'),0x14701a0)['state']
        reference=probe(state,package);body=next(b for b in reference['rider_volumes'] if b['kind']=='human')
        nearby=[b['resource'] for b in reference['verified_instance_bindings'] if b['rider']==body['rider']]
        cases.append(dict(name=name,pose_inputs=pose,ground_inputs=ground,reference_body=body,nearby_instances=nearby))
    fixture=folder/'inputs.json';fixture.write_text(json.dumps(cases,indent=2)+'\n')
    binary=root/'build/generated_body_collision';command=['clang++','-std=c++20','-O2','-ffp-contract=off','-frounding-math',str(root/'tests/generated_body_collision.mm'),str(root/'engine/animation_asset.mm'),str(root/'engine/rider_animation_player.mm'),str(root/'engine/world_collision_asset.mm')]
    command += [str(p) for p in (root/'build/metal-engine').glob('libssx3_*.a')]
    command += ['-framework','Foundation','-o',str(binary)];subprocess.run(command,check=True)
    subprocess.run([str(binary),str(root/'local/assets/native/RIDER_ZOE'),str(root/'local/assets/native/ARA1/world_collision.json'),str(fixture),str(folder/'results.json')],check=True)

if __name__=='__main__':main()
