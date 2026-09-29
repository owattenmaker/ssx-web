#!/usr/bin/env python3
"""Replay generated Zoe body/board poses against owned airborne checkpoints.

No captured bones are used as native inputs. Secondary hair state is excluded
from the exact claim: its independent random/velocity-driven selector is pending.
"""
import json,struct,subprocess
from pathlib import Path
from compare_reference import prepare
from probe_rider_pose import extract
root=Path(__file__).resolve().parents[1]
source=root/'local/native-qa/reference-suite-stable-race-air'
out=root/'local/native-qa/air-pose-replay';out.mkdir(parents=True,exist_ok=True)
def bits(x):return struct.unpack('<I',struct.pack('<f',x))[0]
rows=[]
for case,frame in [('air-from-31-at-29',60),('air-from-31-at-59',90),('jump-90-at-31',31),('jump-90-at-60',60),('jump-90-at-90',90)]:
    spec=json.loads((source/f'{case}.scenario.json').read_text())
    accepted=spec.get('native',{}).get('accepted_input')
    spec=prepare(spec,Path(spec['baseline']))
    if accepted is not None:spec['native']['accepted_input']=accepted
    scenario=out/f'{case}.scenario.json';scenario.write_text(json.dumps(spec,indent=2)+'\n')
    telemetry=out/case;animation=out/f'{case}.animation.json'
    result=subprocess.run([str(root/'build/metal-engine/ssx3_scene_audit'),'--replay',str(scenario),'--telemetry',str(telemetry),'--animation-state',str(animation)],capture_output=True,text=True,check=True)
    (out/f'{case}.log').write_text(result.stdout+result.stderr)
    actual=json.loads(animation.read_text());expected=extract(root/f'local/reference/pcsx2/snow-jam-jump-{frame}.p2s')
    pose=actual['bones'];exact=[]
    for i,b in enumerate(pose[:24]):
        e=expected['bones'][i]
        exact.append([bits(x)for x in b['position_cm']]==e['world_position_bits'] and [bits(x)for x in b['rotation']]==e['world_rotation_bits'])
    nativeBody=[s for s in actual['sequences']if s['channel']==2]
    expectedBody=[s for s in expected['layers']if s['channel']==2]
    sequenceExact=len(nativeBody)==len(expectedBody)
    for a,b in zip(nativeBody,expectedBody):
        sequenceExact &= a['semantic']==b['semantic'] and a['flags']==b['sequence_flags']
        for x,y in [('rate','sequence_speed'),('weight','sequence_weight'),('fade_remaining','fade_remaining')]:sequenceExact &= bits(a[x])==bits(b[y])
        slot=a['slots'][0]
        sequenceExact &= slot['clip']==b['clip'] and slot['loop']==b['loop']
        for x,y in [('time','time'),('duration','duration'),('rate','speed'),('weight','slot_weight')]:sequenceExact &=bits(slot[x])==bits(b[y])
    record=json.loads(telemetry.with_suffix('.json').read_text())['records'][-1]
    row=dict(case=case,frame=frame,ee_sha256=expected['ee_sha256'],body_board_bones_exact=sum(exact),body_board_bones=24,body_sequences_exact=bool(sequenceExact),missing_pose=record['body_collision']['missing_pose'])
    rows.append(row);print(json.dumps(row))
report=dict(scope='24 original body/board transforms and main-channel sequence clocks/fades; secondary hair selector excluded.',states=rows)
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
assert all(r['body_board_bones_exact']==24 and r['body_sequences_exact'] and not r['missing_pose'] for r in rows),report
