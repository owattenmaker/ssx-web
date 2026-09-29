#!/usr/bin/env python3
"""Add raw source weights to an existing original rider package without rebuilding art.

python3 tools/export_rider_skin_weights.py                 Zoe (historical default)
python3 tools/export_rider_skin_weights.py --rider psymon  a Snow Jam opponent package (rider_assets.NPC_PRESETS)
"""
import argparse,hashlib,json
from pathlib import Path
from compare_character_assets import big_members
from rider_assets import decode_high_model
ROOT=Path(__file__).resolve().parents[1]
def export(rider='zoe'):
 archive=ROOT/'local/gamecube/disc/files/data/char/mdlngc.big';models=dict(big_members(archive.read_bytes()))
 package=ROOT/f'local/assets/native/RIDER_{rider.upper()}/rider.json';rig=json.loads(package.read_text());keys={(b['file'],b['index']):i for i,b in enumerate(rig['bones'])};raw=[];normalized=[];totals={}
 prefix=rig.get('resource_prefix',rider)
 for part in rig['parts']:
  name=part.get('resource',f"{'board' if part['part'] in ('BindingsA','BoardFlexA') else prefix}_{part['part']}.mnf")
  data=models[name]
  if hashlib.sha256(data).hexdigest()!=part['source_sha256']:raise ValueError('Rider source model changed')
  model=decode_high_model(data)
  for group in model['skin']:
   raw.append([[keys[(w['file'],w['bone'])],w['source_weight']]for w in group]);normalized.append([[keys[(w['file'],w['bone'])],w['weight']]for w in group]);total=sum(w['source_weight']for w in group);totals[total]=totals.get(total,0)+1
 if normalized!=rig['skin']:raise ValueError('Raw source weights do not map to installed rider vertices')
 rig.update(source_skin=raw,source_skin_weight_units='integer-percent')
 # Zoe's browser package already exists; opponent packages are copied afterwards
 # by tools/export_opponent_packages.py.
 target=ROOT/f'web/public/assets/RIDER_{rider.upper()}/rider.json';web=json.loads(target.read_text())if target.exists()or rider=='zoe' else None
 if web is not None and (web['skin']!=normalized or web['parts']!=rig['parts']):raise ValueError('Browser rider differs from source package')
 package.write_text(json.dumps(rig,indent=2)+'\n')
 if web is not None:web.update(source_skin=raw,source_skin_weight_units='integer-percent');target.write_text(json.dumps(web,indent=2)+'\n')
 report=dict(vertices=len(raw),weight_totals=totals,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),note='Metadata only; existing normalized skinning is unchanged until source palette integration')
 (ROOT/'local/rider-lighting'/('skin-weight-audit.json' if rider=='zoe' else f'skin-weight-audit-{rider}.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
 return report
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
 parser.add_argument('--rider',default='zoe')
 export(parser.parse_args().rider)
