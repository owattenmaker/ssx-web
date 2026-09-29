#!/usr/bin/env python3
"""Read source environment filter inputs, never expected future colour samples."""
import struct,zipfile
from pathlib import Path

def extract(snapshot,slot=0):
 m=zipfile.ZipFile(snapshot).read('eeMemory.bin');f=lambda at:struct.unpack_from('<4f',m,at)
 return dict(ambient=f(0x4fa398+slot*0xf0),ratio=f(0x4fa3a8+slot*0xf0),force_next=bool(struct.unpack_from('<I',m,0x4a30f0+0xa68)[0]),multiplier=f(0x4fab00),air_ambient=f(0x4fab20),air_ratio=f(0x4fab40))
if __name__=='__main__':
 import argparse,json
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=Path);p.add_argument('--slot',type=int,default=0);a=p.parse_args();print(json.dumps(extract(a.snapshot,a.slot),indent=2))
