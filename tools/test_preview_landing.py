#!/usr/bin/env python3
"""Private preview regression: cosmetic rider selection must retain landing physics."""
import json
import os
import subprocess
from pathlib import Path
from compare_reference import prepare

ROOT=Path(__file__).resolve().parents[1]

def main():
    output=ROOT/'local/native-qa/floor-fix';output.mkdir(parents=True,exist_ok=True)
    scenario=output/'jump.scenario.json'
    scenario.write_text(json.dumps(prepare(dict(frames=180,events=[dict(start=0,end=30,buttons=['Cross'])]),
        ROOT/'local/reference/pcsx2/snow-jam-glide.p2s'),indent=2)+'\n')
    baseline=None;results=[]
    for package in ('RIDER_ZOE','RIDER_MAC','RIDER_SAM'):
        prefix=output/package.lower()
        env={k:v for k,v in os.environ.items() if not k.startswith('SSX_')};env['SSX_RIDER_PACKAGE']=package
        subprocess.run([str(ROOT/'build/metal-engine/ssx3_scene_audit'),'--replay',str(scenario),'--telemetry',str(prefix)],
            env=env,check=True,stdout=subprocess.DEVNULL)
        rows=json.loads(prefix.with_suffix('.json').read_text())['records']
        takeoffs=[b['frame'] for a,b in zip(rows,rows[1:]) if a['grounded'] and not b['grounded']]
        landings=[b['frame'] for a,b in zip(rows,rows[1:]) if not a['grounded'] and b['grounded']]
        assert takeoffs==[31] and landings==[115] and rows[-1]['grounded'],(package,takeoffs,landings)
        assert all(not r['body_collision']['missing_pose'] for r in rows[1:]),f'Missing collision pose: {package}'
        physical=[(r['position'],r['velocity'],r['grounded']) for r in rows]
        if baseline is None:baseline=physical
        else:assert physical==baseline,f'Cosmetic selection changed physics: {package}'
        results.append(dict(package=package,takeoff_frame=31,landing_frame=115,grounded_at_end=True))
    (output/'verification.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Zoe, Mac and Sam jump and land; all181 movement frames identical.')

if __name__=='__main__':main()
