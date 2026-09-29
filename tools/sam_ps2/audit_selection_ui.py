"""Resolve selection-screen hashes using the original game's hash algorithm."""
import json,base64,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];out=ROOT/'local/sam-ps2/roster'
def name_hash(s):
 h=0
 for c in s.encode('ascii'):
  h=((h<<4)+c)&0xffffffff;g=h&0xf0000000
  if g:h=h^(g>>23)^g
 return h
ui=json.loads((out/'frontend-ui.json').read_text());name='08sel_char';h=name_hash(name)
screens=[s for s in ui['ScreenTables'] if s['U0']==h];assert len(screens)==1
b=base64.b64decode(screens[0]['RefpackData']);(out/'character-select-screen.bin').write_bytes(b)
found={}
for name in ['08sel_char','names menu','Menu',*map(str,range(10)),':','macface','samface']:
 needle=struct.pack('<I',name_hash(name));positions=[i for i in range(0,len(b)-3,4) if b[i:i+4]==needle]
 found[name]={'hash':f'{name_hash(name):08x}','aligned_occurrences':positions}
report={'screen_hash':f'{h:08x}','decoded_size':len(b),'names':found,'note':'Hash matches are candidate references, not a complete widget parser.'}
(out/'selection-ui-map.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
