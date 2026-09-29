#!/usr/bin/env python3
"""Locate the captured pickup event prefix on the owned original disc."""
import json
from pathlib import Path
from inspect_disc import Disc
root=Path(__file__).resolve().parents[1]
event=json.loads((root/'local/browser-pickups/feedback-event.json').read_text())
needle=bytes.fromhex(event['entry_prefix'])[:8]
from disc_paths import ps2_iso;disc=Disc(ps2_iso());matches=[];tail=b''
try:
 for offset in range(0,disc.size,32*1024*1024):
  data=tail+disc.read(offset,min(32*1024*1024,disc.size-offset));at=0
  while (at:=data.find(needle,at))>=0:
   absolute=offset-len(tail)+at
   owners=[e['path'] for e in disc.entries if e.get('offset',-1)<=absolute<e.get('offset',-1)+e.get('size',0)]
   matches.append(dict(disc_offset=absolute,owners=owners));at+=1
  tail=data[-(len(needle)-1):]
finally:disc.close()
out=root/'local/browser-pickups/feedback-disc-locations.json';out.write_text(json.dumps(dict(prefix=needle.hex(),scope='Exact8-byte prefix search; matching records still require full validation',matches=matches),indent=2)+'\n');print(matches)
