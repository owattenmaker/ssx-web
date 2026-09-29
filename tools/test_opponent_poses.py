#!/usr/bin/env python3
"""Validate original Snow Jam NPC rig/AFB initialization without bone injection."""
import argparse,json,subprocess,struct,zipfile
from pathlib import Path
from probe_rider_pose import extract,animation_inputs
from rider_assets import NPC_PRESETS
from reference_pair_collision import extract_pair_collision
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot',type=Path,default=ROOT/'local/reference/pcsx2/snow-jam-glide.p2s')
    parser.add_argument('--output',type=Path,default=ROOT/'local/native-qa/opponent-poses')
    parser.add_argument('--export-initialization',action='store_true',help='Write inputs-only animation-start.json to the five NPC packages')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);rows=[]
    volume_binary=ROOT/'build/ssx3_opponent_body_volume'
    subprocess.run(['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off',str(ROOT/'tests/opponent_body_volume.cpp'),'-o',str(volume_binary)],check=True)
    with zipfile.ZipFile(args.snapshot) as archive:pair=extract_pair_collision(archive.read('eeMemory.bin'))
    expected_bodies={int(p['rider'],0):p['body'] for p in pair['participants']}
    bits=lambda value:struct.unpack('<I',struct.pack('<f',value))[0]
    for name,preset in NPC_PRESETS.items():
        rig=ROOT/f'local/assets/native/RIDER_{name.upper()}';fixture=extract(args.snapshot,preset['actor'],rig_path=rig)
        path=args.output/f'{name}.json';path.write_text(json.dumps(fixture,indent=2)+'\n')
        result=subprocess.run([str(ROOT/'build/metal-engine/ssx3_animation_audit'),str(rig),str(path)],check=True,capture_output=True,text=True)
        (args.output/f'{name}.comparison.json').write_text(result.stdout);data=json.loads(result.stdout)
        row=dict(character=name,bones=len(data['bones']),local_exact=data['local_exact_bones'],world_position_exact=data['world_position_exact_bones'],max_world_position_error_cm=data['max_world_position_error_cm'],limited_bones=fixture['limited_bones'],ee_sha256=fixture['ee_sha256'])
        # Feed only native-generated world positions to the production builder.
        native_centers=[v for bone in data['bones'][:22] for v in bone['world_position']]
        volume_input=args.output/f'{name}.volume-input.bin';volume_input.write_bytes(struct.pack('<67f',fixture['scale'][0],*native_centers))
        native_body=json.loads(subprocess.check_output([str(volume_binary),str(volume_input)],text=True));expected=expected_bodies[preset['actor']]
        body_exact=native_body['count']==len(expected['spheres']) and native_body['active_mask']==expected['active_mask'] and bits(native_body['broad_radius_cm'])==bits(expected['broad_radius_cm'])
        body_exact &= len(native_body['spheres'])==len(expected['spheres']) and all(a['bone']==b['bone'] and bits(a['radius_cm'])==bits(b['radius_cm']) for a,b in zip(native_body['spheres'],expected['spheres']))
        row.update(body_shape_parameters_exact=body_exact,broad_radius_cm=native_body['broad_radius_cm'],body_spheres=native_body['count'])
        rows.append(row);print(json.dumps(row))
        if args.export_initialization:
            seed=animation_inputs(args.snapshot,preset['actor'],rig_path=rig)
            assert 'bones' not in seed and 'root_position_cm' not in seed
            (rig/'animation-start.json').write_text(json.dumps(seed,indent=2)+'\n')
    report=dict(source=str(args.snapshot),scope='All local transforms exact; post-frame world-pose residuals remain measured. Expected bones are test outputs only.',participants=rows)
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    assert all(r['local_exact']==r['bones'] and r['body_shape_parameters_exact']for r in rows),report
if __name__=='__main__':main()
