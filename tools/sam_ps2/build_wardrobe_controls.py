"""Diagnostic BOLT variants isolate new item rows from rules/default mappings.
These are original-rider regression controls, not playable Sam releases.
"""
from pathlib import Path
import struct,json
from extend_wardrobe import parse
ROOT=Path(__file__).resolve().parents[2]
folder=ROOT/'local/sam-ps2/roster/sam-assets'
old=parse((ROOT/'local/sam-ps2/original/BOLTPS2.DAT').read_bytes())
new=parse((folder/'BOLTPS2-preserved.DAT').read_bytes())
reports={}
for name,tables in [('items-only',old[2]),('items-rules',[new[2][0],*old[2][1:]]),('items-defaults',[old[2][0],*new[2][1:]])]:
 b=new[0]+struct.pack('<I',len(new[1]))+b''.join(new[1])
 for table in tables:b+=struct.pack('<I',len(table))+b''.join(table)
 b+=struct.pack('<I',len(new[3]))+new[3]+new[4]
 check=parse(b)
 assert check[1]==new[1] and check[2]==tables and check[3:]==new[3:]
 (folder/f'BOLTPS2-{name}-control.DAT').write_bytes(b)
 reports[name]={'items':len(new[1]),'table_counts':list(map(len,tables)),'bytes':len(b)}
(folder/'controls.json').write_text(json.dumps(reports,indent=2)+'\n')
print(reports)
