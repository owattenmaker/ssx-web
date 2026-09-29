#!/usr/bin/env python3
"""Run collected Snow Jam checkpoints against the current native engine.

Requires private original-game snapshots. Does not start or control PCSX2.
Results measure discrepancies; this is not a claim of complete game conformance.
"""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
from compare_reference import prepare, compare
from canonical_replay import canonical

ROOT=Path(__file__).resolve().parents[1]
CASES={
    'glide-30': [('glide-1',1),('glide-30-a',30)],
    'glide-120': [('glide-120',120)],
    'turn-left-30': [('turn-left-30',30)],
    'brake-30': [('brake-30',30)],
    'jump-90': [('jump-31',31),('jump-60',60),('jump-90',90)],
    'air-from-31': [('jump-60',29),('jump-90',59)],
    'air-isolated-120': [('air-isolated-60',60),('air-isolated-120',120)],
    'air-capped-1': [('air-capped-1',1)],
    'air-spin-30': [('air-spin-30',30)],
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures',type=Path,default=ROOT/'local/reference/pcsx2')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--binary',type=Path,default=ROOT/'build/metal-engine/ssx3_scene_audit')
    parser.add_argument('--accepted',action='store_true',help='Use each checkpoint\'s independently verified game-accepted commands')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    # Fingerprint all native collision inputs, not just metadata naming them.
    assets=[ROOT/'local/assets/native'/name for name in ('ARA1','RIDER_ZOE')]
    files=[args.binary]+[p for folder in assets for p in folder.rglob('*') if p.is_file()]
    hashes={str(p.resolve()):digest(p) for p in files}
    rows=[];reports={}
    environment={k:v for k,v in os.environ.items() if not k.startswith('SSX_')}
    environment['SSX_RIDER_PACKAGE']='RIDER_ZOE'
    jobs=[]
    for name,checkpoints in CASES.items():
        raw=json.loads((args.fixtures/(name+'.json')).read_text())
        baseline=Path(raw['baseline'])
        if args.accepted:
            for state,frame in checkpoints:
                path=args.fixtures/('snow-jam-'+state+'.p2s')
                spec=canonical(raw,path)
                if spec['frames']!=frame:raise ValueError(f'Accepted logic tick count differs from checkpoint label: {name} frame{frame}')
                jobs.append((name+'-at-'+str(frame),baseline,spec,[(frame,path)]))
        else:
            jobs.append((name,baseline,prepare(raw,baseline),[(frame,args.fixtures/('snow-jam-'+state+'.p2s')) for state,frame in checkpoints]))
    for name,baseline,spec,checkpoints in jobs:
        scenario=args.output/(name+'.scenario.json');scenario.write_text(json.dumps(spec,indent=2)+'\n')
        prefix=args.output/name
        subprocess.run([str(args.binary),'--replay',str(scenario),'--telemetry',str(prefix)],check=True,stdout=subprocess.DEVNULL,env=environment)
        telemetry=json.loads(prefix.with_suffix('.json').read_text())
        points=[(0,baseline)]+checkpoints
        report=compare(spec,telemetry,points);reports[name]=report
        for sample in report['samples'][1:]:
            rows.append(f"| {name} | {sample['frame']} | {sample['position_error_norm_m']:.9g} | {sample['velocity_error_norm_mps']:.9g} | {sample['original']['motion_mode']} |")
    changed=[]
    for path,before in hashes.items():
        after=digest(Path(path)) if Path(path).is_file() else None
        if before!=after:changed.append(dict(path=path,before=before,after=after))
    if changed:
        (args.output/'rejected.json').write_text(json.dumps(dict(status='rejected',reason='Binary/assets changed during execution',changed_files=changed,cases=reports),indent=2)+'\n')
        raise RuntimeError('Native binary/assets changed during suite: '+', '.join(item['path'] for item in changed))
    (args.output/'comparison.json').write_text(json.dumps(dict(fingerprints=hashes,rider_package='RIDER_ZOE',environment_policy='Inherited SSX overrides removed; reference Zoe selected explicitly',cases=reports),indent=2)+'\n')
    markdown=('Measured native/original discrepancies using independently verified game-accepted commands for each checkpoint.\n\n' if args.accepted else
              'Measured native/original discrepancies using requested emulator pad timing. The game-accepted command timeline can differ.\n\n')
    markdown+='| Case | Frame | Position error (m) | Velocity error (m/s) | Original mode |\n|---|---:|---:|---:|---:|\n'+'\n'.join(rows)+'\n'
    (args.output/'comparison.md').write_text(markdown)
    print(markdown)


if __name__=='__main__':main()
