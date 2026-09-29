#!/usr/bin/env python3
"""Compare native Zoe local/world poses against ground and airborne owned PCSX2 checkpoints."""
import json,subprocess
from pathlib import Path
from probe_rider_pose import extract
root=Path(__file__).resolve().parents[1];output=root/'local/native-qa';rows=[]
for name in ('glide','glide-1','glide-30-a','glide-120','turn-left-30','brake-30','jump-30','jump-31','jump-60','jump-90','air-isolated-60','air-isolated-120','charge-long-240'):
    source=root/f'local/reference/pcsx2/snow-jam-{name}.p2s';fixture=extract(source)
    manifest=json.loads((root/'local/assets/native/RIDER_ZOE/animation-packets.json').read_text());durations={c['id']:c['duration']for c in manifest['clips']}
    for layer in fixture['layers']:assert durations[layer['clip']]==layer['duration'],(name,layer['clip'],durations[layer['clip']],layer['duration'])
    path=output/f'zoe-{name}-pose.json';path.write_text(json.dumps(fixture,indent=2)+'\n')
    result=subprocess.run([str(root/'build/metal-engine/ssx3_animation_audit'),str(root/'local/assets/native/RIDER_ZOE'),str(path)],check=True,capture_output=True,text=True)
    compared=json.loads(result.stdout);(output/f'zoe-{name}-pose-comparison.json').write_text(result.stdout)
    row=dict(state=name,ee_sha256=fixture['ee_sha256'],local_bones_exact=compared['local_exact_bones'],bone_count=len(compared['bones']),max_local_rotation_error=compared['max_local_rotation_error'],max_world_position_error_cm=compared['max_world_position_error_cm'],world_position_exact_bones=compared['world_position_exact_bones'],world_rotation_exact_bones=compared['world_rotation_exact_bones'])
    row['collision_centers']=[dict(bone=i,error_cm=compared['bones'][i]['position_error_cm'])for i in (1,5,7,9,12,14,16,17,19,20)]
    rows.append(row);print(name,'local',row['local_bones_exact'],'/',row['bone_count'],'world maximum cm',row['max_world_position_error_cm'])
report=dict(scope='Exact local transforms; world-pose errors remain measured, not accepted as perfect fidelity.',all_local_transforms_exact=all(r['local_bones_exact']==r['bone_count'] for r in rows),states=rows)
(output/'zoe-pose-suite.json').write_text(json.dumps(report,indent=2)+'\n')
if not report['all_local_transforms_exact']:raise SystemExit(1)

for row in rows:
    if row['state'] in ('jump-31','jump-60','jump-90','air-isolated-60','air-isolated-120'):
        assert row['world_position_exact_bones']==row['world_rotation_exact_bones']==row['bone_count'],row
