"""Publish only authored light records and the audited static event light index."""
import argparse,json
from pathlib import Path
location=(p:=argparse.ArgumentParser(description=__doc__),p.add_argument('--location',default='ARA1'),p.parse_args())[2].location
root=Path(__file__).resolve().parents[1];source=root/'local/assets/native'/location;target=root/'web/public/assets'/location
catalog=json.loads((source/'local-lights.json').read_text());tree=json.loads((source/'light-tree.json').read_text())
if catalog['version']!=1 or tree['version']!=1 or catalog['source_sha256']!=tree['source_sha256']:raise ValueError('Light assets disagree')
# Provenance/snapshot fixtures remain local audit artifacts. Runtime uses only
# source records and event topology; no rider pose or retained captured list.
for name,value in [('local-lights.json',dict(version=1,source_sha256=catalog['source_sha256'],lights=catalog['lights'])),('light-tree.json',{k:tree[k]for k in ['version','source_sha256','roots','nodes','light_count','excluded_resources']})]:
 (target/name).write_text(json.dumps(value,separators=(',',':'))+'\n')
print('Published',len(catalog['lights']),'authored lights and',len(tree['nodes']),'static index nodes')
